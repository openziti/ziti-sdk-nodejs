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

// An item that will be generated here and passed into the JavaScript on_listen_client callback
typedef struct OnClientItem {

  int status;
  ziti_connection client;
  int64_t js_arb_data;
  char *caller_id;
  uint8_t *app_data;
  size_t app_data_sz;

} OnClientItem;


/**
 * This function is responsible for calling the JavaScript 'on_listen_client_data' callback function 
 * that was specified when the ziti_listen(...) was called from JavaScript.
 */
static void CallJs_on_listen_client_data(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;

  // This parameter is not used.
  (void) context;

  // Retrieve the OnClientItem created by the worker thread.
  OnClientItem* item = (OnClientItem*)data;
  ZITI_NODEJS_LOG(INFO, "CallJs_on_listen_client_data: client: %p, app_data: %p, len: %zu", item->client, item->app_data, item->app_data_sz);

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    // const obj = {}
    napi_value undefined, js_client_item, js_client, js_buffer, js_arb_data;
    void* result_data;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined");
    }

    int rc = napi_create_object(env, &js_client_item);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }

    // js_client_item.js_arb_data = js_arb_data
    if (item->js_arb_data) {
      rc = napi_create_int64(env, item->js_arb_data, &js_arb_data);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create obj.js_arb_data");
      }
      rc = napi_set_named_property(env, js_client_item, "js_arb_data", js_arb_data);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property status");
      }
      ZITI_NODEJS_LOG(DEBUG, "js_arb_data: %lld", item->js_arb_data);
    } else {
      rc = napi_set_named_property(env, js_client_item, "js_arb_data", undefined);
    }

    // js_client_item.client = client
    napi_create_int64(env, (int64_t)item->client, &js_client);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create js_client_item.client");
    }
    rc = napi_set_named_property(env, js_client_item, "client", js_client);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property client");
    }
    ZITI_NODEJS_LOG(DEBUG, "client: %p", item->client);

    // js_client_item.app_data = app_data
    if (NULL != item->app_data) {
      rc = napi_create_buffer_copy(env, item->app_data_sz, (const void*)item->app_data, (void**)&result_data, &js_buffer);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create js_client_item.app_data");
      }
      rc = napi_set_named_property(env, js_client_item, "app_data", js_buffer);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property app_data");
      }
    } else {
      rc = napi_set_named_property(env, js_client_item, "app_data", undefined);
    }
    ZITI_NODEJS_LOG(DEBUG, "app_data: %p", item->app_data);


    ZITI_NODEJS_LOG(INFO, "calling JS on_listen_client_data callback...");

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

static ssize_t on_listen_client_data(ziti_connection client, const uint8_t *data, ssize_t len) {

  ZITI_NODEJS_LOG(DEBUG, "on_listen_client_data: client: %p, data: %p, len: %zd", client, data, len);

  ListenAddonData* addon_data = (ListenAddonData*) ziti_conn_data(client);

  OnClientItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->client = client;
  item->js_arb_data = addon_data->js_arb_data;

  if ((NULL != data) && ((len > 0))) {
    item->app_data = calloc(1, len + 1);
    memcpy((void*)item->app_data, data, len);
    item->app_data_sz = len;
  }

  if (len > 0) {
    /* NOP */
  }
  else if ((len == ZITI_EOF) || (len == ZITI_CONN_CLOSED)) {
      ZITI_NODEJS_LOG(DEBUG, "on_listen_client_data: client disconnected");
      ziti_close_write(client);
  }
  else {
      ZITI_NODEJS_LOG(ERROR, "on_listen_client_data: error: %zd(%s)", len, ziti_errorstr(len));
      ziti_close(client, NULL);
  }
  
  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  napi_status nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_listen_client_data,        
      item,  // Send the status we received over to the JS callback
      napi_tsfn_blocking);
  if (nstatus != napi_ok) {
    ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
  }

  return len;
}


/**
 * This function is responsible for calling the JavaScript 'on_listen_client_connect' callback function 
 * that was specified when the ziti_listen(...) was called from JavaScript.
 */
static void CallJs_on_listen_client_connect(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;

  // This parameter is not used.
  (void) context;

  // Retrieve the OnClientItem created by the worker thread.
  OnClientItem* item = (OnClientItem*)data;
  ZITI_NODEJS_LOG(INFO, "CallJs_on_listen_client_connect: client: %p, status: %d", item->client, item->status);

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    // const obj = {}
    napi_value undefined, js_client_item, js_client, js_clt_ctx, js_status, js_arb_data;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined");
    }

    int rc = napi_create_object(env, &js_client_item);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }
    rc = napi_create_object(env, &js_clt_ctx);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }

    // js_client_item.status = status
    rc = napi_create_int32(env, item->status, &js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create obj.status");
    }
    rc = napi_set_named_property(env, js_client_item, "status", js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property status");
    }
    ZITI_NODEJS_LOG(DEBUG, "status: %d", item->status);

    // js_client_item.js_arb_data = js_arb_data
    if (item->js_arb_data) {
      rc = napi_create_int64(env, item->js_arb_data, &js_arb_data);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create obj.js_arb_data");
      }
      rc = napi_set_named_property(env, js_client_item, "js_arb_data", js_arb_data);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property status");
      }
      ZITI_NODEJS_LOG(DEBUG, "js_arb_data: %lld", item->js_arb_data);
    } else {
      rc = napi_set_named_property(env, js_client_item, "js_arb_data", undefined);
    }

    // js_client_item.client = client
    napi_create_int64(env, (int64_t)item->client, &js_client);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create obj.client");
    }
    rc = napi_set_named_property(env, js_client_item, "client", js_client);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property req");
    }
    ZITI_NODEJS_LOG(DEBUG, "client: %p", item->client);


    ZITI_NODEJS_LOG(INFO, "calling JS on_listen_client_connect callback...");

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

