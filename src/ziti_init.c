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
  napi_async_work work;
  napi_threadsafe_function tsfn;
  int zitiContextEventStatus;
  bool on_init_cb_invoked;
} AddonData;


static const char *ALL_CONFIG_TYPES[] = {
        "all",
        NULL
};

#define ZROK_PROXY_CFG_V1 "zrok.proxy.v1"
#define ZROK_PROXY_CFG_V1_MODEL(XX, ...) \
XX(auth_scheme, string, none, auth_scheme, __VA_ARGS__) \
XX(basic_auth, string, none, basic_auth, __VA_ARGS__) \
XX(oauth, string, none, oauth, __VA_ARGS__)
DECLARE_MODEL(zrok_proxy_cfg_v1, ZROK_PROXY_CFG_V1_MODEL)

/**
 * This function is responsible for calling the JavaScript callback function 
 * that was specified when the ziti_init(...) was called from JavaScript.
 */
static void CallJs(napi_env env, napi_value js_cb, void* context, void* data) {
    napi_status status;

  // This parameter is not used.
  (void) context;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {
    napi_value undefined, js_rc;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);

    // Retrieve the rc created by the worker thread.
    int64_t rc = (int64_t)data;
    status = napi_create_int64(env, (int64_t)rc, &js_rc);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to napi_create_int64");
    }

    // Call the JavaScript function and pass it the rc
    status = napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_rc,
        NULL
      );
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to napi_call_function");
    }
  }
}


/**
 * 
 */
void on_ziti_init(ziti_context _ztx, int status, void* ctx) {
  napi_status nstatus;

  ZITI_NODEJS_LOG(DEBUG, "_ztx: %p", _ztx);

  // Set the global ztx context variable
  ztx = _ztx;

  AddonData* addon_data = (AddonData*)ctx;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  nstatus = napi_call_threadsafe_function(
      addon_data->tsfn,
      (void*) (long) status,
      napi_tsfn_blocking);
    if (nstatus != napi_ok) {
      ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
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
      if (event->event.ctx.ctrl_status == ZITI_OK) {
        const ziti_version *ctrl_ver = ziti_get_controller_version(_ztx);
        const ziti_identity *proxy_id = ziti_get_identity(_ztx);
        ZITI_NODEJS_LOG(INFO, "controller version = %s(%s)[%s]", ctrl_ver->version, ctrl_ver->revision, ctrl_ver->build_date);
        ZITI_NODEJS_LOG(INFO, "identity = <%s>[%s]@%s", proxy_id->name, proxy_id->id, ziti_get_controller(_ztx));
      }
      else {
          ZITI_NODEJS_LOG(ERROR, "Failed to connect to controller: %s", event->event.ctx.err);
      }

      addon_data->zitiContextEventStatus = event->event.ctx.ctrl_status;

      break;

    case ZitiServiceEvent:
      if (event->event.service.removed != NULL) {
          for (ziti_service **sp = event->event.service.removed; *sp != NULL; sp++) {
              // service_check_cb(ztx, *sp, ZITI_SERVICE_UNAVAILABLE, app_ctx);
              ZITI_NODEJS_LOG(INFO, "Service removed [%s]", (*sp)->name);
          }
      }

      if (event->event.service.added != NULL) {
          for (ziti_service **sp = event->event.service.added; *sp != NULL; sp++) {
              // service_check_cb(ztx, *sp, ZITI_OK, app_ctx);
              ZITI_NODEJS_LOG(INFO, "Service added [%s]", (*sp)->name);
          }
      }

      if (event->event.service.changed != NULL) {
          for (ziti_service **sp = event->event.service.changed; *sp != NULL; sp++) {
              // service_check_cb(ztx, *sp, ZITI_OK, app_ctx);
              ZITI_NODEJS_LOG(INFO, "Service changed [%s]", (*sp)->name);
          }
      }

      //
      for (int i = 0; event->event.service.added && event->event.service.added[i] != NULL; i++) {

        ziti_service *s = event->event.service.added[i];
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

        if (ziti_service_get_config(s, ZITI_INTERCEPT_CFG_V1, intercept, (int (*)(void *, const char *, size_t))parse_ziti_intercept_cfg_v1) == ZITI_OK) {

          const ziti_address *range_addr;
          MODEL_LIST_FOREACH(range_addr, intercept->addresses) {
            ziti_port_range *p;
            MODEL_LIST_FOREACH(p, intercept->port_ranges) {
                int lowport = p->low;
                while (lowport <= p->high) {
                  track_service_to_hostname(s->name, (char *)range_addr->addr.hostname, lowport);
                  lowport++;
                }
            }
          }

        } else if (ziti_service_get_config(s, ZITI_CLIENT_CFG_V1, &clt_cfg, (int (*)(void *, const char *, unsigned long))parse_ziti_client_cfg_v1) == ZITI_OK) {
            track_service_to_hostname(s->name, clt_cfg.hostname.addr.hostname, clt_cfg.port);

        } else if (ziti_service_get_config(s, ZROK_PROXY_CFG_V1, &zrok_cfg, (int (*)(void *, const char *, unsigned long))parse_ziti_client_cfg_v1) == ZITI_OK) {
            track_service_to_hostname(s->name, s->name, 80);
        }

        free_ziti_intercept_cfg_v1(intercept);
        free(intercept);
      }

      for (int i = 0; event->event.service.changed && event->event.service.changed[i] != NULL; i++) {

        ziti_service *s = event->event.service.changed[i];
        ziti_intercept_cfg_v1 *intercept = alloc_ziti_intercept_cfg_v1();
        ziti_client_cfg_v1 clt_cfg = {
          .hostname = {0},
          .port = 0
        };

        if (ziti_service_get_config(s, ZITI_INTERCEPT_CFG_V1, intercept, (int (*)(void *, const char *, size_t))parse_ziti_intercept_cfg_v1) == ZITI_OK) {

          const ziti_address *range_addr;
          MODEL_LIST_FOREACH(range_addr, intercept->addresses) {
            ziti_port_range *p;
            MODEL_LIST_FOREACH(p, intercept->port_ranges) {
                int lowport = p->low;
                while (lowport <= p->high) {
                  track_service_to_hostname(s->name, (char *)range_addr->addr.hostname, lowport);
                  lowport++;
                }
            }
          }

        } else if (ziti_service_get_config(s, ZITI_CLIENT_CFG_V1, &clt_cfg, (int (*)(void *, const char *, unsigned long))parse_ziti_client_cfg_v1) == ZITI_OK) {
            track_service_to_hostname(s->name, clt_cfg.hostname.addr.hostname, clt_cfg.port);
        }

        free_ziti_intercept_cfg_v1(intercept);
        free(intercept);
      }

      // Initiate the call into the JavaScript 'on_init' callback, now that we know about all the services
      ZITI_NODEJS_LOG(DEBUG, "addon_data->on_init_cb_invoked %o", addon_data->on_init_cb_invoked);
      if (!addon_data->on_init_cb_invoked) {
        nstatus = napi_call_threadsafe_function(
            addon_data->tsfn,
            (void*) (long) addon_data->zitiContextEventStatus,
            napi_tsfn_blocking);
        addon_data->on_init_cb_invoked = true;
      }

      break;

    case ZitiRouterEvent:
      switch (event->event.router.status){
        case EdgeRouterConnected:
          ZITI_NODEJS_LOG(INFO, "ziti connected to edge router %s\nversion = %s", event->event.router.name, event->event.router.version);
          break;
        case EdgeRouterDisconnected:
          ZITI_NODEJS_LOG(INFO, "ziti disconnected from edge router %s", event->event.router.name);
          break;
        case EdgeRouterRemoved:
          ZITI_NODEJS_LOG(INFO, "ziti removed edge router %s", event->event.router.name);
          break;
        case EdgeRouterUnavailable:
          ZITI_NODEJS_LOG(INFO, "edge router %s is not available", event->event.router.name);
          break;
        case EdgeRouterAdded:
          ZITI_NODEJS_LOG(INFO, "edge router %s was added", event->event.router.name);
          break;
        }

    default:
        break;
    }
}


