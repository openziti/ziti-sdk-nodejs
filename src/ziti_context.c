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
  napi_threadsafe_function tsfn_on_auth_event;
  int zitiContextEventStatus;
} AddonData;

// An item that will be passed into the JavaScript on_auth_event callback
typedef struct AuthEventItem {
  char *action;
  char *type;
  char *detail;
} AuthEventItem;


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
        napi_value js_rc;
        napi_create_int64(env, (int64_t)rc, &js_rc);
        NAPI_CHECK(env, "calling js callback(success)",
            napi_call_function(env, undefined, js_cb, 1, &js_rc, NULL));
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
 * This function is responsible for calling the JavaScript auth event callback.
 */
static void CallJs_on_auth_event(napi_env env, napi_value js_cb, void* context, void* data) {
    (void) context;

    if (env != NULL && data != NULL) {
        AuthEventItem *item = (AuthEventItem *)data;

        NAPI_UNDEFINED(env, undefined);

        // Create the auth event object to pass to JS
        napi_value auth_event;
        NAPI_CHECK(env, "create auth event object", napi_create_object(env, &auth_event));

        // Set the action property
        napi_value action_val;
        NAPI_CHECK(env, "create action string",
                   napi_create_string_utf8(env, item->action ? item->action : "", NAPI_AUTO_LENGTH, &action_val));
        NAPI_CHECK(env, "set action property",
                   napi_set_named_property(env, auth_event, "action", action_val));

        // Set the type property
        napi_value type_val;
        NAPI_CHECK(env, "create type string",
                   napi_create_string_utf8(env, item->type ? item->type : "", NAPI_AUTO_LENGTH, &type_val));
        NAPI_CHECK(env, "set type property",
                   napi_set_named_property(env, auth_event, "type", type_val));

        // Set the detail property
        napi_value detail_val;
        NAPI_CHECK(env, "create detail string",
                   napi_create_string_utf8(env, item->detail ? item->detail : "", NAPI_AUTO_LENGTH, &detail_val));
        NAPI_CHECK(env, "set detail property",
                   napi_set_named_property(env, auth_event, "detail", detail_val));

        // Call the JS callback with the auth event object
        napi_value argv[1] = { auth_event };
        NAPI_CHECK(env, "calling auth event callback",
                   napi_call_function(env, undefined, js_cb, 1, argv, NULL));

        // Clean up
        if (item->action) free(item->action);
        if (item->type) free(item->type);
        if (item->detail) free(item->detail);
        free(item);
    }
}

/**
 * 
 */