static void on_listen_client_connect(ziti_connection client, int status) {

  ZITI_NODEJS_LOG(DEBUG, "on_listen_client_connect: client: %p, status: %d", client, status);

  ListenAddonData* addon_data = (ListenAddonData*) ziti_conn_data(client);

  OnClientItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->status = status;
  item->client = client;
  item->js_arb_data = addon_data->js_arb_data;

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  napi_status nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_listen_client_connect,        
      item,  // Send the status we received over to the JS callback
      napi_tsfn_blocking);
  if (nstatus != napi_ok) {
    ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
  }

}

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
    napi_value undefined, js_rc;

    ZITI_NODEJS_LOG(INFO, "CallJs_on_listen: data: %lld", (int64_t)data);

    // Retrieve the rc created by the worker thread.
    int64_t rc = (int64_t)data;
    status = napi_create_int64(env, (int64_t)rc, &js_rc);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to napi_create_int64");
    }

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined");
    }

    ZITI_NODEJS_LOG(INFO, "calling JS on_listen callback...");

    // Call the JavaScript function and pass it the rc
    status = napi_call_function(
        env,
        undefined,
        js_cb,
        1,
        &js_rc,
        NULL
      );
    ZITI_NODEJS_LOG(INFO, "returned from JS on_listen callback...");
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to napi_call_function");
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
  ZITI_NODEJS_LOG(INFO, "CallJs_on_listen_client: client: %p, status: %d, caller_id: %p, app_data: %p, app_data_sz: %zu", item->client, item->status, item->caller_id, item->app_data, item->app_data_sz);

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {

    napi_value undefined;

    // const obj = {}
    napi_value js_client_item, js_client, js_clt_ctx, js_status, js_arb_data, js_caller_id, js_buffer;
    void* result_data;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    status = napi_get_undefined(env, &undefined);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_get_undefined (3)");
    }

    int rc = napi_create_object(env, &js_client_item);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }
    rc = napi_create_object(env, &js_clt_ctx);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }

    // js_client_item.status = status
    rc = napi_create_int32(env, item->status, &js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create obj.status");
    }
    rc = napi_set_named_property(env, js_client_item, "status", js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property status");
    }
    ZITI_NODEJS_LOG(DEBUG, "status: %d", item->status);

    // js_client_item.js_arb_data = js_arb_data
    if (item->js_arb_data) {
      rc = napi_create_int64(env, item->js_arb_data, &js_arb_data);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create obj.js_arb_data");
      }
      rc = napi_set_named_property(env, js_client_item, "js_arb_data", js_arb_data);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property status");
      }
      ZITI_NODEJS_LOG(DEBUG, "js_arb_data: %lld", item->js_arb_data);
    } else {
      rc = napi_set_named_property(env, js_client_item, "js_arb_data", undefined);
    }

    // js_client_item.client = client
    napi_create_int64(env, (int64_t)item->client, &js_client);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create obj.client");
    }
    rc = napi_set_named_property(env, js_client_item, "client", js_client);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property req");
    }
    ZITI_NODEJS_LOG(DEBUG, "client: %p", item->client);

    // js_clt_ctx.caller_id = caller_id
    if (NULL != item->caller_id) {
      rc = napi_create_string_utf8(env, (const char*)item->caller_id, strlen(item->caller_id), &js_caller_id);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create js_clt_ctx.caller_id");
      }
      rc = napi_set_named_property(env, js_clt_ctx, "caller_id", js_caller_id);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property caller_id");
      }
      ZITI_NODEJS_LOG(DEBUG, "caller_id: %s", item->caller_id);
    } else {
      rc = napi_set_named_property(env, js_clt_ctx, "caller_id", undefined);
    }

    // js_clt_ctx.app_data = app_data
    if (NULL != item->app_data) {
      rc = napi_create_buffer_copy(env, item->app_data_sz, (const void*)item->app_data, (void**)&result_data, &js_buffer);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create js_clt_ctx.app_data");
      }
      rc = napi_set_named_property(env, js_clt_ctx, "app_data", js_buffer);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property app_data");
      }
    } else {
      rc = napi_set_named_property(env, js_clt_ctx, "app_data", undefined);
    }

    // js_client_item.clt_ctx = js_clt_ctx
    rc = napi_set_named_property(env, js_client_item, "clt_ctx", js_clt_ctx);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property js_clt_ctx");
    }
    ZITI_NODEJS_LOG(DEBUG, "js_clt_ctx added");

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



