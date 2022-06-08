/*
Copyright Netfoundry, Inc.

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

// An item that will be generated here and passed into the JavaScript on_listen_client callback
typedef struct OnClientItem {

  int status;
  ziti_client_ctx *clt_ctx;

} OnClientItem;

/**
 * This function is responsible for calling the JavaScript 'on_listen' callback function 
 * that was specified when the ziti_listen(...) was called from JavaScript.
 */
static void CallJs_on_listen(napi_env env, napi_value js_cb, void* context, void* data) {
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
static void CallJs_on_listen_client(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;

  // This parameter is not used.
  (void) context;

  // Retrieve the OnClientItem created by the worker thread.
  OnClientItem* item = (OnClientItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {

    napi_value undefined, js_buffer;
    void* result_data;

    // const obj = {}
    napi_value js_client_item, js_clt_ctx, js_status;
    int rc = napi_create_object(env, &js_client_item);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }

    // obj.code = status
    rc = napi_create_int32(env, item->status, &js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.status");
    }
    rc = napi_set_named_property(env, js_client_item, "status", js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property status");
    }
    ZITI_NODEJS_LOG(DEBUG, "status: %zd", item->status);

    // obj.clt_ctx = clt_ctx
    napi_create_int64(env, (int64_t)item->clt_ctx, &js_clt_ctx);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.req");
    }
    rc = napi_set_named_property(env, js_client_item, "clt_ctx", js_clt_ctx);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property req");
    }
    ZITI_NODEJS_LOG(DEBUG, "js_clt_ctx: %p", item->clt_ctx);

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
        &js_client_item,
        NULL
      );
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_call_function");
    }
  }
}


static ssize_t on_listen_client_data(ziti_connection clt, uint8_t *data, ssize_t len) {
  // impl me
}

static void on_listen_client_connect(ziti_connection clt, int status) {
  // impl me
}


/**
 * 
 */
void on_listen_client(ziti_connection serv, ziti_connection client, int status, ziti_client_ctx *clt_ctx) {

  napi_status nstatus;

  ListenAddonData* addon_data = (ListenAddonData*) ziti_conn_data(serv);

  ZITI_NODEJS_LOG(INFO, "on_listen_client: client: %p, status: %d, conn: %p", client, status);

  if (status == ZITI_OK) {
    const char *source_identity = clt_ctx->caller_id;
    if (source_identity != NULL) {
        ZITI_NODEJS_LOG(DEBUG, "on_listen_client: incoming connection from '%s'\n", source_identity );
    }
    else {
        ZITI_NODEJS_LOG(DEBUG, "on_listen_client: incoming connection from unidentified client" );
    }
    if (clt_ctx->app_data != NULL) {
        ZITI_NODEJS_LOG(DEBUG, "on_listen_client: got app data '%.*s'!\n", (int) clt_ctx->app_data_sz, clt_ctx->app_data );
    }

    ziti_accept(client, on_listen_client_connect, on_listen_client_data);
  
  } else {
    fprintf(stderr, "failed to accept client: %s(%d)\n", ziti_errorstr(status), status);
    ZITI_NODEJS_LOG(DEBUG, "on_listen_client: failed to accept client: %s(%d)\n", ziti_errorstr(status), status );
  }

  OnClientItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->status = status;
  item->clt_ctx = clt_ctx;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_listen_client,        
      item,  // Send the client ctx we received over to the JS callback
      napi_tsfn_blocking);
  if (nstatus != napi_ok) {
    ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
  }
  
}


/**
 * 
 */
void on_listen(ziti_connection conn, int status) {
  napi_status nstatus;

  ListenAddonData* addon_data = (ListenAddonData*) ziti_conn_data(conn);
  ziti_connection* the_conn = NULL;

  ZITI_NODEJS_LOG(DEBUG, "on_listen: conn: %p, status: %o", conn, status );

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
      addon_data->tsfn_on_listen,
      the_conn,   // Send the ziti_connection over to the JS callback
      napi_tsfn_blocking);
  if (nstatus != napi_ok) {
    ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
  }
}


/**
 * 
 */
napi_value _ziti_listen(napi_env env, const napi_callback_info info) {

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
  char ServiceName[256];  //TODO: make this smarter
  status = napi_get_value_string_utf8(env, args[0], ServiceName, 256, &result);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Service Name");
  }
  ZITI_NODEJS_LOG(DEBUG, "ServiceName: %s", ServiceName);

  // Obtain ptr to JS 'on_listen' callback function
  napi_value js_on_listen = args[1];
  napi_value work_name_listen;

  ListenAddonData* addon_data = memset(malloc(sizeof(*addon_data)), 0, sizeof(*addon_data));

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_listen",
    NAPI_AUTO_LENGTH,
    &work_name_listen);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
      env,
      js_on_listen,
      NULL,
      work_name_listen,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_listen,
      &(addon_data->tsfn_on_listen));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_threadsafe_function");
  }

  // Obtain ptr to JS 'on_listen_client' callback function
  napi_value js_on_listen_client = args[2];
  napi_value work_name_client;

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_listen_client",
    NAPI_AUTO_LENGTH,
    &work_name_client);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
      env,
      js_on_listen_client,
      NULL,
      work_name_client,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_listen_client,
      &(addon_data->tsfn_on_listen_client));
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


  // Start listening
  ZITI_NODEJS_LOG(DEBUG, "calling ziti_listen_with_options: %p", ztx);
  ziti_listen_opts listen_opts = {
    .bind_using_edge_identity = false,
  };
  ziti_listen_with_options(conn, ServiceName, &listen_opts, on_listen, on_listen_client);

  return NULL;
}



/**
 * 
 */
void expose_ziti_listen(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_listen, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_listen");
  }

  status = napi_set_named_property(env, exports, "ziti_listen", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_listen");
  }

}

