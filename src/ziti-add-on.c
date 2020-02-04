/*
Copyright 2019-2020 Netfoundry, Inc.

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

napi_value Init(napi_env env, napi_value exports) {

  // Install call-stack tracer
  // set_signal_handler();

  // Expose some Ziti SDK functions to JavaScript
  expose_NF_hello(env, exports);
  expose_NF_init(env, exports);
  expose_NF_shutdown(env, exports);
  expose_NF_dial(env, exports);
  expose_NF_write(env, exports);
  expose_NF_service_available(env, exports);
  expose_NF_close(env, exports);

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)