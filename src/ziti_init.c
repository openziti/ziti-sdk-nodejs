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

ziti_context ztx = NULL;
uv_loop_t *loop = NULL;

uv_thread_t thread;
uv_async_t async;


typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
} AddonData;


/**
 * This function is responsible for calling the JavaScript callback function 
 * that was specified when the ziti_init(...) was called from JavaScript.
 */
static void CallJs(napi_env env, napi_value js_cb, void* context, void* data) {
    napi_status status;

  // This parameter is not used.
  (void) context;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {
    napi_value undefined, js_rc;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);

    // Retrieve the rc created by the worker thread.
    int rc = (int)data;
    status = napi_create_int64(env, (int64_t)rc, &js_rc);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to napi_create_int64");
    }

    // Call the JavaScript function and pass it the rc
    status = napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_rc,
        NULL
      );
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to napi_call_function");
    }
  }
}


/**
 * 
 */
void on_ziti_init(ziti_context _ztx, int status, void* ctx) {
  napi_status nstatus;

  ZITI_NODEJS_LOG(DEBUG, "_ztx: %p", _ztx);

  // Set the global ztx context variable
  ztx = _ztx;

  if (status == ZITI_OK) {

    ziti_set_timeout(ztx, 60*1000);

  }

  AddonData* addon_data = (AddonData*)ctx;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  nstatus = napi_call_threadsafe_function(
      addon_data->tsfn,
      (void*) (long) status,
      napi_tsfn_blocking);
    if (nstatus != napi_ok) {
      ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
    }
}



static void child_thread(void *data){
    uv_loop_t *thread_loop = (uv_loop_t *) data;

    //Start this loop
    uv_run(thread_loop, UV_RUN_DEFAULT);
}

static void consumer_notify(uv_async_t *handle, int status) { }

/**
 * 
 */
napi_value _ziti_init(napi_env env, const napi_callback_info info) {
  napi_status status;
  napi_value jsRetval;

  ZITI_NODEJS_LOG(DEBUG, "initializing");

  size_t argc = 2;
  napi_value args[2];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 2) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain location of identity file
  size_t result;
  char ConfigFileName[100];
  status = napi_get_value_string_utf8(env, args[0], ConfigFileName, 100, &result);

  // Obtain ptr to JS callback function
  napi_value js_cb = args[1];
  napi_value work_name;
  AddonData* addon_data = malloc(sizeof(AddonData));

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_ziti_init",
    NAPI_AUTO_LENGTH,
    &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
      env,
      js_cb,
      NULL,
      work_name,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs,
      &(addon_data->tsfn));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to napi_create_threadsafe_function");
  }

  // Create and set up the consumer thread
  if (NULL == thread_loop) {  // Spawn the loop only once
    ZITI_NODEJS_LOG(DEBUG, "calling uv_loop_new()");
    thread_loop = uv_loop_new();
    uv_async_init(thread_loop, &async, (uv_async_cb)consumer_notify);
    uv_thread_create(&thread, (uv_thread_cb)child_thread, thread_loop);
  }

  // Light this candle!
  int rc = ziti_init(ConfigFileName, thread_loop, on_ziti_init, addon_data);

  status = napi_create_int32(env, rc, &jsRetval);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
  }

  return jsRetval;
}


void expose_ziti_init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_init, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_init");
  }

  status = napi_set_named_property(env, exports, "ziti_init", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_init");
  }

}
