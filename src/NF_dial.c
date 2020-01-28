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

#include <nf/ziti-nodejs.h>
#include <unistd.h>

/**
 * This function is responsible for calling the JavaScript 'connect' callback function 
 * that was specified when the NF_dial(...) was called from JavaScript.
 */
static void CallJs_on_connect(napi_env env, napi_value js_cb, void* context, void* data) {

  // This parameter is not used.
  (void) context;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {
    napi_value undefined, js_conn;

    // Convert the nf_connection to a napi_value.
    if (NULL != data) {
      // Retrieve the nf_connection from the item created by the worker thread.
      nf_connection conn = *(nf_connection*)data;
      assert(napi_create_int64(env, (int64_t)conn, &js_conn) == napi_ok);
    } else {
      assert(napi_get_undefined(env, &js_conn) == napi_ok);
    }

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

    // Call the JavaScript function and pass it the nf_connection
    assert(
      napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_conn,
        NULL
      ) == napi_ok);
  }
}


/**
 * This function is responsible for calling the JavaScript 'data' callback function 
 * that was specified when the NF_dial(...) was called from JavaScript.
 */
static void CallJs_on_data(napi_env env, napi_value js_cb, void* context, void* data) {

  // This parameter is not used.
  (void) context;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {
    napi_value undefined, js_buf;

    // Convert the buf to a napi_value.
    assert(napi_create_string_utf8(env, (const char*)data, NAPI_AUTO_LENGTH, &js_buf) == napi_ok);

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

    // Call the JavaScript function and pass it the data
    assert(
      napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_buf,
        NULL
      ) == napi_ok);
  }
}




/**
 * This function is the callback invoked by the C-SDK when data arrives on the connection.
 */
void on_data(nf_connection conn, uint8_t *buf, int len) {

  ConnAddonData* addon_data = (ConnAddonData*) NF_conn_data(conn);

  if (len == ZITI_EOF) {
    NF_close(&conn);
  }
  else if (len < 0) {
    NF_close(&conn);
  }
  else {

    // Save the 'buf' to the heap. The JavaScript marshaller (CallJs)
    // will free this item after having sent it to JavaScript.
    void* newBuf = memset(malloc(len+2), 0, len+2);
    memcpy(newBuf, buf, len);

    // Initiate the call into the JavaScript callback. 
    // The call into JavaScript will not have happened 
    // when this function returns, but it will be queued.
    assert(
      napi_call_threadsafe_function(
        addon_data->tsfn_on_data,        
        newBuf,  // Send the data we received from the service on over to the JS callback
        napi_tsfn_blocking) == napi_ok);
  }
}


/**
 * 
 */
void on_connect(nf_connection conn, int status) {

  ConnAddonData* addon_data = (ConnAddonData*) NF_conn_data(conn);
  nf_connection* the_conn = NULL;

  if (status == ZITI_OK) {

    // Save the 'nf_connection' to the heap. The JavaScript marshaller (CallJs)
    // will free this item after having sent it to JavaScript.
    the_conn = malloc(sizeof(nf_connection));
    *the_conn = conn;

  }

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  assert(
    napi_call_threadsafe_function(
      addon_data->tsfn_on_connect,
      the_conn,   // Send the nf_connection over to the JS callback
      napi_tsfn_blocking) == napi_ok);
}


/**
 * 
 */
napi_value _NF_dial(napi_env env, const napi_callback_info info) {

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

  // Obtain service name
  size_t result;
  char ServiceName[100];  //TODO: make this smarter
  status = napi_get_value_string_utf8(env, args[0], ServiceName, 100, &result);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Service Name");
  }

  // Obtain ptr to JS 'connect' callback function
  napi_value js_connect_cb = args[1];
  napi_value work_name_connect;

  ConnAddonData* addon_data = memset(malloc(sizeof(*addon_data)), 0, sizeof(*addon_data));

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(
    env,
    "N-API on_connect",
    NAPI_AUTO_LENGTH,
    &work_name_connect) == napi_ok);

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  assert(
    napi_create_threadsafe_function(
      env,
      js_connect_cb,
      NULL,
      work_name_connect,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_connect,
      &(addon_data->tsfn_on_connect)) == napi_ok);

  // Obtain ptr to JS 'data' callback function
  napi_value js_data_cb = args[2];
  napi_value work_name_data;

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(
    env,
    "N-API on_data",
    NAPI_AUTO_LENGTH,
    &work_name_data) == napi_ok);

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  assert(
    napi_create_threadsafe_function(
      env,
      js_data_cb,
      NULL,
      work_name_data,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_data,
      &(addon_data->tsfn_on_data)) == napi_ok);

  // Init a Ziti connection object, and attach our add-on data to it so we can 
  // pass context around between our callbacks, as propagate it all the way out
  // to the JavaScript callbacks
  nf_connection conn;
  DIE(NF_conn_init(nf, &conn, addon_data));

  // Connect to the service
  // fprintf(stderr, "==NODEJS== calling NF_dial(%s)\n", ServiceName);
  DIE(NF_dial(conn, ServiceName, on_connect, on_data));

  // usleep(100*1000);

  return NULL;
}



/**
 * 
 */
void expose_NF_dial(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _NF_dial, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_NF_dial");
  }

  status = napi_set_named_property(env, exports, "NF_dial", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'NF_dial");
  }

}

