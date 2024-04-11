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
#include <string.h>

// An item that will be generated here and passed into the JavaScript write callback
typedef struct WSWriteItem {
  tlsuv_websocket_t *ws;
  ssize_t status;
} WSWriteItem;


/**
 * This function is responsible for calling the JavaScript 'write' callback function 
 * that was specified when the ziti_write(...) was called from JavaScript.
 */
static void CallJs_on_write(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;

  // This parameter is not used.
  (void) context;

  // Retrieve the WriteItem created by the worker thread.
  WSWriteItem* item = (WSWriteItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {

    napi_value undefined;

    // const obj = {}
    napi_value js_write_item, js_ws, js_status;
    status = napi_create_object(env, &js_write_item);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_object");
    }

    // obj.ws = ws
    ZITI_NODEJS_LOG(DEBUG, "ws=%p", item->ws);
    status = napi_create_int64(env, (int64_t)item->ws, &js_ws);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_int64");
    }
    status = napi_set_named_property(env, js_write_item, "ws", js_ws);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_set_named_property");
    }

    // obj.status = status
    ZITI_NODEJS_LOG(DEBUG, "status=%zd", item->status);
    status = napi_create_int64(env, (int64_t)item->status, &js_status);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_int64");
    }
    status = napi_set_named_property(env, js_write_item, "status", js_status);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_set_named_property");
    }

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined (5)");
    }

    // Call the JavaScript function and pass it the WriteItem
    status = napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_write_item,
        NULL);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_call_function");
    }

    free(item);
  }
}


/**
 * 
 */
static void on_write(uv_write_t *req, int status) {
  ZITI_NODEJS_LOG(DEBUG, "=========ws_write_cb: req: %p, status: %d", req, status);

  WSAddonData* addon_data = (WSAddonData*) req->data;

  free(req);

  WSWriteItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->ws = &(addon_data->ws);
  item->status = status;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  napi_status nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_write,
      item,
      napi_tsfn_blocking);
  if (nstatus != napi_ok) {
    ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
  }

}


/**
 * 
 */
napi_value _ziti_websocket_write(napi_env env, const napi_callback_info info) {
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

  // Obtain websocket
  int64_t js_ws;
  status = napi_get_value_int64(env, args[0], &js_ws);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Conn");
  }
  tlsuv_websocket_t *ws = (tlsuv_websocket_t*)js_ws;
  ZITI_NODEJS_LOG(DEBUG, "========= ws: %p", ws);

  WSAddonData* addon_data = (WSAddonData*) ws->data;
  ZITI_NODEJS_LOG(DEBUG, "========= &(addon_data->ws): %p", &(addon_data->ws));

  // Obtain data to write (we expect a Buffer)
  void*  buffer;
  size_t bufferLength;
  status = napi_get_buffer_info(env, args[1], &buffer, &bufferLength);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Buffer info");
  }

  // Since the underlying Buffer's lifetime is not guaranteed if it's managed by the VM, we will copy the chunk into our heap
  void* chunk = memset(malloc(bufferLength + 1), 0, bufferLength + 1);
  memcpy(chunk, buffer, bufferLength);

  // Obtain ptr to JS 'write' callback function
  napi_value js_write_cb = args[2];
  napi_value work_name;

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_write",
    NAPI_AUTO_LENGTH,
    &work_name);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
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
      &(addon_data->tsfn_on_write));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to napi_create_threadsafe_function");
  }

  // Now, call the C-SDK to actually write the data over the websocket

  uv_write_t *wr = malloc(sizeof(uv_write_t));
  wr->data = addon_data;
  uv_buf_t b;
  b.base = chunk;
  b.len = bufferLength;

  tlsuv_websocket_write(wr, &(addon_data->ws), &b, on_write);

  return NULL;
}



/**
 * 
 */
void expose_ziti_websocket_write(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_websocket_write, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_websocket_write");
  }

  status = napi_set_named_property(env, exports, "ziti_websocket_write", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_websocket_write");
  }

}

