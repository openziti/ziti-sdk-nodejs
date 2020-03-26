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

  // Install call-stack tracer
  // set_signal_handler();


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
  ZITI_NODEJS_LOG(INFO, "Ziti NodeJS SDK version %s @%s(%s) starting at (%s.%03ld)",
        ziti_nodejs_get_version(false), ziti_nodejs_git_commit(), ziti_nodejs_git_branch(),
        time_str,
        start_time.tv_usec/1000);
#    else
  ZITI_NODEJS_LOG(INFO, "Ziti NodeJS SDK version %s @%s(%s) starting at (%s.%03d)",
        ziti_nodejs_get_version(false), ziti_nodejs_git_commit(), ziti_nodejs_git_branch(),
        time_str,
        start_time.tv_usec/1000);
#    endif
#  endif

  // Expose some Ziti SDK functions to JavaScript
  expose_NF_close(env, exports);
  expose_NF_dial(env, exports);
  expose_NF_enroll(env, exports);
  expose_NF_hello(env, exports);
  expose_NF_init(env, exports);
  expose_NF_service_available(env, exports);
  expose_NF_shutdown(env, exports);
  expose_NF_write(env, exports);

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)