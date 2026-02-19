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

ziti_context ztx = NULL;

typedef struct {
  napi_threadsafe_function tsfn;
  int zitiContextEventStatus;
} AddonData;


static const char *ALL_CONFIG_TYPES[] = {
        "all",
        NULL
};

#define ZROK_PROXY_CFG_V1 "zrok.proxy.v1"
#define ZROK_PROXY_CFG_V1_MODEL(XX, ...) \
XX(auth_scheme, model_string, none, auth_scheme, __VA_ARGS__) \
XX(basic_auth, model_string, none, basic_auth, __VA_ARGS__) \
XX(oauth, model_string, none, oauth, __VA_ARGS__)
DECLARE_MODEL(zrok_proxy_cfg_v1, ZROK_PROXY_CFG_V1_MODEL)

/**
 * This function is responsible for calling the JavaScript callback function 
 * that was specified when the ziti_init(...) was called from JavaScript.
 */
static void CallJs(napi_env env, napi_value js_cb, void* context, void* data) {
  // This parameter is not used.
  (void) context;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {
      NAPI_UNDEFINED(env, undefined);
      // Retrieve the rc created by the worker thread.
      int64_t rc = (int64_t)data;
      if (rc == ZITI_OK) {
          NAPI_CHECK(env, "calling js callback(success)",
                     napi_call_function(env, undefined, js_cb, 1, &undefined, NULL));
      } else {
          napi_value result;
          napi_value err_msg;
          if (rc == ZITI_EXTERNAL_LOGIN_REQUIRED) {
              NAPI_CHECK(env, "create error string",
                         napi_create_string_utf8(env, "additional authentication is not supported",
                                                 NAPI_AUTO_LENGTH, &err_msg));
          } else {
              NAPI_CHECK(env, "create error string",
                         napi_create_string_utf8(env, ziti_errorstr((int) rc), NAPI_AUTO_LENGTH, &err_msg));
          }
          NAPI_CHECK(env, "create error", napi_create_error(env, NULL, err_msg, &result));
          NAPI_CHECK(env, "calling js callback(error)",
                     napi_call_function(env, undefined, js_cb, 1, &result, NULL));
      }
  }
}

static void complete_init(AddonData *addon_data, int rc) {
    if (addon_data && addon_data->tsfn) {
        napi_call_threadsafe_function(addon_data->tsfn, (void *) (long) rc, napi_tsfn_blocking);
        napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release);
        addon_data->tsfn = NULL;
    }
}

/**
 * 
 */