static void on_ziti_event(ziti_context _ztx, const ziti_event_t *event) {
  napi_status nstatus;

  AddonData* addon_data = (AddonData*)ziti_app_ctx(_ztx);

  ZITI_NODEJS_LOG(DEBUG, "on_ziti_event: event->type: %d", event->type);

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
          const char *action_str = NULL;
          int should_fail = 0;
          int status = ZITI_EXTERNAL_LOGIN_REQUIRED;

          switch (event->auth.action) {
              case ziti_auth_login_external:
                  action_str = "login_external";
                  ZITI_NODEJS_LOG(INFO, "external auth required: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
              case ziti_auth_select_external:
                  action_str = "select_external";
                  ZITI_NODEJS_LOG(INFO, "select external auth provider: type=%s", event->auth.type);
                  break;
              case ziti_auth_cannot_continue:
                  action_str = "cannot_continue";
                  status = ZITI_AUTHENTICATION_FAILED;
                  should_fail = 1;
                  ZITI_NODEJS_LOG(ERROR, "cannot continue authentication: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
              case ziti_auth_prompt_totp:
                  action_str = "prompt_totp";
                  ZITI_NODEJS_LOG(INFO, "TOTP auth required: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
              case ziti_auth_prompt_pin:
                  action_str = "prompt_pin";
                  ZITI_NODEJS_LOG(INFO, "PIN auth required: type=%s, detail=%s", event->auth.type,
                                  event->auth.detail);
                  break;
          }

          // If we have an auth event callback, send the event to JavaScript
          if (addon_data && addon_data->tsfn_on_auth_event) {
              AuthEventItem *item = calloc(1, sizeof(AuthEventItem));
              if (item) {
                  item->action = action_str ? strdup(action_str) : NULL;
                  item->type = event->auth.type ? strdup(event->auth.type) : NULL;
                  item->detail = event->auth.detail ? strdup(event->auth.detail) : NULL;
                  napi_call_threadsafe_function(addon_data->tsfn_on_auth_event, item, napi_tsfn_nonblocking);
              }
          }

          // Only complete init with failure for cannot_continue
          // For other auth events, the JS code will handle authentication
          if (should_fail) {
              complete_init(addon_data, status);
          }
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

    size_t argc = 3;
    napi_value args[3];
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

    // Handle optional auth event callback (third argument)
    if (argc >= 3) {
        napi_valuetype arg_type;
        status = napi_typeof(env, args[2], &arg_type);
        if (status == napi_ok && arg_type == napi_function) {
            NAPI_LITERAL(env, auth_work_name, "N-API on_auth_event");
            NAPI_CHECK(env, "create auth event callback", napi_create_threadsafe_function(
                    env,
                    args[2],
                    NULL,
                    auth_work_name,
                    0,
                    1,
                    NULL,
                    NULL,
                    NULL,
                    CallJs_on_auth_event,
                    &(addon_data->tsfn_on_auth_event)));
        }
    }

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
        if (addon_data) {
            if (addon_data->tsfn) {
                napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release);
            }
            if (addon_data->tsfn_on_auth_event) {
                napi_release_threadsafe_function(addon_data->tsfn_on_auth_event, napi_tsfn_release);
            }
            free(addon_data);
        }
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
        ziti_context local_ztx = ztx;
        ztx = NULL;

        AddonData *addon_data = (AddonData*)ziti_app_ctx(local_ztx);
        if (addon_data) {
            if (addon_data->tsfn) {
                napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release);
                addon_data->tsfn = NULL;
            }
            if (addon_data->tsfn_on_auth_event) {
                napi_release_threadsafe_function(addon_data->tsfn_on_auth_event, napi_tsfn_release);
                addon_data->tsfn_on_auth_event = NULL;
            }
            // Don't free addon_data here — ziti_shutdown() is async and
            // its events still reference addon_data via ziti_app_ctx().
            // The nulled-out tsfn pointers ensure the event handlers no-op safely.
        }
        ziti_shutdown(local_ztx);
    }
    return jsRetval;
}


ZNODE_EXPOSE(ziti_init, load_ztx)

ZNODE_EXPOSE(ziti_shutdown, ztx_shutdown)


/**
 * Data structure for external auth initialization
 */
typedef struct {
    napi_threadsafe_function tsfn_on_complete;    // Completion callback
    napi_threadsafe_function tsfn_on_auth_event;  // Auth event callback
    tlsuv_http_t http_client;
    tls_context *tls;
    char *controller_url;
    char *ca_pem;           // PEM-encoded CA certificate (result)
    char *response_body;    // Accumulated response body
    size_t response_body_len;
    size_t response_body_capacity;
} ExtAuthAddonData;

// Global pointer to track the external auth data
static ExtAuthAddonData *ext_auth_data = NULL;

/**
 * Item passed to the completion callback
 */
typedef struct ExtAuthCompleteItem {
    char *zt_api;
    char *ca_pem;
    int error_code;
    char *error_message;
} ExtAuthCompleteItem;

/**
 * JavaScript callback for external auth completion.
 * Creates the config object: { ztAPI: "...", id: { ca: "..." } }
 */
