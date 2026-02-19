/*
Copyright NetFoundry Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "ziti-nodejs.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 1025
#endif

struct conn_data {
    ziti_connection conn;
    napi_threadsafe_function on_connect;
    int status;
    uv_os_sock_t sock;
};

static void on_js_connect(napi_env env, napi_value js_cb, void *ctx, void *data) {
    struct conn_data *cd = data;
    napi_release_threadsafe_function(cd->on_connect, napi_tsfn_release);
    NAPI_GLOBAL(env, glob);
    if (cd->status != ZITI_OK) {
        napi_value msg, err;
        napi_create_string_utf8(env, ziti_errorstr(cd->status), NAPI_AUTO_LENGTH, &msg);
        napi_create_error(env, NULL, msg, &err);
        napi_call_function(env, glob, js_cb, 1, &err, NULL);
    } else {
        napi_value result[2];
        napi_get_undefined(env, &result[0]);
        napi_create_int64(env, (int64_t)cd->sock, &result[1]);
        napi_call_function(env, glob, js_cb, 2, result, NULL);
    }
}

static void on_z_connect(ziti_connection conn, int status) {
    struct conn_data *cd = ziti_conn_data(conn);
    assert(cd);
    assert(cd->conn == conn);
    cd->status = status;
    if (status != ZITI_OK) {
        ziti_close(conn, NULL);
        ZITI_NODEJS_LOG(ERROR, "failed to connect: %d/%s", status, ziti_errorstr(status));
    } else {
        uv_os_sock_t pipe[2] = { -1, -1 };
        int uv_rc = uv_socketpair(AF_UNIX, 0, pipe, UV_NONBLOCK_PIPE, UV_NONBLOCK_PIPE);
        if (uv_rc != 0) {
            ZITI_NODEJS_LOG(ERROR, "failed to create socket pair: %d/%s", uv_rc, uv_strerror(uv_rc));
            ziti_close(conn, NULL);
            cd->status = uv_rc;
            return;
        }
        ziti_conn_bridge_fds(conn, pipe[1], pipe[1], NULL, NULL);
        cd->sock = pipe[0];
    }
    napi_call_threadsafe_function(cd->on_connect, cd, napi_tsfn_blocking);
}

static napi_value z_connect(napi_env env, napi_callback_info info) {

    size_t argc = 4;
    napi_value args[4] = {};
    NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 2) {
        napi_throw_error(env, NULL, "too few arguments, signature => (service, [terminator,] callback)");
    }
    napi_valuetype cb_type;
    napi_typeof(env, args[argc - 1], &cb_type);
    if (cb_type != napi_function) {
        napi_throw_error(env, NULL, "last argument must be a callback function");
    }
    struct conn_data *cd = calloc(1, sizeof(struct conn_data));
    cd->sock = -1;

    NAPI_LITERAL(env, tsfn_name, "N-API on_connect");
    napi_create_threadsafe_function(env, args[argc - 1], NULL, tsfn_name,
                                    0, 1, NULL, NULL, NULL, on_js_connect,
                                    &cd->on_connect);

    char *service_name = NULL;
    char *terminator = NULL;
    char *dial_data = NULL;

    size_t service_name_len = 0;
    NAPI_CHECK(env, "get service name", napi_get_value_string_utf8(env, args[0], NULL, 0, &service_name_len));
    service_name = malloc(service_name_len + 1);
    NAPI_CHECK(env, "get service name", napi_get_value_string_utf8(env, args[0], service_name, service_name_len + 1, &service_name_len));
    if(argc > 2) {
        napi_valuetype arg_type;
        napi_typeof(env, args[1], &arg_type);
        if (arg_type == napi_string) {
            size_t len = 0;
            NAPI_CHECK(env, "get terminator", napi_get_value_string_utf8(env, args[1], NULL, 0, &len));
            terminator = malloc(len + 1);
            NAPI_CHECK(env, "get terminator", napi_get_value_string_utf8(env, args[1], terminator, len + 1, &len));
        }
    }

    if (argc > 3) {
        napi_valuetype arg_type;
        napi_typeof(env, args[2], &arg_type);
        if (arg_type == napi_string) {
            size_t len = 0;
            NAPI_CHECK(env, "get dial data", napi_get_value_string_utf8(env, args[2], NULL, 0, &len));
            dial_data = malloc(len + 1);
            NAPI_CHECK(env, "get dial data", napi_get_value_string_utf8(env, args[2], dial_data, len + 1, &len));
        }
    }

    int rc = ziti_conn_init(ztx, &cd->conn, cd);
    if (rc == ZITI_OK) {
        ziti_dial_opts opts = {
                .identity = terminator,
                .stream = true,
                .app_data = dial_data,
                .app_data_sz = dial_data ? strlen(dial_data) : 0,
        };
        rc = ziti_dial_with_options(cd->conn, service_name, &opts, on_z_connect, NULL);
    }

    free(dial_data);
    free(service_name);
    free(terminator);

    if (rc != ZITI_OK) {
        if (cd->conn) {
            ziti_close(cd->conn, NULL);
        }
        free(cd);
        napi_throw_error(env, NULL, ziti_errorstr(rc));
    }
    NAPI_UNDEFINED(env, undefined);
    return undefined;
}

static napi_value z_get_service_for_addr(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3] = {};
    NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    char proto[6] = {};
    size_t proto_len = 0;
    NAPI_CHECK(env, "get protocol", napi_get_value_string_utf8(env, args[0], proto, sizeof(proto), &proto_len));

    char host[MAXHOSTNAMELEN] = {};
    size_t host_len = 0;
    NAPI_CHECK(env, "get address", napi_get_value_string_utf8(env, args[1], host, sizeof(host), &host_len));

    int port = 0;
    NAPI_CHECK(env, "get port", napi_get_value_int32(env, args[2], &port));

    ziti_protocol zitiProtocol = ziti_protocols.value_of(proto);

    ziti_dial_opts dialOpts;
    const ziti_service *srv = ziti_dial_opts_for_addr(&dialOpts, ztx, zitiProtocol, host, port, NULL, 0);
    if (srv == NULL) {
        napi_throw_error(env, NULL, ziti_errorstr(ZITI_SERVICE_UNAVAILABLE));
        return NULL;
    }

    napi_value result;
    napi_create_object(env, &result);

    napi_value serviceName;
    napi_value identity;
    napi_value data;
    NAPI_CHECK(env, "create service name", napi_create_string_utf8(env, srv->name, NAPI_AUTO_LENGTH, &serviceName));
    napi_set_named_property(env, result, "service", serviceName);
    NAPI_CHECK(env, "create identity", napi_create_string_utf8(env, dialOpts.identity, NAPI_AUTO_LENGTH, &identity));
    napi_set_named_property(env, result, "identity", identity);
    NAPI_CHECK(env, "create data", napi_create_string_utf8(env, dialOpts.app_data, dialOpts.app_data_sz, &data));
    napi_set_named_property(env, result, "data", data);

    free(dialOpts.app_data);
    free((void*)dialOpts.identity);

    return result;
}

ZNODE_EXPOSE(get_ziti_service, z_get_service_for_addr)
ZNODE_EXPOSE(ziti_connect, z_connect)