/**
 * 
 */
napi_value _ziti_init(napi_env env, const napi_callback_info info) {
    napi_status status;
    napi_value jsRetval;

    ZITI_NODEJS_LOG(DEBUG, "initializing");

    // uv_mbed_set_debug(TRACE, stdout);

    size_t argc = 2;
    napi_value args[2];
    status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

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

    // Obtain ptr to JS callback function
    napi_value js_cb = args[1];
    napi_value work_name;
    AddonData *addon_data = calloc(1, sizeof(AddonData));

    // Create a string to describe this asynchronous operation.
    status = napi_create_string_utf8(
            env,
            "N-API on_ziti_init",
            NAPI_AUTO_LENGTH,
            &work_name);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to napi_create_string_utf8");
    }

    // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn)
    // which we can call from a worker thread.
    status = napi_create_threadsafe_function(
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
            &(addon_data->tsfn));
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to napi_create_threadsafe_function");
    }

    ziti_log_init(thread_loop, ZITI_LOG_DEFAULT_LEVEL, NULL);

    ziti_config cfg = {0};

    int rc = ziti_load_config(&cfg, config_file_name);
    ZITI_NODEJS_LOG(DEBUG, "ziti_load_config => %d", rc);
    if (rc != ZITI_OK) goto done;

    rc = ziti_context_init(&ztx, &cfg);
    ZITI_NODEJS_LOG(DEBUG, "ziti_context_init => %d", rc);
    if (rc != ZITI_OK) goto done;

    ziti_options opts = {
            .events = ZitiContextEvent | ZitiServiceEvent | ZitiRouterEvent,
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
    status = napi_create_int32(env, rc, &jsRetval);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Unable to create return value");
    }

    return jsRetval;
}


void expose_ziti_init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_init, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_init");
  }

  status = napi_set_named_property(env, exports, "ziti_init", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_init");
  }

}