static void CallJs_on_ext_auth_complete(napi_env env, napi_value js_cb, void* context, void* data) {
    (void) context;

    if (env != NULL && data != NULL) {
        ExtAuthCompleteItem *item = (ExtAuthCompleteItem *)data;

        NAPI_UNDEFINED(env, undefined);

        if (item->error_code != 0) {
            // Call callback with error
            napi_value err_msg;
            napi_value result;
            NAPI_CHECK(env, "create error string",
                       napi_create_string_utf8(env, item->error_message ? item->error_message : "Unknown error",
                                               NAPI_AUTO_LENGTH, &err_msg));
            NAPI_CHECK(env, "create error", napi_create_error(env, NULL, err_msg, &result));
            NAPI_CHECK(env, "calling ext auth complete callback(error)",
                       napi_call_function(env, undefined, js_cb, 1, &result, NULL));
        } else {
            // Create the config object: { ztAPI: "...", id: { ca: "..." } }
            napi_value config_obj;
            NAPI_CHECK(env, "create config object", napi_create_object(env, &config_obj));

            // Set ztAPI property
            napi_value zt_api_val;
            NAPI_CHECK(env, "create ztAPI string",
                       napi_create_string_utf8(env, item->zt_api ? item->zt_api : "", NAPI_AUTO_LENGTH, &zt_api_val));
            NAPI_CHECK(env, "set ztAPI property",
                       napi_set_named_property(env, config_obj, "ztAPI", zt_api_val));

            // Create id object
            napi_value id_obj;
            NAPI_CHECK(env, "create id object", napi_create_object(env, &id_obj));

            // Set ca property on id object
            napi_value ca_val;
            NAPI_CHECK(env, "create ca string",
                       napi_create_string_utf8(env, item->ca_pem ? item->ca_pem : "", NAPI_AUTO_LENGTH, &ca_val));
            NAPI_CHECK(env, "set ca property",
                       napi_set_named_property(env, id_obj, "ca", ca_val));

            // Set id property on config object
            NAPI_CHECK(env, "set id property",
                       napi_set_named_property(env, config_obj, "id", id_obj));

            // Call the callback with the config object
            napi_value argv[1] = { config_obj };
            NAPI_CHECK(env, "calling ext auth complete callback",
                       napi_call_function(env, undefined, js_cb, 1, argv, NULL));
        }

        // Clean up
        if (item->zt_api) free(item->zt_api);
        if (item->ca_pem) free(item->ca_pem);
        if (item->error_message) free(item->error_message);
        free(item);
    }
}

/**
 * Helper to call the completion callback
 */
static void complete_ext_auth(ExtAuthAddonData *data, int error_code, const char *error_message) {
    if (data && data->tsfn_on_complete) {
        ExtAuthCompleteItem *item = calloc(1, sizeof(ExtAuthCompleteItem));
        if (item) {
            item->error_code = error_code;
            if (error_code == 0) {
                item->zt_api = data->controller_url ? strdup(data->controller_url) : NULL;
                item->ca_pem = data->ca_pem ? strdup(data->ca_pem) : NULL;
            } else {
                item->error_message = error_message ? strdup(error_message) : NULL;
            }
            napi_call_threadsafe_function(data->tsfn_on_complete, item, napi_tsfn_blocking);
        }
    }
}

/**
 * Certificate verification callback that accepts all certificates.
 * This is used for the initial CA fetch where we're bootstrapping trust.
 */
static int ext_auth_cert_verify(const struct tlsuv_certificate_s *cert, void *ctx) {
    ZITI_NODEJS_LOG(DEBUG, "ext auth: accepting server certificate for CA bootstrap");
    return 0;  // 0 = success, accept the certificate
}

/**
 * Callback for HTTP response body data from /.well-known/est/cacerts
 */
