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

ziti_context nf;
uv_loop_t *enroll_loop = NULL;

uv_loop_t *thread_loop;
uv_thread_t thread;
uv_async_t async;


typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
} AddonData;

// An item that will be generated here and passed into the JavaScript enroll callback
typedef struct EnrollItem {

  unsigned char *json_salvo;
  int status;
  char *err;

} EnrollItem;


/**
 * This function is responsible for calling the JavaScript callback function 
 * that was specified when the ziti_enroll(...) was called from JavaScript.
 */
static void CallJs_on_enroll(napi_env env, napi_value js_cb, void* context, void* data) {

  ZITI_NODEJS_LOG(DEBUG, "entered");

  // This parameter is not used.
  (void) context;

  // Retrieve the EnrollItem created by the worker thread.
  EnrollItem* item = (EnrollItem*)data;

  ZITI_NODEJS_LOG(DEBUG, "item->json_salvo: %s", item->json_salvo);
  ZITI_NODEJS_LOG(DEBUG, "item->status: %d", item->status);
  ZITI_NODEJS_LOG(DEBUG, "item->err: %s", item->err);

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    napi_value undefined;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

    // const obj = {}
    napi_value js_enroll_item, js_json_salvo, js_status, js_err;
    assert(napi_create_object(env, &js_enroll_item) == napi_ok);

    // obj.identity = identity
    if (NULL != item->json_salvo) {
      assert(napi_create_string_utf8(env, (const char*)item->json_salvo, NAPI_AUTO_LENGTH, &js_json_salvo) == napi_ok);
      assert(napi_set_named_property(env, js_enroll_item, "identity", js_json_salvo) == napi_ok);
    } else {
      assert(napi_set_named_property(env, js_enroll_item, "identity", undefined) == napi_ok);
    }

    // obj.status = status
    assert(napi_create_int64(env, (int64_t)item->status, &js_status) == napi_ok);
    assert(napi_set_named_property(env, js_enroll_item, "status", js_status) == napi_ok);

    // obj.err = err
    if (NULL != item->err) {
      assert(napi_create_string_utf8(env, (const char*)item->err, NAPI_AUTO_LENGTH, &js_err) == napi_ok);
      assert(napi_set_named_property(env, js_enroll_item, "err", js_err) == napi_ok);
    } else {
      assert(napi_set_named_property(env, js_enroll_item, "err", undefined) == napi_ok);
    }


    // Call the JavaScript function and pass it the WriteItem
    assert(
      napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_enroll_item,
        NULL) == napi_ok);

  }
}


/**
 * 
 */
void on_ziti_enroll(const ziti_config *cfg, int status, const char *err, void *ctx) {

  ZITI_NODEJS_LOG(DEBUG, "\nstatus: %d, \nerr: %s,\nctx: %p", status, err, ctx);

  AddonData* addon_data = (AddonData*)ctx;

  EnrollItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));

  item->status = status;

  if (NULL != err) {
    item->err = calloc(1, strlen(err) + 1);
    strcpy(item->err, err);
  } else {
    item->err = NULL;
  }

  if (status == ZITI_OK) {
    size_t len;
    char *output_buf = ziti_config_to_json(cfg, 0, &len);
    item->json_salvo = calloc(1, len + 1);
    strcpy(item->json_salvo, output_buf);
  }

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  assert(
    napi_call_threadsafe_function(
      addon_data->tsfn,
      item,
      napi_tsfn_blocking) == napi_ok);
}



void child_thread(void *data){
    uv_loop_t *thread_loop = (uv_loop_t *) data;

    //Start this loop
    uv_run(thread_loop, UV_RUN_DEFAULT);
}

static void consumer_notify(uv_async_t *handle, int status) { }

/**
 * 
 */
napi_value _ziti_enroll(napi_env env, const napi_callback_info info) {
  napi_status status;
  napi_value jsRetval;

  ZITI_NODEJS_LOG(DEBUG, "entered");

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

  // Obtain location of JWT file
  size_t result;
  char JWTFileName[256];
  status = napi_get_value_string_utf8(env, args[0], JWTFileName, 256, &result);

  // Obtain ptr to JS callback function
  napi_value js_cb = args[1];
  napi_value work_name;
  AddonData* addon_data = malloc(sizeof(AddonData));

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(
    env,
    "N-API on_ziti_enroll",
    NAPI_AUTO_LENGTH,
    &work_name) == napi_ok);

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  assert(
    napi_create_threadsafe_function(
      env,
      js_cb,
      NULL,
      work_name,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_enroll,
      &(addon_data->tsfn)) == napi_ok);

  // Create and set up the consumer thread
  thread_loop = uv_loop_new();
  uv_async_init(thread_loop, &async, (uv_async_cb)consumer_notify);
  uv_thread_create(&thread, (uv_thread_cb)child_thread, thread_loop);

  // Initiate the enrollment
  ziti_enroll_opts opts = {0};
  opts.jwt = JWTFileName;
  int rc = ziti_enroll(&opts, thread_loop, on_ziti_enroll, addon_data);

  status = napi_create_int32(env, rc, &jsRetval);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
  }

  return jsRetval;
}


void expose_ziti_enroll(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_enroll, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_enroll");
  }

  status = napi_set_named_property(env, exports, "ziti_enroll", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_enroll");
  }

}
