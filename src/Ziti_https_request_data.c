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


/**
 * This function is responsible for calling the JavaScript on_resp_data callback function 
 * that was specified when the Ziti_https_request_data(...) was called from JavaScript.
 */
static void CallJs_on_req_body(napi_env env, napi_value js_cb, void* context, void* data) {

  ZITI_NODEJS_LOG(DEBUG, "entered");

  // This parameter is not used.
  (void) context;

  // Retrieve the HttpsRespBodyItem created by the worker thread.
  HttpsReqBodyItem* item = (HttpsReqBodyItem*)data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    napi_value undefined;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_get_undefined(env, &undefined);

    // const obj = {}
    napi_value js_http_item, js_req, js_status, js_body;
    int rc = napi_create_object(env, &js_http_item);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }

    // obj.req = req
    napi_create_int64(env, (int64_t)item->req, &js_req);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.req");
    }
    rc = napi_set_named_property(env, js_http_item, "req", js_req);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property req");
    }
    ZITI_NODEJS_LOG(DEBUG, "js_req: %p", item->req);

    // obj.code = status
    rc = napi_create_int32(env, item->status, &js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.status");
    }
    rc = napi_set_named_property(env, js_http_item, "status", js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property status");
    }
    ZITI_NODEJS_LOG(DEBUG, "status: %zd", item->status);

    // obj.body = body
    rc = napi_create_int32(env, (int64_t)item->body, &js_body);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.body");
    }
    rc = napi_set_named_property(env, js_http_item, "body", js_body);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property body");
    }

    // Call the JavaScript function and pass it the HttpsRespItem
    rc = napi_call_function(
      env,
      undefined,
      js_cb,
      1,
      &js_http_item,
      NULL
    );
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to invoke JS callback");
    }

  }
}



/**
 * 
 */
void on_req_body(tlsuv_http_req_t *req, const char *body, ssize_t status) {

  ZITI_NODEJS_LOG(DEBUG, "status: %zd, body: %p", status, body);

  HttpsAddonData* addon_data = (HttpsAddonData*) req->data;
  ZITI_NODEJS_LOG(DEBUG, "addon_data is: %p", addon_data);

  HttpsReqBodyItem* item = calloc(1, sizeof(*item));
  ZITI_NODEJS_LOG(DEBUG, "new HttpsReqBodyItem is: %p", item);
  
  //  Grab everything off the tlsuv_http_resp_t that we need to eventually pass on to the JS on_resp_body callback.
  //  If we wait until CallJs_on_resp_body is invoked to do that work, the tlsuv_http_resp_t may have already been free'd by the C-SDK

  item->req = req;
  item->body = (void*)body;
  item->status = status;

  ZITI_NODEJS_LOG(DEBUG, "calling tsfn_on_req_body: %p", addon_data->tsfn_on_req_body);

  // Initiate the call into the JavaScript callback.
  int rc = napi_call_threadsafe_function(
      addon_data->tsfn_on_req_body,
      item,
      napi_tsfn_blocking);
  if (rc != napi_ok) {
    napi_throw_error(addon_data->env, "EINVAL", "failure to invoke JS callback");
  }
}



/**
 * Send Body data over active HTTPS request
 * 
 * @param {string} [0] req
 * @param {string} [1] data (we expect a Buffer)
 * @param {func}   [2] JS on_write callback;      This is invoked from 'on_req_body' function above
 * 
 * @returns NULL
 */
napi_value _Ziti_http_request_data(napi_env env, const napi_callback_info info) {
  napi_status status;
  int rc;
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

  // Obtain tlsuv_http_req_t
  int64_t js_req;
  status = napi_get_value_int64(env, args[0], &js_req);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Req");
  }
  // tlsuv_http_req_t *r = (tlsuv_http_req_t*)js_req;
  HttpsReq* httpsReq = (HttpsReq*)js_req;
  tlsuv_http_req_t *r = httpsReq->req;

  ZITI_NODEJS_LOG(DEBUG, "req: %p", r);

  HttpsAddonData* addon_data = httpsReq->addon_data;
  ZITI_NODEJS_LOG(DEBUG, "addon_data is: %p", addon_data);

  // If some kind of Ziti error previously occured on this request, then short-circuit now
  if (httpsReq->on_resp_has_fired && (httpsReq->respCode < 0)) {
    ZITI_NODEJS_LOG(DEBUG, "aborting due to previous error: %d", httpsReq->respCode);
    return NULL;
  }

  // Obtain data to write (we expect a Buffer)
  void*  buffer;
  size_t bufferLength;
  status = napi_get_buffer_info(env, args[1], &buffer, &bufferLength);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get Buffer info");
  }
  ZITI_NODEJS_LOG(DEBUG, "bufferLength: %zd", bufferLength);

  // Since the underlying Buffer's lifetime is not guaranteed if it's managed by the VM, we will copy the chunk into our heap
  void* chunk = calloc(1, bufferLength + 1);
  memcpy(chunk, buffer, bufferLength);

  // Obtain ptr to JS 'on_write' callback function
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
  rc = napi_create_threadsafe_function(
      env,
      js_write_cb,
      NULL,
      work_name,
      0,
      1,
      NULL,
      NULL,
      NULL,
      CallJs_on_req_body,
      &(addon_data->tsfn_on_req_body)
  );
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create threadsafe_function");
  }
  ZITI_NODEJS_LOG(DEBUG, "napi_create_threadsafe_function addon_data->tsfn_on_req_body() : %p", addon_data->tsfn_on_req_body);

  // Now, call the C-SDK to actually write the data over to the service
  tlsuv_http_req_data(r, chunk, bufferLength, on_req_body );

  return NULL;
}


void expose_ziti_https_request_data(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _Ziti_http_request_data, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_Ziti_http_request_data");
  }

  status = napi_set_named_property(env, exports, "Ziti_http_request_data", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'Ziti_http_request_data");
  }

}