static void on_ext_auth_resp_body(tlsuv_http_req_t *req, char *body, ssize_t len) {
    ExtAuthAddonData *data = (ExtAuthAddonData *)req->data;

    if (body == NULL || len == UV_EOF) {
        // End of response - now parse the PKCS7
        ZITI_NODEJS_LOG(DEBUG, "ext auth: received complete response, len=%zd", data->response_body_len);

        if (data->response_body && data->response_body_len > 0) {
            // Parse the base64-encoded PKCS7 certificate
            tlsuv_certificate_t chain = NULL;

            ZITI_NODEJS_LOG(DEBUG, "ext auth: parsing PKCS7 data");

            int rc = data->tls->parse_pkcs7_certs(&chain, data->response_body, data->response_body_len);
            if (rc != 0) {
                ZITI_NODEJS_LOG(ERROR, "ext auth: failed to parse PKCS7 certs, rc=%d", rc);
                complete_ext_auth(data, rc, "Failed to parse PKCS7 certificates");
            } else {
                // Convert to PEM
                char *pem = NULL;
                size_t pem_len = 0;
                rc = chain->to_pem(chain, 1, &pem, &pem_len);
                if (rc != 0) {
                    ZITI_NODEJS_LOG(ERROR, "ext auth: failed to convert cert to PEM, rc=%d", rc);
                    complete_ext_auth(data, rc, "Failed to convert certificate to PEM");
                } else {
                    ZITI_NODEJS_LOG(INFO, "ext auth: successfully obtained CA PEM, len=%zd", pem_len);
                    // ZITI_NODEJS_LOG(DEBUG, "ext auth: CA PEM:\n%s", pem);
                    data->ca_pem = pem;
                    // Call completion callback with success
                    complete_ext_auth(data, 0, NULL);
                }
                chain->free(chain);
            }
        } else {
            ZITI_NODEJS_LOG(ERROR, "ext auth: empty response body");
            complete_ext_auth(data, -1, "Empty response from controller");
        }

        // Clean up response body buffer
        if (data->response_body) {
            free(data->response_body);
            data->response_body = NULL;
            data->response_body_len = 0;
            data->response_body_capacity = 0;
        }
        return;
    }

    // Accumulate response body
    if (len > 0) {
        // Grow buffer if needed
        size_t needed = data->response_body_len + len + 1;
        if (needed > data->response_body_capacity) {
            size_t new_capacity = needed * 2;
            char *new_buf = realloc(data->response_body, new_capacity);
            if (new_buf == NULL) {
                ZITI_NODEJS_LOG(ERROR, "ext auth: failed to allocate response buffer");
                return;
            }
            data->response_body = new_buf;
            data->response_body_capacity = new_capacity;
        }
        memcpy(data->response_body + data->response_body_len, body, len);
        data->response_body_len += len;
        data->response_body[data->response_body_len] = '\0';
    }
}

/**
 * Callback for HTTP response from /.well-known/est/cacerts
 */
static void on_ext_auth_resp(tlsuv_http_resp_t *resp, void *ctx) {
    ExtAuthAddonData *data = (ExtAuthAddonData *)ctx;

    ZITI_NODEJS_LOG(DEBUG, "ext auth: received response, code=%d, status=%s",
                    resp->code, resp->status ? resp->status : "NULL");

    if (resp->code < 0) {
        ZITI_NODEJS_LOG(ERROR, "ext auth: HTTP request failed, code=%d", resp->code);
        complete_ext_auth(data, resp->code, resp->status ? resp->status : "HTTP request failed");
        return;
    }

    if (resp->code != 200) {
        ZITI_NODEJS_LOG(ERROR, "ext auth: unexpected HTTP status %d", resp->code);
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "HTTP error %d: %s", resp->code, resp->status ? resp->status : "Unknown");
        complete_ext_auth(data, resp->code, err_msg);
        return;
    }

    // Set up body callback to receive the response data
    resp->body_cb = on_ext_auth_resp_body;
}

/**
 * Initialize with external authentication using a controller hostname.
 *
 * @param controllerHost - The hostname of the Ziti controller (e.g., "ctrl.example.com:1280")
 * @param onComplete - Callback function called with config object when CA fetch completes
 * @param onAuthEvent - Callback function for authentication events
 */
