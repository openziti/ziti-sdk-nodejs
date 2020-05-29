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
#include <ziti/ziti_src.h>

static const unsigned int U1 = 1;



/**
 * This function is responsible for calling the JavaScript on_resp callback function 
 * that was specified when the Ziti_https_request(...) was called from JavaScript.
 */
static void CallJs_on_resp(napi_env env, napi_value js_cb, void* context, void* data) {

  ZITI_NODEJS_LOG(DEBUG, "entered");

  // This parameter is not used.
  (void) context;

  // Retrieve the HttpsRespItem created by the worker thread.
  HttpsRespItem* item = (HttpsRespItem*)data;
  ZITI_NODEJS_LOG(DEBUG, "HttpsRespItem is: %p", item);


  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    napi_value undefined;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_get_undefined(env, &undefined);

    // const obj = {}
    napi_value js_http_item, js_req, js_code, js_status;
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

    // obj.code = code
    rc = napi_create_int32(env, item->code, &js_code);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.code");
    }
    rc = napi_set_named_property(env, js_http_item, "code", js_code);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property code");
    }
    ZITI_NODEJS_LOG(DEBUG, "code: %d", item->code);

    // obj.status = status
    rc = napi_create_string_utf8(env, (const char*)item->status, strlen(item->status), &js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.status");
    }
    rc = napi_set_named_property(env, js_http_item, "status", js_status);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property status");
    }
    ZITI_NODEJS_LOG(DEBUG, "status: %s", item->status);

    // // const headers_array = []
    // napi_value js_headers_array, js_header_value;

    // rc = napi_create_array(env, &js_headers_array);
    // if (rc != napi_ok) {
    //   napi_throw_error(env, "EINVAL", "failure to create js_headers_array");
    // }
    // um_http_hdr *h;
    // int i = 0;
    // for (h = item->headers; h != NULL && h->name != NULL; h++) {
    //   ZITI_NODEJS_LOG(DEBUG, "Processing header: %p", h);
    //   ZITI_NODEJS_LOG(DEBUG, "\t%s: %s", h->name, h->value);

    //   char* tmp = calloc(1, strlen(h->name)+strlen(h->value)+2);
    //   strcat(tmp, h->name);
    //   strcat(tmp, "=");
    //   strcat(tmp, h->value);

    //   rc = napi_create_string_utf8(env, (const char*)tmp, strlen(tmp), &js_header_value);
    //   if (rc != napi_ok) {
    //     napi_throw_error(env, "EINVAL", "failure to create js_header_element");
    //   }
    //   free(tmp);
    //   rc = napi_set_element(env, js_headers_array, i++, js_header_value);
    //   if (rc != napi_ok) {
    //     napi_throw_error(env, "EINVAL", "failure to set js_header_element");
    //   }
    // }
    // ZITI_NODEJS_LOG(DEBUG, "Done with js_headers_array");

    // // obj.headers_array = headers_array
    // rc = napi_set_named_property(env, js_http_item, "headers_array", js_headers_array);
    // if (rc != napi_ok) {
    //   napi_throw_error(env, "EINVAL", "failure to set named property headers_array");
    // }

    // const headers = {}
    um_http_hdr *h;
    int i = 0;
    napi_value js_headers, js_header_value;
    napi_value js_cookies_array = NULL;

    rc = napi_create_object(env, &js_headers);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create js_headers object");
    }

    for (h = item->headers; h != NULL && h->name != NULL; h++) {
      ZITI_NODEJS_LOG(DEBUG, "Processing header: %p", h);
      ZITI_NODEJS_LOG(DEBUG, "\t%s: %s", h->name, h->value);

      if (strcasecmp(h->name, "set-cookie") == 0) {
        if (NULL == js_cookies_array) {
          rc = napi_create_array(env, &js_cookies_array);
          if (rc != napi_ok) {
            napi_throw_error(env, "EINVAL", "failure to create js_cookies_array");
          }
        }
        rc = napi_create_string_utf8(env, (const char*)h->value, strlen(h->value), &js_header_value);
        if (rc != napi_ok) {
          napi_throw_error(env, "EINVAL", "failure to create js_header_value");
        }
        rc = napi_set_element(env, js_cookies_array, i++, js_header_value);
        if (rc != napi_ok) {
          napi_throw_error(env, "EINVAL", "failure to set js_header_element");
        }
      } else {
        rc = napi_create_string_utf8(env, (const char*)h->value, strlen(h->value), &js_header_value);
        if (rc != napi_ok) {
          napi_throw_error(env, "EINVAL", "failure to create js_header_value");
        }
        rc = napi_set_named_property(env, js_headers, h->name, js_header_value);
        if (rc != napi_ok) {
          napi_throw_error(env, "EINVAL", "failure to set header");
        }
      }
    }
    if (NULL != js_cookies_array) {
      rc = napi_set_named_property(env, js_headers, "Set-Cookie", js_cookies_array);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set set-cookie header");
      }
    }

    ZITI_NODEJS_LOG(DEBUG, "Done with headers");

    // obj.headers = headers
    rc = napi_set_named_property(env, js_http_item, "headers", js_headers);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property headers");
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
void on_resp(um_http_resp_t *resp, void *data) {

  HttpsAddonData* addon_data = (HttpsAddonData*) data;

  addon_data->httpsReq->on_resp_has_fired = true; // TEMP

  ZITI_NODEJS_LOG(DEBUG, "addon_data is: %p", data);

  HttpsRespItem* item = calloc(1, sizeof(*item));
  ZITI_NODEJS_LOG(DEBUG, "new HttpsRespItem is: %p", item);
  
  //  Grab everything off the um_http_resp_t that we need to eventually pass on to the JS on_resp callback.
  //  If we wait until CallJs_on_resp is invoked to do that work, the um_http_resp_t may have already been free'd by the C-SDK

  item->req = resp->req;
  ZITI_NODEJS_LOG(DEBUG, "item->req: %p", item->req);

  item->code = resp->code;
  ZITI_NODEJS_LOG(DEBUG, "item->code: %d", item->code);

  item->status = strdup(resp->status);
  ZITI_NODEJS_LOG(DEBUG, "item->status: %s", item->status);

  int header_cnt = 0;
  um_http_hdr *h;
  for (h = resp->headers; h != NULL && h->name != NULL; h++) {
    header_cnt++;
  }
  ZITI_NODEJS_LOG(DEBUG, "header_cnt: %d", header_cnt);

  item->headers = calloc(header_cnt + 1, sizeof(um_http_hdr));
  header_cnt = 0;
  for (h = resp->headers; h != NULL && h->name != NULL; h++) {
    item->headers[header_cnt].name = strdup(h->name);
    item->headers[header_cnt].value = strdup(h->value);
    ZITI_NODEJS_LOG(DEBUG, "item->headers[%d]: %s : %s", header_cnt, item->headers[header_cnt].name, item->headers[header_cnt].value);
    header_cnt++;
  }

  // Initiate the call into the JavaScript callback. The call into JavaScript will not have happened when this function returns, but it will be queued.
  int rc = napi_call_threadsafe_function(
      addon_data->tsfn_on_resp,
      item,
      napi_tsfn_blocking);
  if (rc != napi_ok) {
    napi_throw_error(addon_data->env, "EINVAL", "failure to invoke JS callback");
  }

}


