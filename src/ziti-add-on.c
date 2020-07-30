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

extern void set_signal_handler();


napi_value Init(napi_env env, napi_value exports) {

  //
  thread_loop = NULL;

  if (uv_mutex_init(&client_pool_lock))
    abort();

  // Install call-stack tracer
  // set_signal_handler();

// TEMP: skip logging on windows
#ifndef WIN32

  init_nodejs_debug();

#  ifdef NODE_MAJOR_VERSION
#    if NODE_MAJOR_VERSION == 11
  uv_timeval_t start_time;
#    else
  uv_timeval64_t start_time;
#    endif
#  endif

  uv_gettimeofday(&start_time);

  struct tm *start_tm = gmtime((const time_t*)&start_time.tv_sec);
  char time_str[32];
  strftime(time_str, sizeof(time_str), "%FT%T", start_tm);

#  ifdef NODE_MAJOR_VERSION
#    if NODE_MAJOR_VERSION == 11
  ZITI_NODEJS_LOG(INFO, "Ziti NodeJS SDK version %s@%s(%s) starting at (%s.%03ld)",
        ziti_nodejs_get_version(true), ziti_nodejs_git_commit(), ziti_nodejs_git_branch(),
        time_str,
        start_time.tv_usec/1000);
#    else
  ZITI_NODEJS_LOG(INFO, "Ziti NodeJS SDK version %s@%s(%s) starting at (%s.%03d)",
        ziti_nodejs_get_version(true), ziti_nodejs_git_commit(), ziti_nodejs_git_branch(),
        time_str,
        start_time.tv_usec/1000);
#    endif
#  endif

#endif

  // Expose some Ziti SDK functions to JavaScript
  expose_ziti_close(env, exports);
  expose_ziti_dial(env, exports);
  expose_ziti_enroll(env, exports);
  expose_ziti_hello(env, exports);
  expose_ziti_init(env, exports);
  expose_ziti_service_available(env, exports);
  expose_ziti_shutdown(env, exports);
  expose_ziti_write(env, exports);

  expose_ziti_https_request(env, exports);
  expose_ziti_https_request_data(env, exports);
  expose_ziti_https_request_end(env, exports);

  expose_ziti_websocket_connect(env, exports);
  expose_ziti_websocket_write(env, exports);

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)