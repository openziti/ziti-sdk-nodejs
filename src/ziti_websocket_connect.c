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
#include <ziti/ziti_src.h>
#include <string.h>

// An item that will be generated here and passed into the JavaScript on_data callback
typedef struct OnWsDataItem {

  uint8_t *buf;
  int len;

} OnWsDataItem;

static const unsigned int U1 = 1;

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
    napi_value undefined, js_ws;

    ZITI_NODEJS_LOG(INFO, "data: %p", data);

    // Convert the websocket to a napi_value.
    if (NULL != data) {

      // Retrieve the websocket from the item created by the worker thread.
      tlsuv_websocket_t* websocket = (tlsuv_websocket_t*)data;

      status = napi_create_int64(env, (int64_t)websocket, &js_ws);
      if (status != napi_ok) {
        napi_throw_error(env, NULL, "Unable to napi_create_int64");
      }

    } else {

      status = napi_get_undefined(env, &js_ws);
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
        &js_ws,
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

  // Retrieve the OnWsDataItem created by the worker thread.
  OnWsDataItem* item = (OnWsDataItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env != NULL) {
    napi_value undefined, js_buffer;
    void* result_data;

    // const obj = {}
    napi_value js_write_item, js_len;
    status = napi_create_object(env, &js_write_item);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_object");
    }

    // obj.len = len
    ZITI_NODEJS_LOG(DEBUG, "len=%d", item->len);
    status = napi_create_int64(env, (int64_t)item->len, &js_len);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_create_int64");
    }
    status = napi_set_named_property(env, js_write_item, "len", js_len);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_set_named_property");
    }

    // obj.data = buf
    if (item->buf) {
      status = napi_create_buffer_copy(env,
                                item->len,
                                (const void*)item->buf,
                                (void**)&result_data,
                                &js_buffer);
      if (status != napi_ok) {
        napi_throw_error(env, NULL, "Unable to napi_create_buffer_copy");
      }
      status = napi_set_named_property(env, js_write_item, "data", js_buffer);
      if (status != napi_ok) {
        napi_throw_error(env, NULL, "Unable to napi_set_named_property");
      }
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
        &js_write_item,
        NULL
      );
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to napi_call_function");
    }
  }

  if (item->buf) {
    free(item->buf);
  }
  free(item);
}


static void on_connect(uv_connect_t *req, int status) {
  ZITI_NODEJS_LOG(DEBUG, "=========on_connect: req: %p, status: %d", req, status);
  tlsuv_websocket_t *ws = (tlsuv_websocket_t *) req->handle;
  WSAddonData* addon_data = (WSAddonData*) ws->data;
  ZITI_NODEJS_LOG(DEBUG, "websocket: %p, addon_data: %p", ws, addon_data);

  tlsuv_websocket_t* the_websocket = NULL;

  if (status == ZITI_OK) {
    // Save the 'websocket' to the heap. The JavaScript marshaller (CallJs)
    // will free this item after having sent it to JavaScript.
    the_websocket = malloc(sizeof(*the_websocket));
    the_websocket = ws;
  }

  // Initiate the call into the JavaScript callback. 
  // The call into JavaScript will not have happened 
  // when this function returns, but it will be queued.
  napi_status nstatus = napi_call_threadsafe_function(
      addon_data->tsfn_on_connect,
      the_websocket,   // Send the websocket over to the JS callback
      napi_tsfn_blocking);
    if (nstatus != napi_ok) {
      ZITI_NODEJS_LOG(ERROR, "Unable to napi_call_threadsafe_function");
    }
}


static void on_ws_read(uv_stream_t *stream, ssize_t status, const uv_buf_t *buf) {
  tlsuv_websocket_t *ws = (tlsuv_websocket_t*)stream;
  ZITI_NODEJS_LOG(DEBUG, "=========on_ws_read: ws: %p, status: %zd", stream, status);
  WSAddonData* addon_data = (WSAddonData*) ws->data;
  ZITI_NODEJS_LOG(DEBUG, "=========on_ws_read: ws: %p, addon_data: %p", ws, addon_data);

  OnWsDataItem* item = memset(malloc(sizeof(*item)), 0, sizeof(*item));

  if (status < 0) {
    ZITI_NODEJS_LOG(ERROR, "status: %zd", status);

    item->len = status;
  
  } else {

    ZITI_NODEJS_LOG(DEBUG, "<-------- %.*s", (int)status, buf->base);

    item->buf = calloc(1, status);
    memcpy(item->buf, buf->base, status);
    item->len = status;
  
  }

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
}


