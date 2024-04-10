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

#include <ziti/ziti.h>


/**
 * 
 */
napi_value _ziti_sdk_version(napi_env env, const napi_callback_info info) {
  napi_value jsRetval = NULL;
  napi_status status = napi_generic_failure;

  const ziti_version *ver = ziti_get_version();

  status = napi_create_string_utf8(env, ver->version, NAPI_AUTO_LENGTH, &jsRetval);
  if (status != napi_ok) return NULL;

  return jsRetval;
}


/**
 * 
 */
void expose_ziti_sdk_version(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_sdk_version, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_sdk_version");
  }

  status = napi_set_named_property(env, exports, "ziti_sdk_version", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_sdk_version");
  }

}