/**
 * This function is responsible for calling the JavaScript on_resp_body callback function 
 * that was specified when the Ziti_https_request(...) was called from JavaScript.
 */
static void CallJs_on_resp_body(napi_env env, napi_value js_cb, void* context, void* data) {

  ZITI_NODEJS_LOG(DEBUG, "entered");

  // This parameter is not used.
  (void) context;

  // Retrieve the HttpsRespBodyItem created by the worker thread.
  HttpsRespBodyItem* item = (HttpsRespBodyItem*)data;


  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    napi_value undefined;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_get_undefined(env, &undefined);

    // const obj = {}
    napi_value js_http_item, js_req, js_len, js_buffer;
    void* result_data;
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

    // obj.len = len
    rc = napi_create_int32(env, item->len, &js_len);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.len");
    }
    rc = napi_set_named_property(env, js_http_item, "len", js_len);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property len");
    }
    ZITI_NODEJS_LOG(DEBUG, "len: %zd", item->len);

    // obj.body = body
    if (NULL != item->body) {
      rc = napi_create_buffer_copy(env, item->len, (const void*)item->body, (void**)&result_data, &js_buffer);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to create resp.data");
      }
      rc = napi_set_named_property(env, js_http_item, "body", js_buffer);
      if (rc != napi_ok) {
        napi_throw_error(env, "EINVAL", "failure to set named property body");
      }
    } else {
      rc = napi_set_named_property(env, js_http_item, "body", undefined);
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
void on_resp_body(um_http_req_t *req, const char *body, ssize_t len) {

  // ZITI_NODEJS_LOG(DEBUG, "len: %zd, body is: \n>>>>>%s<<<<<", len, body);
  ZITI_NODEJS_LOG(DEBUG, "body: %p", body);
  ZITI_NODEJS_LOG(DEBUG, "len: %zd", len);

  HttpsAddonData* addon_data = (HttpsAddonData*) req->data;

  HttpsRespBodyItem* item = calloc(1, sizeof(*item));
  ZITI_NODEJS_LOG(DEBUG, "new HttpsRespBodyItem is: %p", item);
  
  //  Grab everything off the um_http_resp_t that we need to eventually pass on to the JS on_resp_body callback.
  //  If we wait until CallJs_on_resp_body is invoked to do that work, the um_http_resp_t may have already been free'd by the C-SDK

  item->req = req;

  if (NULL != body) {
    item->body = calloc(1, len);
    memcpy((void*)item->body, body, len);
  } else {
    item->body = NULL;
  }

  item->len = len;

  ZITI_NODEJS_LOG(DEBUG, "calling tsfn_on_resp_body: %p", addon_data->tsfn_on_resp_body);

  // Initiate the call into the JavaScript callback.
  int rc = napi_call_threadsafe_function(
      addon_data->tsfn_on_resp_body,
      item,
      napi_tsfn_blocking);
  if (rc != napi_ok) {
    napi_throw_error(addon_data->env, "EINVAL", "failure to invoke JS callback");
  }

}


/**
 * Initiate an HTTPS request
 * 
 * @param {string}   [0] url
 * @param {string}   [1] method
 * @param {string[]} [2] headers;                  Array of strings of the form "name:value"
 * @param {func}     [3] JS on_resp callback;      This is invoked from 'on_resp' function above
 * @param {func}     [4] JS on_resp_data callback; This is invoked from 'on_resp_data' function above
 * 
 * @returns {um_http_req_t} req  This allows the JS to subsequently write the Body to the request (see _Ziti_http_request_data)

 */
napi_value _Ziti_http_request(napi_env env, const napi_callback_info info) {

  napi_status status;
  size_t result;
  napi_value jsRetval;
  char* service;
  char* path = "";
  char* query = "";
  char* scheme_host_port;
  int rc;

  ZITI_NODEJS_LOG(DEBUG, "entered");

  size_t argc = 5;
  napi_value args[5];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
  }

  if (argc < 5) {
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

  struct http_parser_url url_parse = {0};
  rc = http_parser_parse_url(url, strlen(url), false, &url_parse);
  if (rc != 0) {
    napi_throw_error(env, "EINVAL", "Failed to parse url");
  }

  if (url_parse.field_set & (U1 << (unsigned int) UF_HOST)) {
    service = strndup(url + url_parse.field_data[UF_HOST].off, url_parse.field_data[UF_HOST].len);
  }
  else {
    ZITI_NODEJS_LOG(ERROR, "invalid URL: no host");
    napi_throw_error(env, "EINVAL", "invalid URL: no host");
  }

  ZITI_NODEJS_LOG(DEBUG, "service: %s", service);

  if (url_parse.field_set & (U1 << (unsigned int) UF_PATH)) {
    path = strndup(url + url_parse.field_data[UF_PATH].off, url_parse.field_data[UF_PATH].len);
  }
  else {
    ZITI_NODEJS_LOG(ERROR, "invalid URL: no path");
    napi_throw_error(env, "EINVAL", "invalid URL: no path");
  }

  ZITI_NODEJS_LOG(DEBUG, "path: [%s]", path);

  if (url_parse.field_set & (U1 << (unsigned int) UF_QUERY)) {
    query = strndup(url + url_parse.field_data[UF_QUERY].off, url_parse.field_data[UF_QUERY].len);
    ZITI_NODEJS_LOG(DEBUG, "query: [%s]", query);
    char* expanded_path = calloc(1, strlen(path)+strlen(query)+2);

    strcat(expanded_path, path);
    strcat(expanded_path, "?");
    strcat(expanded_path, query);
    ZITI_NODEJS_LOG(DEBUG, "expanded_path: [%s]", expanded_path);
    free(path);
    path = expanded_path;
  }
  else {
    ZITI_NODEJS_LOG(ERROR, "FYI, URL: no query found");
  }
  ZITI_NODEJS_LOG(DEBUG, "adjusted path: [%s]", path);

  scheme_host_port = strndup(url + url_parse.field_data[UF_SCHEMA].off, url_parse.field_data[UF_PATH].off);

  ZITI_NODEJS_LOG(DEBUG, "scheme_host_port: %s", scheme_host_port);


  // Obtain method length
  size_t method_len;
  status = napi_get_value_string_utf8(env, args[1], NULL, 0, &method_len);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "method is not a string");
  }
  // Obtain method
  char* method = calloc(1, method_len+2);
  status = napi_get_value_string_utf8(env, args[1], method, method_len+1, &result);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to obtain method");
  }

  ZITI_NODEJS_LOG(DEBUG, "method: %s", method);

  // Obtain ptr to JS on_resp callback function
  napi_value js_cb = args[3];
  napi_value work_name;

  HttpsAddonData* addon_data = calloc(1, sizeof(HttpsAddonData));
  ZITI_NODEJS_LOG(DEBUG, "allocated addon_data : %p", addon_data);

  addon_data->env = env;

  // Create a string to describe this asynchronous operation.
  rc = napi_create_string_utf8(env, "on_resp", NAPI_AUTO_LENGTH, &work_name);
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create string");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  rc = napi_create_threadsafe_function(
    env,
    js_cb,
    NULL,
    work_name,
    0,
    1,
    NULL,
    NULL,
    NULL,
    CallJs_on_resp,
    &(addon_data->tsfn_on_resp)
  );
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create threadsafe_function");
  }
  ZITI_NODEJS_LOG(DEBUG, "napi_create_threadsafe_function addon_data->tsfn_on_resp() : %p", addon_data->tsfn_on_resp);


  // Obtain ptr to JS on_resp_data callback function
  napi_value js_cb2 = args[4];

  // Create a string to describe this asynchronous operation.
  rc = napi_create_string_utf8(env, "on_resp_data", NAPI_AUTO_LENGTH, &work_name);
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create string");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  rc = napi_create_threadsafe_function(
    env,
    js_cb2,
    NULL,
    work_name,
    0,
    1,
    NULL,
    NULL,
    NULL,
    CallJs_on_resp_body,
    &(addon_data->tsfn_on_resp_body)
  );
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create threadsafe_function");
  }
  ZITI_NODEJS_LOG(DEBUG, "napi_create_threadsafe_function addon_data->tsfn_on_resp_body() : %p", addon_data->tsfn_on_resp_body);


  // Set up the Ziti source
  ziti_src_init(thread_loop, &(addon_data->ziti_src), service, ztx );
  um_http_init_with_src(
    thread_loop, 
    &(addon_data->client), 
    scheme_host_port, 
    (um_http_src_t *)&(addon_data->ziti_src) 
  );

  // Initiate the request:   HTTP -> TLS -> Ziti -> Service 
  um_http_req_t *r = um_http_req(
    &(addon_data->client),
    method,
    path,
    on_resp,
    addon_data  /* Pass our addon data around so we can eventually find our way back to the JS callback */
  );
  ZITI_NODEJS_LOG(DEBUG, "req: %p", r);

  // We need body of the HTTP response, so wire it up
  r->resp.body_cb = on_resp_body;

  // Add headers to request
  // Obtain headers array length
  uint32_t i, headers_array_length;
  status = napi_get_array_length(env, args[2], &headers_array_length);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to obtain headers array");
  }
  ZITI_NODEJS_LOG(DEBUG, "headers_array_length: %d", headers_array_length);
  // Process each header
  for (i = 0; i < headers_array_length; i++) {
    napi_value headers_array_element;
    status = napi_get_element(env, args[2], i, &headers_array_element);
    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "Failed to obtain headers element");
    }

    // Obtain element length
    size_t element_len;
    status = napi_get_value_string_utf8(env, headers_array_element, NULL, 0, &element_len);
    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "header arry element is not a string");
    }
    // ZITI_NODEJS_LOG(DEBUG, "element_len: %zd", element_len);

    // Obtain element
    char* header_element = calloc(1, element_len+2);
    status = napi_get_value_string_utf8(env, headers_array_element, header_element, element_len+1, &result);
    if (status != napi_ok) {
      napi_throw_error(env, "EINVAL", "Failed to obtain element");
    }
    // ZITI_NODEJS_LOG(DEBUG, "header_element: %s", header_element);

    char * header_name = strtok(header_element, ":");
    if (NULL == header_name) {
      napi_throw_error(env, "EINVAL", "Failed to split header element");
    }
    char * header_value = strtok(NULL, ":");
    if (strlen(header_value) < 1) {
      napi_throw_error(env, "EINVAL", "Failed to split header element");
    }
    ZITI_NODEJS_LOG(DEBUG, "header name: %s, value: %s", header_name, header_value);

    rc = um_http_req_header(r, header_name, header_value);
    ZITI_NODEJS_LOG(DEBUG, "um_http_req_header rc: %d", rc);
  }

  HttpsReq* httpsReq = calloc(1, sizeof(HttpsReq));
  httpsReq->req = r;

  addon_data->httpsReq = httpsReq;

  status = napi_create_int64(env, (int64_t)httpsReq, &jsRetval);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
  }

  return jsRetval;
}


void expose_ziti_https_request(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, _Ziti_http_request, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function '_Ziti_http_request");
  }

  status = napi_set_named_property(env, exports, "Ziti_http_request", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports for 'Ziti_http_request");
  }

}