/**
 * Initiate an HTTPS request
 * 
 * @param {string}   [0] url
 * @param {string[]} [1] headers;                  Array of strings of the form "name:value"
 * @param {func}     [2] JS on_connect callback;   This is invoked from 'on_connect' function above
 * @param {func}     [3] JS on_data callback;      This is invoked from 'on_ws_read' function above
 * 
 */
napi_value _ziti_websocket_connect(napi_env env, const napi_callback_info info) {

  napi_status status;
  size_t result;
  size_t argc = 4;
  napi_value args[4];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 3) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // Obtain url length
  size_t url_len;
  status = napi_get_value_string_utf8(env, args[0], NULL, 0, &url_len);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "url is not a string");
  }
  // Obtain url
  char* url = calloc(1, url_len+2);
  status = napi_get_value_string_utf8(env, args[0], url, url_len+1, &result);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to obtain url");
  }

  ZITI_NODEJS_LOG(DEBUG, "url: %s", url);

  WSAddonData* addon_data = memset(malloc(sizeof(*addon_data)), 0, sizeof(*addon_data));

  struct tlsuv_url_s url_parse = {0};
  int rc = tlsuv_parse_url(&url_parse, url);
  if (rc != 0) {
    napi_throw_error(env, "EINVAL", "Failed to parse url");
  }

  if (url_parse.hostname) {
    addon_data->service = strndup(url_parse.hostname, url_parse.hostname_len);
  }
  else {
    ZITI_NODEJS_LOG(ERROR, "invalid URL: no host");
    napi_throw_error(env, "EINVAL", "invalid URL: no host");
  }

  ZITI_NODEJS_LOG(DEBUG, "service: %s", addon_data->service);

  // Obtain ptr to JS 'connect' callback function
  napi_value js_wsect_cb = args[2];
  napi_value work_name_connect;



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
      js_wsect_cb,
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

  //
  // Capture headers
  //
  uint32_t i;
  status = napi_get_array_length(env, args[1], &addon_data->headers_array_length);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to obtain headers array");
  }
  ZITI_NODEJS_LOG(DEBUG, "headers_array_length: %d", addon_data->headers_array_length);
  for (i = 0; i < addon_data->headers_array_length; i++) {

    napi_value headers_array_element;
    status = napi_get_element(env, args[1], i, &headers_array_element);
    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "Failed to obtain headers element");
    }

    // Obtain element length
    size_t element_len;
    status = napi_get_value_string_utf8(env, headers_array_element, NULL, 0, &element_len);
    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "header arry element is not a string");
    }
    ZITI_NODEJS_LOG(DEBUG, "element_len: %zd", element_len);

    if (element_len > 8*1024) {
      ZITI_NODEJS_LOG(ERROR, "skipping header element; length too long");
      break;
    }

    // Obtain element
    char* header_element = calloc(1, element_len+2);
    status = napi_get_value_string_utf8(env, headers_array_element, header_element, element_len+1, &result);
    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "Failed to obtain element");
    }
    ZITI_NODEJS_LOG(DEBUG, "header_element: %s", header_element);

    char * header_name = strtok(header_element, ":");
    if (NULL == header_name) {
      napi_throw_error(env, "EINVAL", "Failed to split header element");
    }
    char * header_value = strtok(NULL, ":");
    if (strlen(header_value) < 1) {
      napi_throw_error(env, "EINVAL", "Failed to split header element");
    }

    addon_data->header_name[i] = strdup(header_name);
    addon_data->header_value[i] = strdup(header_value);

    free(header_element);
  }

  // Crank up the websocket
  ziti_src_init(thread_loop, &(addon_data->ziti_src), addon_data->service, ztx );
  tlsuv_websocket_init_with_src(thread_loop, &(addon_data->ws), &(addon_data->ziti_src));

  // Add Cookies to request
  for (int i = 0; i < (int)addon_data->headers_array_length; i++) {
    tlsuv_http_req_header(addon_data->ws.req, addon_data->header_name[i], addon_data->header_value[i]);
    free(addon_data->header_name[i]);
    free(addon_data->header_value[i]);
  }

  addon_data->ws.data = addon_data;  // Pass our addon data around so we can eventually find our way back to the JS callback
  rc = tlsuv_websocket_connect(&(addon_data->req), &(addon_data->ws), url, on_connect, on_ws_read);
  if (rc != ZITI_OK) {
    napi_throw_error(env, NULL, "failure in 'ziti_conn_init");
  }

  return NULL;
}



/**
 * 
 */
void expose_ziti_websocket_connect(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _ziti_websocket_connect, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_ziti_websocket_connect");
  }

  status = napi_set_named_property(env, exports, "ziti_websocket_connect", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'ziti_websocket_connect");
  }

}