static napi_value load_ztx_external_auth(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value jsRetval;

    ZITI_NODEJS_LOG(DEBUG, "initializing external auth");

    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        napi_throw_error(env, "EINVAL", "Too few arguments - requires controllerHost, and onComplete callback");
        return NULL;
    }

    // Get the controller hostname string
    size_t result;
    size_t controller_host_len;
    status = napi_get_value_string_utf8(env, args[0], NULL, 0, &controller_host_len);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "controllerHost is not a string");
        return NULL;
    }
    char *controller_host = calloc(1, controller_host_len + 2);
    status = napi_get_value_string_utf8(env, args[0], controller_host, controller_host_len + 1, &result);
    if (status != napi_ok) {
        free(controller_host);
        napi_throw_error(env, "EINVAL", "Failed to obtain controllerHost");
        return NULL;
    }
    ZITI_NODEJS_LOG(DEBUG, "controller_host: %s", controller_host);

    // Verify the onComplete callback is a function
    napi_valuetype arg_type;
    status = napi_typeof(env, args[1], &arg_type);
    if (status != napi_ok || arg_type != napi_function) {
        free(controller_host);
        napi_throw_error(env, "EINVAL", "onComplete must be a function");
        return NULL;
    }

    // Set up addon data
    ExtAuthAddonData *addon_data = calloc(1, sizeof(ExtAuthAddonData));
    ext_auth_data = addon_data;

    // Create the completion callback threadsafe function
    NAPI_LITERAL(env, complete_work_name, "N-API on_ext_auth_complete");
    NAPI_CHECK(env, "create complete callback", napi_create_threadsafe_function(
            env,
            args[1],
            NULL,
            complete_work_name,
            0,
            1,
            NULL,
            NULL,
            NULL,
            CallJs_on_ext_auth_complete,
            &(addon_data->tsfn_on_complete)));

    // Build the controller URL (add https:// prefix if needed)
    size_t url_len = controller_host_len + 10; // "https://" + host + null
    addon_data->controller_url = calloc(1, url_len);
    if (strncmp(controller_host, "https://", 8) == 0 || strncmp(controller_host, "http://", 7) == 0) {
        strcpy(addon_data->controller_url, controller_host);
    } else {
        strcpy(addon_data->controller_url, "https://");
        strcat(addon_data->controller_url, controller_host);
    }
    ZITI_NODEJS_LOG(DEBUG, "controller_url: %s", addon_data->controller_url);

    // Create TLS context - use empty string to skip default CA bundle
    // We'll verify certs manually since we're bootstrapping trust
    addon_data->tls = default_tls_context("", 0);
    if (addon_data->tls == NULL) {
        ZITI_NODEJS_LOG(ERROR, "failed to create TLS context");
        free(controller_host);
        free(addon_data->controller_url);
        free(addon_data);
        napi_throw_error(env, "EINVAL", "Failed to create TLS context");
        return NULL;
    }

    // Set custom certificate verification that accepts all certs for CA bootstrap
    addon_data->tls->set_cert_verify(addon_data->tls, ext_auth_cert_verify, NULL);

    // Initialize the HTTP client
    int rc = tlsuv_http_init(thread_loop, &addon_data->http_client, addon_data->controller_url);
    if (rc != 0) {
        ZITI_NODEJS_LOG(ERROR, "failed to initialize HTTP client, rc=%d", rc);
        addon_data->tls->free_ctx(addon_data->tls);
        free(controller_host);
        free(addon_data->controller_url);
        free(addon_data);
        napi_throw_error(env, "EINVAL", "Failed to initialize HTTP client");
        return NULL;
    }

    // Set the TLS context on the HTTP client
    tlsuv_http_set_ssl(&addon_data->http_client, addon_data->tls);

    // Make the GET request to /.well-known/est/cacerts
    ZITI_NODEJS_LOG(DEBUG, "requesting /.well-known/est/cacerts");
    tlsuv_http_req_t *req = tlsuv_http_req(
            &addon_data->http_client,
            "GET",
            "/.well-known/est/cacerts",
            on_ext_auth_resp,
            addon_data);

    if (req == NULL) {
        ZITI_NODEJS_LOG(ERROR, "failed to create HTTP request");
        tlsuv_http_close(&addon_data->http_client, NULL);
        addon_data->tls->free_ctx(addon_data->tls);
        free(controller_host);
        free(addon_data->controller_url);
        free(addon_data);
        napi_throw_error(env, "EINVAL", "Failed to create HTTP request");
        return NULL;
    }

    // Store addon_data in request for use in callbacks
    req->data = addon_data;

    // Set the Accept header for PKCS7
    tlsuv_http_req_header(req, "Accept", "application/pkcs7-mime");

    free(controller_host);

    rc = ZITI_OK;
    NAPI_CHECK(env, "create return value", napi_create_int32(env, rc, &jsRetval));
    return jsRetval;
}

ZNODE_EXPOSE(ziti_init_external_auth, load_ztx_external_auth)
