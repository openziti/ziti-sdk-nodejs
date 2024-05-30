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

  napi_status status;

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
    status = napi_create_object(env, &js_enroll_item);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_object");
    }

    // obj.identity = identity
    if (NULL != item->json_salvo) {
      napi_create_string_utf8(env, (const char*)item->json_salvo, NAPI_AUTO_LENGTH, &js_json_salvo);
      napi_set_named_property(env, js_enroll_item, "identity", js_json_salvo);
    }

    // obj.status = status
    napi_create_int64(env, (int64_t)item->status, &js_status);
    napi_set_named_property(env, js_enroll_item, "status", js_status);

    // obj.err = err
    if (NULL != item->err) {
      napi_create_string_utf8(env, (const char*)item->err, NAPI_AUTO_LENGTH, &js_err);
      napi_set_named_property(env, js_enroll_item, "err", js_err);
    }


    // Call the JavaScript function and pass it the EnrollItem
    napi_value global;
    status = napi_get_global(env, &global);

    status = napi_call_function(
      env,
      global,
      js_cb,
      1,
      &js_enroll_item,
      NULL
    );

    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to invoke JS callback");
    }

  }
}


/**
 * 
 */
void on_ziti_enroll(ziti_config *cfg, int status, char *err, void *ctx) {
  napi_status nstatus;

  ZITI_NODEJS_LOG(DEBUG, "\nstatus: %d, \nerr: %s,\nctx: %p", status, err, ctx);

  EnrollAddonData* addon_data = (EnrollAddonData*)ctx;

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
  nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_enroll,
      item,
      napi_tsfn_blocking);
  if (nstatus != napi_ok) {
    ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
  }

}




/**
 * 
 */
napi_value _ziti_enroll(napi_env env, const napi_callback_info info) {
  napi_status status;
  napi_value jsRetval;
  napi_valuetype js_cb_type;

  ziti_log_init(thread_loop, ZITI_LOG_DEFAULT_LEVEL, NULL);

  ZITI_NODEJS_LOG(DEBUG, "entered");

  size_t argc = 2;
  napi_value args[2];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 2) {
    ZITI_NODEJS_LOG(DEBUG, "Too few arguments");
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain location of JWT file
  size_t result;
  char JWTFileName[256];
  status = napi_get_value_string_utf8(env, args[0], JWTFileName, 256, &result);

  // Obtain ptr to JS callback function
  // napi_value js_cb = args[1];
  napi_typeof(env, args[1], &js_cb_type);
  if (js_cb_type != napi_function) {
    ZITI_NODEJS_LOG(DEBUG, "args[1] is NOT a napi_function");
  } else {
    ZITI_NODEJS_LOG(DEBUG, "args[1] IS a napi_function");
  }
  napi_value work_name;

  EnrollAddonData* addon_data = memset(malloc(sizeof(*addon_data)), 0, sizeof(*addon_data));

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(
    env,
    "N-API on_ziti_enroll",
    NAPI_AUTO_LENGTH,
    &work_name) == napi_ok);

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
      env,
      args[1],
      NULL,
      work_name,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_enroll,
      &(addon_data->tsfn_on_enroll));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_threadsafe_function");
  }


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
