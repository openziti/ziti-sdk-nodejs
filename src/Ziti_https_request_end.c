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
#include <ziti/ziti_src.h>


/**
 * Indicate that an active HTTPS request is now complete
 * 
 * @param {string} [0] req
 * 
 * @returns NULL
 */
napi_value _Ziti_http_request_end(napi_env env, const napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc != 1) {
    napi_throw_error(env, "EINVAL", "Invalid argument count");
    return NULL;
  }

  // Obtain tlsuv_http_req_t
  int64_t js_req;
  status = napi_get_value_int64(env, args[0], &js_req);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Req");
  }
  HttpsReq* httpsReq = (HttpsReq*)js_req;
  tlsuv_http_req_t *r = httpsReq->req;

  ZITI_NODEJS_LOG(DEBUG, "req: %p", r);

  // TEMP hack to work around an issue still being debugged
  ZITI_NODEJS_LOG(DEBUG, "httpsReq->on_resp_has_fired: %o", httpsReq->on_resp_has_fired);
  if (httpsReq->on_resp_has_fired) {
    ZITI_NODEJS_LOG(DEBUG, "seems as though on_resp has previously fired... skipping call to tlsuv_http_req_end");
  } else {
    tlsuv_http_req_end(r);
  }

  return NULL;
}


void expose_ziti_https_request_end(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _Ziti_http_request_end, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_Ziti_http_request_end");
  }

  status = napi_set_named_property(env, exports, "Ziti_http_request_end", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'Ziti_http_request_end");
  }

}
