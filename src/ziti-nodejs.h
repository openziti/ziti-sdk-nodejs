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
#include <uv_mbed/uv_mbed.h>
#include <uv_mbed/um_http.h>

#include <node_version.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>

#include <ziti/ziti.h>
#include "utils.h"

#ifdef _WIN32
#define _NO_CRT_STDIO_INLINE 1
  /* Windows - set up dll import/export decorators. */
# if defined(BUILDING_UV_SHARED)
    /* Building shared library. */
#   define UV_EXTERN __declspec(dllexport)
# elif defined(USING_UV_SHARED)
    /* Using shared library. */
#   define UV_EXTERN __declspec(dllimport)
# else
    /* Building static library. */
#   define UV_EXTERN /* nothing */
# endif


#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#if !defined (strndup_DEFINED)
#define strndup_DEFINED
static char* strndup(char* p, size_t len) {
    char *s = malloc(len + 1);
    strncpy(s, p, len);
    s[len] = '\0';
    return s;
}
#endif // strndup_DEFINED


#elif __GNUC__ >= 4
# define UV_EXTERN __attribute__((visibility("default")))
#else
# define UV_EXTERN /* nothing */
#endif

#  ifdef NODE_MAJOR_VERSION
#    if NODE_MAJOR_VERSION == 11
UV_EXTERN int uv_gettimeofday(uv_timeval_t* tv);
#    else
UV_EXTERN int uv_gettimeofday(uv_timeval64_t* tv);
#    endif
#  endif



/**
 * 
 */
typedef struct {
  bool isWebsocket;
  napi_async_work work;
  napi_threadsafe_function tsfn_on_connect;
  napi_threadsafe_function tsfn_on_data;
  napi_threadsafe_function tsfn_on_write;
  napi_threadsafe_function tsfn_on_service_available;
} ConnAddonData;


// An item that will be passed into the JavaScript on_resp callback
typedef struct HttpsRespItem {
  um_http_req_t *req;
  int code;
  char* status;
  um_http_hdr *headers;
} HttpsRespItem;

// An item that will be passed into the JavaScript on_resp_body callback
typedef struct HttpsRespBodyItem {
  um_http_req_t *req;
  const void *body;
  ssize_t len;
} HttpsRespBodyItem;

// An item that will be passed into the JavaScript on_req_body callback
typedef struct HttpsReqBodyItem {
  um_http_req_t *req;
  const void *body;
  ssize_t status;
} HttpsReqBodyItem;


typedef struct HttpsAddonData HttpsAddonData;

typedef struct HttpsReq {
  um_http_req_t *req;
  bool on_resp_has_fired;
  int respCode;
  HttpsAddonData *addon_data;
} HttpsReq;

typedef struct {
  char* scheme_host_port;
  um_http_t client;
  um_http_src_t ziti_src;
  bool active;
} HttpsClient;

struct HttpsAddonData {
  napi_env env;
  um_http_t client;
  um_http_src_t ziti_src;
  napi_threadsafe_function tsfn_on_req;
  napi_threadsafe_function tsfn_on_resp;
  napi_threadsafe_function tsfn_on_resp_body;
  napi_threadsafe_function tsfn_on_req_body;
  HttpsRespItem* item;
  HttpsReq* httpsReq;
  uv_work_t uv_req;
  char* service;
  char* scheme_host_port;
  char* method;
  char* path;
  uint32_t headers_array_length;
  char* header_name[100];
  char* header_value[100];
  HttpsClient* httpsClient;
} ;


#ifdef __cplusplus
extern "C" {
#endif

extern ziti_context ztx;
extern uv_loop_t *thread_loop;

extern uv_mutex_t client_pool_lock;

// extern void set_signal_handler();

extern void expose_ziti_close(napi_env env, napi_value exports);
extern void expose_ziti_dial(napi_env env, napi_value exports);
extern void expose_ziti_enroll(napi_env env, napi_value exports);
extern void expose_ziti_hello(napi_env env, napi_value exports);
extern void expose_ziti_init(napi_env env, napi_value exports);
extern void expose_ziti_service_available(napi_env env, napi_value exports);
extern void expose_ziti_shutdown(napi_env env, napi_value exports);
extern void expose_ziti_write(napi_env env, napi_value exports);
extern void expose_ziti_https_request(napi_env env, napi_value exports);
extern void expose_ziti_https_request_data(napi_env env, napi_value exports);
extern void expose_ziti_https_request_end(napi_env env, napi_value exports);


#ifdef __cplusplus
}
#endif

#endif /* NF_ZITI_ADD_ON_H */