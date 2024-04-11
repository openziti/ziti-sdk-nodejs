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
 * 
 */
napi_value _ziti_set_log_level(napi_env env, const napi_callback_info info) {
  napi_status status;
  napi_value jsRetval;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 1) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  int64_t js_log_level;
  status = napi_get_value_int64(env, args[0], &js_log_level);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get logLevel");
  }

  ZITI_NODEJS_LOG(DEBUG, "js_log_level: %lld", js_log_level);

  ziti_nodejs_debug_level = js_log_level;
  ziti_log_set_level(js_log_level, NULL);

  status = napi_create_int32(env, 0, &jsRetval);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
  }

  return jsRetval;
}


void expose_ziti_set_log_level(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_set_log_level, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_set_log_level");
  }

  status = napi_set_named_property(env, exports, "ziti_set_log_level", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_set_log_level");
  }

}
