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

// An item that will be generated here and passed into the JavaScript on_data callback
typedef struct OnDataItem {

  const unsigned char *buf;
  int len;

} OnDataItem;

/**
 * This function is responsible for calling the JavaScript 'connect' callback function 
 * that was specified when the ziti_dial(...) was called from JavaScript.
 */
static void CallJs_on_connect(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;

  // This parameter is not used.
  (void) context;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {
    napi_value undefined, js_conn;

    ZITI_NODEJS_LOG(INFO, "data: %p", data);

    // Convert the ziti_connection to a napi_value.
    if (NULL != data) {
      // Retrieve the ziti_connection from the item created by the worker thread.
      ziti_connection conn = *(ziti_connection*)data;
      status = napi_create_int64(env, (int64_t)conn, &js_conn);
      if (status != napi_ok) {
        napi_throw_error(env, NULL, "Unable to napi_create_int64");
      }
    } else {
      status = napi_get_undefined(env, &js_conn) == napi_ok;
      if (status != napi_ok) {
        napi_throw_error(env, NULL, "Unable to napi_get_undefined (1)");
      }
    }

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined (2)");
    }

    ZITI_NODEJS_LOG(INFO, "calling JS callback...");

    // Call the JavaScript function and pass it the ziti_connection
    status = napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_conn,
        NULL
      );
    ZITI_NODEJS_LOG(INFO, "returned from JS callback...");
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_call_function");
    }
  }
}


/**
 * This function is responsible for calling the JavaScript 'data' callback function 
 * that was specified when the ziti_dial(...) was called from JavaScript.
 */
static void CallJs_on_data(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;

  // This parameter is not used.
  (void) context;

  // Retrieve the OnDataItem created by the worker thread.
  OnDataItem* item = (OnDataItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {
    napi_value undefined, js_buffer;
    void* result_data;

    // Convert the buffer to a napi_value.
    status = napi_create_buffer_copy(env,
                              item->len,
                              (const void*)item->buf,
                              (void**)&result_data,
                              &js_buffer);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_buffer_copy");
    }

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined (3)");
    }

    // Call the JavaScript function and pass it the data
    status = napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_buffer,
        NULL
      );
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_call_function");
    }
  }
}



/**
 * This function is the callback invoked by the C-SDK when data arrives on the connection.
 */
long on_data(struct ziti_conn *conn, const unsigned char *buf, long len) {
  napi_status status;

  ConnAddonData* addon_data = (ConnAddonData*) ziti_conn_data(conn);

  ZITI_NODEJS_LOG(INFO, "len: %zd, conn: %p", len, conn);

  if (len == ZITI_EOF) {
    if (addon_data->isWebsocket) {
      ZITI_NODEJS_LOG(DEBUG, "skipping ziti_close on ZITI_EOF due to isWebsocket=true");
      return 0;
    } else {
      ziti_close(conn, NULL);
      return 0;
    }
  }
  else if (len < 0) {
    ziti_close(conn, NULL);
    return 0;
  }
  else {

    OnDataItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
    item->buf = buf;
    item->buf = calloc(1, len);
    memcpy((void*)item->buf, buf, len);
    item->len = len;

    // if (addon_data->isWebsocket) {
    //   hexDump("on_data", item->buf, item->len);
    // }

    // Initiate the call into the JavaScript callback. 
    // The call into JavaScript will not have happened 
    // when this function returns, but it will be queued.
    status = napi_call_threadsafe_function(
        addon_data->tsfn_on_data,        
        item,  // Send the data we received from the service on over to the JS callback
        napi_tsfn_blocking);
    if (status != napi_ok) {
      ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
    }

    return len;
  }
}


/**
 * 
 */
void on_connect(ziti_connection conn, int status) {
  napi_status nstatus;


  ConnAddonData* addon_data = (ConnAddonData*) ziti_conn_data(conn);
  ziti_connection* the_conn = NULL;

  ZITI_NODEJS_LOG(DEBUG, "conn: %p, status: %o, isWebsocket: %o", conn, status, addon_data->isWebsocket);

  if (status == ZITI_OK) {

    // Save the 'ziti_connection' to the heap. The JavaScript marshaller (CallJs)
    // will free this item after having sent it to JavaScript.
    the_conn = malloc(sizeof(ziti_connection));
    *the_conn = conn;
  }

    ZITI_NODEJS_LOG(DEBUG, "the_conn: %p", the_conn);

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_connect,
      the_conn,   // Send the ziti_connection over to the JS callback
      napi_tsfn_blocking);
    if (nstatus != napi_ok) {
      ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
    }
}


/**
 * 
 */
napi_value _ziti_dial(napi_env env, const napi_callback_info info) {

  napi_status status;
  size_t argc = 4;
  napi_value args[4];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 4) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain service name
  size_t result;
  char ServiceName[256];  //TODO: make this smarter
  status = napi_get_value_string_utf8(env, args[0], ServiceName, 256, &result);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Service Name");
  }
  ZITI_NODEJS_LOG(DEBUG, "ServiceName: %s", ServiceName);

  // Obtain isWebsocket flag
  bool isWebsocket = false;
  status = napi_get_value_bool(env, args[1], &isWebsocket);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get isWebsocket flag");
  }
  ZITI_NODEJS_LOG(DEBUG, "isWebsocket is: %o", isWebsocket);

  // Obtain ptr to JS 'connect' callback function
  napi_value js_connect_cb = args[2];
  napi_value work_name_connect;

  ConnAddonData* addon_data = memset(malloc(sizeof(*addon_data)), 0, sizeof(*addon_data));

  addon_data->isWebsocket = isWebsocket;

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_connect",
    NAPI_AUTO_LENGTH,
    &work_name_connect);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
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
      &(addon_data->tsfn_on_connect));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_threadsafe_function");
  }

  // Obtain ptr to JS 'data' callback function
  napi_value js_data_cb = args[3];
  napi_value work_name_data;

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_data",
    NAPI_AUTO_LENGTH,
    &work_name_data);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
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
      &(addon_data->tsfn_on_data));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_threadsafe_function");
  }
  
  // Init a Ziti connection object, and attach our add-on data to it so we can 
  // pass context around between our callbacks, as propagate it all the way out
  // to the JavaScript callbacks
  ziti_connection conn;
  int rc = ziti_conn_init(ztx, &conn, addon_data);
  if (rc != ZITI_OK) {
    napi_throw_error(env, NULL, "failure in 'ziti_conn_init");
  }


  // Connect to the service
  ZITI_NODEJS_LOG(DEBUG, "calling ziti_dial: %p", ztx);
  rc = ziti_dial(conn, ServiceName, on_connect, on_data);
  if (rc != ZITI_OK) {
    napi_throw_error(env, NULL, "failure in 'ziti_dial");
  }
  ZITI_NODEJS_LOG(DEBUG, "returned from ziti_dial: %p", ztx);

  return NULL;
}



/**
 * 
 */
void expose_ziti_dial(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_dial, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_dial");
  }

  status = napi_set_named_property(env, exports, "ziti_dial", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_dial");
  }

}

