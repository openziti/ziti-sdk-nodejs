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
#include <string.h>


// An item that will be generated here and passed into the JavaScript write callback
typedef struct WriteItem {
  nf_connection conn;
  ssize_t status;
} WriteItem;


/**
 * This function is responsible for calling the JavaScript 'write' callback function 
 * that was specified when the NF_write(...) was called from JavaScript.
 */
static void CallJs_on_write(napi_env env, napi_value js_cb, void* context, void* data) {

  // This parameter is not used.
  (void) context;

  // Retrieve the WriteItem created by the worker thread.
  WriteItem* item = (WriteItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {

    napi_value undefined;

    // const obj = {}
    napi_value js_write_item, js_conn, js_status;
    assert(napi_create_object(env, &js_write_item) == napi_ok);

    // obj.conn = conn
    assert(napi_create_int64(env, (int64_t)item->conn, &js_conn) == napi_ok);
    assert(napi_set_named_property(env, js_write_item, "conn", js_conn) == napi_ok);

    // obj.status = status
    assert(napi_create_int64(env, (int64_t)item->status, &js_status) == napi_ok);
    assert(napi_set_named_property(env, js_write_item, "status", js_status) == napi_ok);

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

    // Call the JavaScript function and pass it the WriteItem
    assert(
      napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_write_item,
        NULL) == napi_ok);
  }

}


/**
 * 
 */
static void on_write(nf_connection conn, ssize_t status, void *ctx) {

  ConnAddonData* addon_data = (ConnAddonData*) NF_conn_data(conn);

  WriteItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->conn = conn;
  item->status = status;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  assert(
    napi_call_threadsafe_function(
      addon_data->tsfn_on_write,
      item,
      napi_tsfn_blocking) == napi_ok);
}


/**
 * 
 */
napi_value _NF_write(napi_env env, const napi_callback_info info) {
  napi_status status;
  size_t argc = 3;
  napi_value args[3];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 3) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain nf_connection
  int64_t js_conn;
  status = napi_get_value_int64(env, args[0], &js_conn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Conn");
  }
  nf_connection conn = (nf_connection)js_conn;

  ConnAddonData* addon_data = (ConnAddonData*) NF_conn_data(conn);

  // Obtain data to write (we expect a Buffer)
  void*  buffer;
  size_t bufferLength;
  status = napi_get_buffer_info(env, args[1], &buffer, &bufferLength);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Buffer info");
  }

  // Since the underlying Buffer's lifetime is not guaranteed if it's managed by the VM, we will copy the chunk into our heap
  void* chunk = memset(malloc(bufferLength), 0, bufferLength);
  memcpy(chunk, buffer, bufferLength);

  // Obtain ptr to JS 'write' callback function
  napi_value js_write_cb = args[2];
  napi_value work_name;

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(
    env,
    "N-API on_write",
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
      CallJs_on_write,
      &(addon_data->tsfn_on_write)) == napi_ok);

  // Now, call the C-SDK to actually write the data over to the service
  NF_write(conn, chunk, bufferLength, on_write, NULL);

  return NULL;
}



/**
 * 
 */
void expose_NF_write(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _NF_write, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_NF_write");
  }

  status = napi_set_named_property(env, exports, "NF_write", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'NF_write");
  }

}