static void on_ziti_event(ziti_context _ztx, const ziti_event_t *event) {
  napi_status nstatus;

  AddonData* addon_data = (AddonData*)ziti_app_ctx(_ztx);

  switch (event->type) {

      case ZitiContextEvent:
          if (event->ctx.ctrl_status == ZITI_OK) {
              const ziti_version *ctrl_ver = ziti_get_controller_version(_ztx);
              const ziti_identity *proxy_id = ziti_get_identity(_ztx);
              ZITI_NODEJS_LOG(INFO, "controller version = %s(%s)[%s]", ctrl_ver->version, ctrl_ver->revision,
                              ctrl_ver->build_date);
              if (proxy_id)
                  ZITI_NODEJS_LOG(INFO, "identity = <%s>[%s]@%s", proxy_id->name, proxy_id->id,
                                  ziti_get_controller(_ztx));
          } else {
              if (event->ctx.ctrl_status != ZITI_DISABLED) {
                  ZITI_NODEJS_LOG(ERROR, "Failed to connect to controller: %s", event->ctx.err);
              }
              complete_init(addon_data, event->ctx.ctrl_status);
          }

          addon_data->zitiContextEventStatus = event->ctx.ctrl_status;

          break;

      case ZitiServiceEvent:
          if (event->service.removed != NULL) {
              for (ziti_service **sp = event->service.removed; *sp != NULL; sp++) {
                  // service_check_cb(ztx, *sp, ZITI_SERVICE_UNAVAILABLE, app_ctx);
                  ZITI_NODEJS_LOG(INFO, "Service removed [%s]", (*sp)->name);
              }
          }

          if (event->service.added != NULL) {
              for (ziti_service **sp = event->service.added; *sp != NULL; sp++) {
                  // service_check_cb(ztx, *sp, ZITI_OK, app_ctx);
                  ZITI_NODEJS_LOG(INFO, "Service added [%s]", (*sp)->name);
              }
          }

          if (event->service.changed != NULL) {
              for (ziti_service **sp = event->service.changed; *sp != NULL; sp++) {
                  // service_check_cb(ztx, *sp, ZITI_OK, app_ctx);
                  ZITI_NODEJS_LOG(INFO, "Service changed [%s]", (*sp)->name);
              }
          }

          //
          for (int i = 0; event->service.added && event->service.added[i] != NULL; i++) {

              ziti_service *s = event->service.added[i];
              ziti_intercept_cfg_v1 *intercept = alloc_ziti_intercept_cfg_v1();
              ziti_client_cfg_v1 clt_cfg = {
                      .hostname = {0},
                      .port = 0
              };
              zrok_proxy_cfg_v1 zrok_cfg = {
                      .auth_scheme = "",
                      .basic_auth = "",
                      .oauth = ""
              };

              if (ziti_service_get_config(s, ZITI_INTERCEPT_CFG_V1, intercept,
                                          (int (*)(void *, const char *, size_t)) parse_ziti_intercept_cfg_v1) ==
                  ZITI_OK) {

                  const ziti_address *range_addr;
                  MODEL_LIST_FOREACH(range_addr, intercept->addresses) {
                      ziti_port_range *p;
                      MODEL_LIST_FOREACH(p, intercept->port_ranges) {
                          int lowport = p->low;
                          while (lowport <= p->high) {
                              track_service_to_hostname(s->name, (char *) range_addr->addr.hostname, lowport);
                              lowport++;
                          }
                      }
                  }

              } else if (ziti_service_get_config(s, ZITI_CLIENT_CFG_V1, &clt_cfg, (int (*)(void *, const char *,
                                                                                           unsigned long)) parse_ziti_client_cfg_v1) ==
                         ZITI_OK) {
                  track_service_to_hostname(s->name, clt_cfg.hostname.addr.hostname, clt_cfg.port);

              } else if (ziti_service_get_config(s, ZROK_PROXY_CFG_V1, &zrok_cfg, (int (*)(void *, const char *,
                                                                                           unsigned long)) parse_ziti_client_cfg_v1) ==
                         ZITI_OK) {
                  track_service_to_hostname(s->name, s->name, 80);
              }

              free_ziti_intercept_cfg_v1(intercept);
              free(intercept);
          }

          for (int i = 0; event->service.changed && event->service.changed[i] != NULL; i++) {

              ziti_service *s = event->service.changed[i];
              ziti_intercept_cfg_v1 *intercept = alloc_ziti_intercept_cfg_v1();
              ziti_client_cfg_v1 clt_cfg = {
                      .hostname = {0},
                      .port = 0
              };

              if (ziti_service_get_config(s, ZITI_INTERCEPT_CFG_V1, intercept,
                                          (int (*)(void *, const char *, size_t)) parse_ziti_intercept_cfg_v1) ==
                  ZITI_OK) {

                  const ziti_address *range_addr;
                  MODEL_LIST_FOREACH(range_addr, intercept->addresses) {
                      ziti_port_range *p;
                      MODEL_LIST_FOREACH(p, intercept->port_ranges) {
                          int lowport = p->low;
                          while (lowport <= p->high) {
                              track_service_to_hostname(s->name, (char *) range_addr->addr.hostname, lowport);
                              lowport++;
                          }
                      }
                  }

              } else if (ziti_service_get_config(s, ZITI_CLIENT_CFG_V1, &clt_cfg, (int (*)(void *, const char *,
                                                                                           unsigned long)) parse_ziti_client_cfg_v1) ==
                         ZITI_OK) {
                  track_service_to_hostname(s->name, clt_cfg.hostname.addr.hostname, clt_cfg.port);
              }

              free_ziti_intercept_cfg_v1(intercept);
              free(intercept);
          }

          // Initiate the call into the JavaScript 'on_init' callback, now that we know about all the services
          ZITI_NODEJS_LOG(DEBUG, "init callback = %p", addon_data->tsfn);
          complete_init(addon_data, addon_data->zitiContextEventStatus);
          break;

      case ZitiAuthEvent: {
          int status = ZITI_EXTERNAL_LOGIN_REQUIRED;
          switch (event->auth.action) {
              case ziti_auth_login_external:
                  ZITI_NODEJS_LOG(INFO, "external auth required: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
              case ziti_auth_select_external:
                  ZITI_NODEJS_LOG(INFO, "select external auth provider: type=%s", event->auth.type);
                  break;
              case ziti_auth_cannot_continue:
                  status = ZITI_AUTHENTICATION_FAILED;
                  ZITI_NODEJS_LOG(ERROR, "cannot continue authentication: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
              case ziti_auth_prompt_totp:
                  ZITI_NODEJS_LOG(INFO, "external auth required: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
              case ziti_auth_prompt_pin:
                  ZITI_NODEJS_LOG(INFO, "external auth required: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
          }
          complete_init(addon_data, status);
          break;
      }
      case ZitiRouterEvent: {
          switch (event->router.status) {
              case EdgeRouterConnected:
                  ZITI_NODEJS_LOG(INFO, "ziti connected to edge router %s\nversion = %s", event->router.name,
                                  event->router.version);
                  break;
              case EdgeRouterDisconnected:
                  ZITI_NODEJS_LOG(INFO, "ziti disconnected from edge router %s", event->router.name);
                  break;
              case EdgeRouterRemoved:
                  ZITI_NODEJS_LOG(INFO, "ziti removed edge router %s", event->router.name);
                  break;
              case EdgeRouterUnavailable:
                  ZITI_NODEJS_LOG(INFO, "edge router %s is not available", event->router.name);
                  break;
              case EdgeRouterAdded:
                  ZITI_NODEJS_LOG(INFO, "edge router %s was added", event->router.name);
                  break;
          }
          break;
      }
  }
}


/**
 * 
 */
static napi_value load_ztx(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value jsRetval;

    ZITI_NODEJS_LOG(DEBUG, "initializing");

    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        napi_throw_error(env, "EINVAL", "Too few arguments");
        return NULL;
    }

    // Obtain length of location of identity file
    size_t result;
    size_t config_path_len;
    status = napi_get_value_string_utf8(env, args[0], NULL, 0, &config_path_len);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "location of identity file is not a string");
    }
    // Obtain length of location of identity file
    char *config_file_name = calloc(1, config_path_len + 2);
    status = napi_get_value_string_utf8(env, args[0], config_file_name, config_path_len + 1, &result);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to obtain location of identity file");
    }
    ZITI_NODEJS_LOG(DEBUG, "config_file_name: %s", config_file_name);

    ziti_config cfg = {0};
    int rc = ziti_load_config(&cfg, config_file_name);
    ZITI_NODEJS_LOG(DEBUG, "ziti_load_config => %d", rc);
    if (rc != ZITI_OK) {
        napi_throw_error(env, "EINVAL", ziti_errorstr(rc));
        return NULL;
    }

    rc = ziti_context_init(&ztx, &cfg);
    ZITI_NODEJS_LOG(DEBUG, "ziti_context_init => %d", rc);
    if (rc != ZITI_OK) {
        napi_throw_error(env, "EINVAL", ziti_errorstr(rc));
    }

    // Obtain ptr to JS callback function
    napi_value js_cb = args[1];
    AddonData *addon_data = calloc(1, sizeof(AddonData));

    // Create a string to describe this asynchronous operation.
    NAPI_LITERAL(env, work_name, "N-API on_ziti_init");

    // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn)
    // which we can call from a worker thread.
    NAPI_CHECK(env, "create init callback", napi_create_threadsafe_function(
            env,
            js_cb,
            NULL,
            work_name,
            0,
            1,
            NULL,
            NULL,
            NULL,
            CallJs,
            &(addon_data->tsfn)));

    ziti_options opts = {
            .events = ZitiContextEvent | ZitiServiceEvent | ZitiRouterEvent | ZitiAuthEvent,
            .event_cb = on_ziti_event,
            .refresh_interval = 60,
            .app_ctx = addon_data,
            .config_types = ALL_CONFIG_TYPES,
            .metrics_type = INSTANT,
    };
    rc = ziti_context_set_options(ztx, &opts);
    ZITI_NODEJS_LOG(DEBUG, "ziti_context_set_options => %d", rc);
    if (rc != ZITI_OK) goto done;

    rc = ziti_context_run(ztx, thread_loop);
    ZITI_NODEJS_LOG(DEBUG, "ziti_context_run => %d", rc);

    done:
    if (rc != ZITI_OK) {
        if (addon_data && addon_data->tsfn) {
            napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release);
        }
        free(addon_data);
        napi_throw_error(env, "EINVAL", ziti_errorstr(rc));
        if (ztx) {
            ziti_shutdown(ztx);
            ztx = NULL;
        }
    }

    NAPI_CHECK(env, "create return value", napi_create_int32(env, rc, &jsRetval));
    return jsRetval;
}

static napi_value ztx_shutdown(napi_env env, napi_callback_info info) {
    ZITI_NODEJS_LOG(DEBUG, "ztx: %p", ztx);
    NAPI_UNDEFINED(env, jsRetval);
    if (ztx) {
        AddonData *addon_data = (AddonData*)ziti_app_ctx(ztx);
        if (addon_data && addon_data->tsfn) {
            napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release);
            free(addon_data);
        }
        ziti_shutdown(ztx);
    }
    return jsRetval;
}


ZNODE_EXPOSE(ziti_init, load_ztx)

ZNODE_EXPOSE(ziti_shutdown, ztx_shutdown)
