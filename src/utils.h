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

#ifndef ZITI_TLS_UTILS_H
#define ZITI_TLS_UTILS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *ziti_nodejs_get_version(int verbose); 
extern const char *ziti_nodejs_git_branch();
extern const char *ziti_nodejs_git_commit();

extern void init_nodejs_debug(uv_loop_t *loop);

#define ZITI_NODEJS_LOG(level, fmt, ...) ZITI_LOG(level, fmt, ##__VA_ARGS__)

#define NAPI_CHECK(env, msg, op) do {             \
  if ((op) != napi_ok)  napi_throw_error(env, NULL, "failed to " msg); \
} while(0)

#define NAPI_UNDEFINED(env, var) \
napi_value var; NAPI_CHECK(env, "get undefined", napi_get_undefined(env, &var));

#define NAPI_GLOBAL(env, var) \
napi_value var; NAPI_CHECK(env, "get global", napi_get_global(env, &var));

#ifdef __cplusplus
}
#endif

#endif //ZITI_TLS_UTILS_H
