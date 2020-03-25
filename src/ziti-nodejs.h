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

#ifndef NF_ZITI_ADD_ON_H
#define NF_ZITI_ADD_ON_H

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define NAPI_EXPERIMENTAL
#include <node_api.h>

#include <nf/ziti.h>

#include "utils.h"


#define DIE(v) do { \
int code = (v);\
if (code != ZITI_OK) {\
fprintf(stderr, "ERROR: " #v " => %s\n", ziti_errorstr(code));\
exit(code);\
}} while(0)


/**
 * 
 */
typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn_on_connect;
  napi_threadsafe_function tsfn_on_data;
  napi_threadsafe_function tsfn_on_write;
  napi_threadsafe_function tsfn_on_service_available;
} ConnAddonData;


#ifdef __cplusplus
extern "C" {
#endif

extern nf_context nf;

// extern pthread_mutex_t nf_init_lock;

// extern void set_signal_handler();

extern void expose_NF_close(napi_env env, napi_value exports);
extern void expose_NF_dial(napi_env env, napi_value exports);
extern void expose_NF_enroll(napi_env env, napi_value exports);
extern void expose_NF_hello(napi_env env, napi_value exports);
extern void expose_NF_init(napi_env env, napi_value exports);
extern void expose_NF_service_available(napi_env env, napi_value exports);
extern void expose_NF_shutdown(napi_env env, napi_value exports);
extern void expose_NF_write(napi_env env, napi_value exports);


#ifdef __cplusplus
}
#endif

#endif /* NF_ZITI_ADD_ON_H */