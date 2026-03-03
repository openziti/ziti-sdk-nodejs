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


/**
 * Callback invoked when websocket close completes
 */
static void on_ws_close(uv_handle_t* handle) {
  ZITI_NODEJS_LOG(DEBUG, "websocket closed: %p", handle);
  // The WSAddonData will be cleaned up here if needed
  // For now, we just log the close
}


/**
 * Close a WebSocket connection
 *
 * @param {number} [0] ws - The WebSocket handle
 *
 * @returns NULL
 */
napi_value _ziti_websocket_close(napi_env env, const napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return NULL;
  }

  if (argc < 1) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain websocket handle
  int64_t js_ws;
  status = napi_get_value_int64(env, args[0], &js_ws);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get WebSocket handle");
    return NULL;
  }

  tlsuv_websocket_t *ws = (tlsuv_websocket_t*)js_ws;
  ZITI_NODEJS_LOG(DEBUG, "closing websocket: %p", ws);

  if (ws == NULL) {
    napi_throw_error(env, "EINVAL", "Invalid WebSocket handle");
    return NULL;
  }

  // Close the websocket
  int rc = tlsuv_websocket_close(ws, on_ws_close);
  if (rc != 0) {
    ZITI_NODEJS_LOG(ERROR, "tlsuv_websocket_close failed: %d", rc);
  }

  return NULL;
}


/**
 * Expose the function to JavaScript
 */
void expose_ziti_websocket_close(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_websocket_close, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_websocket_close'");
  }

  status = napi_set_named_property(env, exports, "ziti_websocket_close", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_websocket_close'");
  }
}