/**
 * 
 */
void on_listen_client(ziti_connection serv, ziti_connection client, int status, ziti_client_ctx *clt_ctx) {

  ZITI_NODEJS_LOG(INFO, "on_listen_client: client: %p, status: %d, clt_ctx: %p", client, status, clt_ctx);

  napi_status nstatus;

  ListenAddonData* addon_data = (ListenAddonData*) ziti_conn_data(serv);

  if (status == ZITI_OK) {

    ziti_conn_set_data(client, addon_data);

    const char *source_identity = clt_ctx->caller_id;
    if (source_identity != NULL) {
        ZITI_NODEJS_LOG(DEBUG, "on_listen_client: incoming connection from '%s'", source_identity );
    }
    else {
        ZITI_NODEJS_LOG(DEBUG, "on_listen_client: incoming connection from unidentified client" );
    }
    if (clt_ctx->app_data != NULL) {
        ZITI_NODEJS_LOG(DEBUG, "on_listen_client: got app data '%.*s'!", (int) clt_ctx->app_data_sz, clt_ctx->app_data );
    }

    ziti_accept(client, on_listen_client_connect, on_listen_client_data);
  
  } else {
    ZITI_NODEJS_LOG(DEBUG, "on_listen_client: failed to accept client: %s(%d)\n", ziti_errorstr(status), status );
  }

  OnClientItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));
  item->status = status;
  item->js_arb_data = addon_data->js_arb_data;
  item->client = client;
  item->caller_id = strdup(clt_ctx->caller_id);
  if (NULL != clt_ctx->app_data) {
    item->app_data_sz = clt_ctx->app_data_sz;
    item->app_data = calloc(1, clt_ctx->app_data_sz + 1);
    memcpy((void*)item->app_data, clt_ctx->app_data, clt_ctx->app_data_sz);
  }

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
void on_listen(ziti_connection serv, int status) {
  napi_status nstatus;

  ListenAddonData* addon_data = (ListenAddonData*) ziti_conn_data(serv);

  if (status == ZITI_OK) {
    ZITI_NODEJS_LOG(DEBUG, "on_listen: successfully bound to service[%s]", addon_data->service_name );
  }
  else {
    ZITI_NODEJS_LOG(DEBUG, "on_listen: failed to bind to service[%s]\n", addon_data->service_name );
    ziti_close(serv, NULL);
  }

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_listen,
      (void*)(int64_t)status,   // Send the status over to the JS callback
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
  size_t argc = 6;
  napi_value args[6];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 6) {
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

  ListenAddonData* addon_data = memset(malloc(sizeof(*addon_data)), 0, sizeof(*addon_data));

  // Obtain (optional) arbitrary-data value
  int64_t js_arb_data;
  status = napi_get_value_int64(env, args[1], &js_arb_data);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get js_arb_data");
  }
  addon_data->js_arb_data = js_arb_data;
  ZITI_NODEJS_LOG(DEBUG, "js_arb_data: %lld", js_arb_data);

  // Obtain ptr to JS 'on_listen' callback function
  napi_value js_on_listen = args[2];
  napi_value work_name_listen;

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
  napi_value js_on_listen_client = args[3];
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

  // Obtain ptr to JS 'on_listen_client_connect' callback function
  napi_value js_on_listen_client_connect = args[4];

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_listen_client_connect",
    NAPI_AUTO_LENGTH,
    &work_name_client);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
      env,
      js_on_listen_client_connect,
      NULL,
      work_name_client,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_listen_client_connect,
      &(addon_data->tsfn_on_listen_client_connect));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_threadsafe_function");
  }

  // Obtain ptr to JS 'on_listen_client_data' callback function
  napi_value js_on_listen_client_data = args[5];

  // Create a string to describe this asynchronous operation.
  status = napi_create_string_utf8(
    env,
    "N-API on_listen_client_data",
    NAPI_AUTO_LENGTH,
    &work_name_client);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_string_utf8");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  status = napi_create_threadsafe_function(
      env,
      js_on_listen_client_data,
      NULL,
      work_name_client,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_listen_client_data,
      &(addon_data->tsfn_on_listen_client_data));
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to napi_create_threadsafe_function");
  }

  // Init a Ziti connection object, and attach our add-on data to it so we can 
  // pass context around between our callbacks, as propagate it all the way out
  // to the JavaScript callbacks
  addon_data->service_name = strdup(ServiceName);
  int rc = ziti_conn_init(ztx, &addon_data->server, addon_data);
  if (rc != ZITI_OK) {
    napi_throw_error(env, NULL, "failure in 'ziti_conn_init");
  }

  // Start listening
  ZITI_NODEJS_LOG(DEBUG, "calling ziti_listen_with_options: %p, addon_data: %p", ztx, addon_data);
  ziti_listen_opts listen_opts = {
    .bind_using_edge_identity = false,
  };

  ziti_listen_with_options(addon_data->server, ServiceName, &listen_opts, on_listen, on_listen_client);
  // ziti_listen_with_options(addon_data->server, ServiceName, NULL, on_listen, on_listen_client);

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

