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
#include <time.h> 


typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
} AddonData;

// An item that will be generated here and passed into the JavaScript service_available callback
typedef struct ServiceAvailableItem {
  ssize_t status;
  ssize_t permissions;
} ServiceAvailableItem;


/**
 * This function is responsible for calling the JavaScript 'service_available' callback function 
 * that was specified when the NF_service_available(...) was called from JavaScript.
 */
static void CallJs_on_service_available(napi_env env, napi_value js_cb, void* context, void* data) {

  // This parameter is not used.
  (void) context;

  // Retrieve the ServiceAvailableItem created by the worker thread.
  ServiceAvailableItem* item = (ServiceAvailableItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {

    napi_value undefined;

    // const obj = {}
    napi_value js_service_available_item, js_status, js_permissions;
    assert(napi_create_object(env, &js_service_available_item) == napi_ok);

    // obj.status = status
    assert(napi_create_int64(env, (int64_t)item->status, &js_status) == napi_ok);
    assert(napi_set_named_property(env, js_service_available_item, "status", js_status) == napi_ok);

    // obj.permissions = permissions
    assert(napi_create_int64(env, (int64_t)item->permissions, &js_permissions) == napi_ok);
    assert(napi_set_named_property(env, js_service_available_item, "permissions", js_permissions) == napi_ok);

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

    // Call the JavaScript function and pass it the ServiceAvailableItem
    assert(
      napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_service_available_item,
        NULL) == napi_ok);
  }
}


/**
 * 
 */
static void on_service_available(nf_context nf_ctx, const char* service, int status, unsigned int permissions, void *ctx) {

  AddonData* addon_data = (AddonData*)ctx;

  ServiceAvailableItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->status = status;
  item->permissions = permissions;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  assert(
    napi_call_threadsafe_function(
      addon_data->tsfn,
      item,
      napi_tsfn_blocking) == napi_ok);
}

/**
 * 
 */
napi_value _NF_service_available(napi_env env, const napi_callback_info info) {
  napi_status status;
  size_t argc = 2;
  napi_value args[2];
  napi_value jsRetval;

  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 2) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain service name
  size_t result;
  char* ServiceName = malloc(256); // TODO: make this smarter
  status = napi_get_value_string_utf8(env, args[0], ServiceName, 100, &result);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Service Name not provided");
  }

  // Obtain ptr to JS 'service_available' callback function
  napi_value js_write_cb = args[1];
  napi_value work_name;
  AddonData* addon_data = malloc(sizeof(AddonData));

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(
    env,
    "N-API on_service_available",
    NAPI_AUTO_LENGTH,
    &work_name) == napi_ok);

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  assert(
    napi_create_threadsafe_function(
      env,
      js_write_cb,
      NULL,
      work_name,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_service_available,
      &(addon_data->tsfn)) == napi_ok);

  // Now, call the C-SDK to see if the service name is present
  NF_service_available(nf, ServiceName, on_service_available, addon_data);

  status = napi_create_int32(env, 0 /* always succeed here, it is the cb that tells the real tale */, &jsRetval);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
  }

  free(ServiceName);

  return jsRetval;
}


/**
 * 
 */
void expose_NF_service_available(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _NF_service_available, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_NF_service_available");
  }

  status = napi_set_named_property(env, exports, "NF_service_available", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'NF_service_available");
  }

}

