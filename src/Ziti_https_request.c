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
 *  Number of hosts we can have client pools for
 */
enum { listMapCapacity = 50 };

/**
 *  Number of simultaneous HTTP requests we can have active against a single host before queueing
 */
enum { perKeyListMapCapacity = 25 };

struct ListMap* HttpsClientListMap;

struct key_value {
    char* key;
    void* value;
};

struct ListMap {
  struct   key_value kvPairs[listMapCapacity];
  size_t   count;
  uv_sem_t sem;
};

struct ListMap* newListMap() {
    struct ListMap* listMap = calloc(1, sizeof *listMap);
    return listMap;
}

uv_mutex_t client_pool_lock;

bool listMapInsert(struct ListMap* collection, char* key, void* value) {

  if (collection->count == listMapCapacity) {
    ZITI_NODEJS_LOG(ERROR, "collection->count already at capacity [%d], insert FAIL", listMapCapacity);
    return false;
  }

  collection->kvPairs[collection->count].key = strdup(key);
  collection->kvPairs[collection->count].value = value;
  collection->count++;
  ZITI_NODEJS_LOG(DEBUG, "collection->count total is: [%zd]", collection->count);

  return true;
}


struct ListMap* getInnerListMapValueForKey(struct ListMap* collection, char* key) {
  struct ListMap* value = NULL;
  for (size_t i = 0 ; i < collection->count && value == NULL ; ++i) {
    if (strcmp(collection->kvPairs[i].key, key) == 0) {
      value = collection->kvPairs[i].value;
    }
  }
  return value;
}

HttpsClient* getHttpsClientForKey(struct ListMap* collection, char* key) {
  HttpsClient* value = NULL;
  int busyCount = 0;
  for (size_t i = 0 ; i < collection->count && value == NULL ; ++i) {
    if (strcmp(collection->kvPairs[i].key, key) == 0) {
      value = collection->kvPairs[i].value;
      if (value->active) {      // if it's in use
        value = NULL;           //  then keep searching
      }
      else if (value->purge) {  // if it's broken
        value = NULL;           //  then keep searching
      } else {
        value->active = true;   // mark the one we will return as 'in use
      }
      busyCount++;
    }
  }
  ZITI_NODEJS_LOG(DEBUG, "returning value '%p', collection->count is: [%zd], busy-count is: [%d]", value, collection->count, busyCount);
  return value;
}

void freeListMap(struct ListMap* collection) {
    if (collection == NULL) {
        return;
    }
    for (size_t i = 0 ; i < collection->count ; ++i) {
        free(collection->kvPairs[i].value);
    }
    free(collection);
}

/**
 * 
 */
static int purge_and_replace_bad_clients(struct ListMap* clientListMap, HttpsAddonData* addon_data) {
  int numReplaced = 0;
  for (int i = 0; i < perKeyListMapCapacity; i++) {

    HttpsClient* httpsClient = clientListMap->kvPairs[i].value;

    if (httpsClient->purge) {

      ZITI_NODEJS_LOG(DEBUG, "*********** purging client [%p] from slot [%d]", httpsClient, i);

      // FIXME: find out why the following free causes things to eventually crash in the uv_loop
      // free(httpsClient->scheme_host_port);
      // free(httpsClient);

      httpsClient = calloc(1, sizeof *httpsClient);
      httpsClient->scheme_host_port = strdup(addon_data->scheme_host_port);
      ziti_src_init(thread_loop, &(httpsClient->ziti_src), addon_data->service, ztx );
      um_http_init_with_src(thread_loop, &(httpsClient->client), addon_data->scheme_host_port, (um_src_t *)&(httpsClient->ziti_src) );

      clientListMap->kvPairs[i].value = httpsClient;

      ZITI_NODEJS_LOG(DEBUG, "*********** new client [%p] now occupying slot [%d]", httpsClient, i);

      numReplaced++;
    }
  }
  return numReplaced;
}

/**
 * 
 */
static void allocate_client(uv_work_t* req) {

  ZITI_NODEJS_LOG(DEBUG, "allocate_client() entered, uv_work_t is: %p", req);

  HttpsAddonData* addon_data = (HttpsAddonData*) req->data;

  ZITI_NODEJS_LOG(DEBUG, "addon_data is: %p", addon_data);

  // if (uv_mutex_trylock(&client_pool_lock)) {
  //   ZITI_NODEJS_LOG(ERROR, "uv_mutex_lock failure");
  // }

  struct ListMap* clientListMap = getInnerListMapValueForKey(HttpsClientListMap, addon_data->scheme_host_port);

  if (NULL == clientListMap) { // If first time seeing this key, spawn a pool of clients for it

    clientListMap = newListMap();
    listMapInsert(HttpsClientListMap, addon_data->scheme_host_port, (void*)clientListMap);

    uv_sem_init(&(clientListMap->sem), perKeyListMapCapacity);

    for (int i = 0; i < perKeyListMapCapacity; i++) {

      HttpsClient* httpsClient = calloc(1, sizeof *httpsClient);
      httpsClient->scheme_host_port = strdup(addon_data->scheme_host_port);
      ziti_src_init(thread_loop, &(httpsClient->ziti_src), addon_data->service, ztx );
      um_http_init_with_src(thread_loop, &(httpsClient->client), addon_data->scheme_host_port, (um_src_t *)&(httpsClient->ziti_src) );

      listMapInsert(clientListMap, addon_data->scheme_host_port, (void*)httpsClient);

    }
  }
  else {

    purge_and_replace_bad_clients(clientListMap, addon_data);

  }

  ZITI_NODEJS_LOG(DEBUG, "----------> acquiring sem");
  uv_sem_wait(&(clientListMap->sem));
  ZITI_NODEJS_LOG(DEBUG, "----------> successfully acquired sem");

  addon_data->httpsClient = getHttpsClientForKey(clientListMap, addon_data->scheme_host_port);
  ZITI_NODEJS_LOG(DEBUG, "----------> client is: [%p]", addon_data->httpsClient);

  if (NULL == addon_data->httpsClient) {
    ZITI_NODEJS_LOG(DEBUG, "----------> client is NULL, so we must purge_and_replace_bad_clients");
    purge_and_replace_bad_clients(clientListMap, addon_data);

    addon_data->httpsClient = getHttpsClientForKey(clientListMap, addon_data->scheme_host_port);
    ZITI_NODEJS_LOG(DEBUG, "----------> client is: [%p]", addon_data->httpsClient);
  }

  if (NULL == addon_data->httpsClient) {
    ZITI_NODEJS_LOG(DEBUG, "----------> client is NULL, so we are in an unrecoverable state!");
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
  ZITI_NODEJS_LOG(DEBUG, "addon_data->httpsClient is: %p", addon_data->httpsClient);

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

  if ((NULL == body) && (UV_EOF == len)) {
    ZITI_NODEJS_LOG(DEBUG, "<--------- returning httpsClient [%p] back to pool", addon_data->httpsClient);
    addon_data->httpsClient->active = false;

    struct ListMap* clientListMap = getInnerListMapValueForKey(HttpsClientListMap, addon_data->scheme_host_port);

    ZITI_NODEJS_LOG(DEBUG, "<-------- returning sem for client: [%p] ", addon_data->httpsClient);
    uv_sem_post(&(clientListMap->sem));
    ZITI_NODEJS_LOG(DEBUG, "          after returning sem for client: [%p] ", addon_data->httpsClient);
  }

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
  ZITI_NODEJS_LOG(DEBUG, "resp: %p", resp);

  HttpsAddonData* addon_data = (HttpsAddonData*) data;
  ZITI_NODEJS_LOG(DEBUG, "addon_data->httpsReq is: %p", addon_data->httpsReq);

  addon_data->httpsReq->on_resp_has_fired = true;
  addon_data->httpsReq->respCode = resp->code;

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
  LIST_FOREACH(h, &resp->headers, _next) {
    header_cnt++;
  }
  ZITI_NODEJS_LOG(DEBUG, "header_cnt: %d", header_cnt);

  item->headers = calloc(header_cnt + 1, sizeof(um_http_hdr));
  header_cnt = 0;
  LIST_FOREACH(h, &resp->headers, _next) {
    item->headers[header_cnt].name = strdup(h->name);
    item->headers[header_cnt].value = strdup(h->value);
    ZITI_NODEJS_LOG(DEBUG, "item->headers[%d]: %s : %s", header_cnt, item->headers[header_cnt].name, item->headers[header_cnt].value);
    header_cnt++;
  }

  if ((UV_EOF == resp->code) || (resp->code < 0)) {
    ZITI_NODEJS_LOG(ERROR, "<--------- returning httpsClient [%p] back to pool due to error: [%d]", addon_data->httpsClient, resp->code);
    addon_data->httpsClient->active = false;

    // Before we fully release this client (via semaphore post) let's indicate purge is needed, because after errs happen on a client, 
    // subsequent requests using that client never get processed.
    ZITI_NODEJS_LOG(DEBUG, "*********** due to error, purge now necessary for client: [%p]", addon_data->httpsClient);
    addon_data->httpsClient->purge = true;

    struct ListMap* clientListMap = getInnerListMapValueForKey(HttpsClientListMap, addon_data->scheme_host_port);

    ZITI_NODEJS_LOG(DEBUG, "<-------- returning sem for client: [%p] ", addon_data->httpsClient);
    uv_sem_post(&(clientListMap->sem));
    ZITI_NODEJS_LOG(DEBUG, "          after returning sem for client: [%p] ", addon_data->httpsClient);
  }

  // Initiate the call into the JavaScript callback. The call into JavaScript will not have happened when this function returns, but it will be queued.
  int rc = napi_call_threadsafe_function(
      addon_data->tsfn_on_resp,
      item,
      napi_tsfn_blocking);
  if (rc != napi_ok) {
    napi_throw_error(addon_data->env, "EINVAL", "failure to invoke JS callback");
  }

  if (UV_EOF != resp->code) {
    // We need body of the HTTP response, so wire up that callback now
    resp->body_cb = on_resp_body;
  }
}


/**
 * This function is responsible for calling the JavaScript on_resp callback function 
 * that was specified when the Ziti_https_request(...) was called from JavaScript.
 */
static void CallJs_on_req(napi_env env, napi_value js_cb, void* context, void* data) {

  ZITI_NODEJS_LOG(DEBUG, "entered");

  // This parameter is not used.
  (void) context;

  // Retrieve the addon_data created by the worker thread.
  HttpsAddonData* addon_data = (HttpsAddonData*) data;

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript
  if (env != NULL) {

    napi_value undefined;

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_get_undefined(env, &undefined);

    // const obj = {}
    napi_value js_http_item, js_req;
    int rc = napi_create_object(env, &js_http_item);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create object");
    }

    // obj.req = req
    napi_create_int64(env, (int64_t)addon_data->httpsReq, &js_req);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to create resp.req");
    }
    rc = napi_set_named_property(env, js_http_item, "req", js_req);
    if (rc != napi_ok) {
      napi_throw_error(env, "EINVAL", "failure to set named property req");
    }
    ZITI_NODEJS_LOG(DEBUG, "js_req: %p", addon_data->httpsReq);

    // Call the JavaScript function and pass it the req
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
void on_client(uv_work_t* req, int status) {

  ZITI_NODEJS_LOG(DEBUG, "on_client() entered, uv_work_t is: %p, status is: %d", req, status);

  HttpsAddonData* addon_data = (HttpsAddonData*) req->data;
  ZITI_NODEJS_LOG(DEBUG, "client is: [%p]", addon_data->httpsClient);

  // Initiate the request:   HTTP -> TLS -> Ziti -> Service 
  um_http_req_t *r = um_http_req(
    &(addon_data->httpsClient->client),
    addon_data->method,
    addon_data->path,
    on_resp,
    addon_data  /* Pass our addon data around so we can eventually find our way back to the JS callback */
  );

  ZITI_NODEJS_LOG(DEBUG, "req: %p", r);
  addon_data->httpsReq->req = r;

  // Add headers to request
  for (int i = 0; i < (int)addon_data->headers_array_length; i++) {
    um_http_req_header(r, addon_data->header_name[i], addon_data->header_value[i]);
    // free(addon_data->header_name[i]);
    // free(addon_data->header_value[i]);
  }

  // Initiate the call into the JavaScript callback. The call into JavaScript will not have happened when this function returns, but it will be queued.
  int rc = napi_call_threadsafe_function(
      addon_data->tsfn_on_req,
      addon_data,
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
 * @param {func}     [3] JS on_req  callback;      This is invoked from 'on_client' function above
 * @param {func}     [4] JS on_resp callback;      This is invoked from 'on_resp' function above
 * @param {func}     [5] JS on_resp_data callback; This is invoked from 'on_resp_data' function above
 * 
 * @returns {um_http_req_t} req  This allows the JS to subsequently write the Body to the request (see _Ziti_http_request_data)

 */
napi_value _Ziti_http_request(napi_env env, const napi_callback_info info) {

  napi_status status;
  size_t result;
  napi_value jsRetval;
  char* query = "";
  int rc;

  ZITI_NODEJS_LOG(DEBUG, "entered");

  if (NULL == HttpsClientListMap) {
    HttpsClientListMap = newListMap();
  }

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

  HttpsAddonData* addon_data = calloc(1, sizeof(HttpsAddonData));
  ZITI_NODEJS_LOG(DEBUG, "allocated addon_data : %p", addon_data);
  addon_data->env = env;

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
    addon_data->service = strndup(url + url_parse.field_data[UF_HOST].off, url_parse.field_data[UF_HOST].len);
  }
  else {
    ZITI_NODEJS_LOG(ERROR, "invalid URL: no host");
    napi_throw_error(env, "EINVAL", "invalid URL: no host");
  }

  ZITI_NODEJS_LOG(DEBUG, "service: %s", addon_data->service);

  if (url_parse.field_set & (U1 << (unsigned int) UF_PATH)) {
    addon_data->path = strndup(url + url_parse.field_data[UF_PATH].off, url_parse.field_data[UF_PATH].len);
  }
  else {
    ZITI_NODEJS_LOG(ERROR, "invalid URL: no path");
    napi_throw_error(env, "EINVAL", "invalid URL: no path");
  }

  ZITI_NODEJS_LOG(DEBUG, "path: [%s]", addon_data->path);

  if (url_parse.field_set & (U1 << (unsigned int) UF_QUERY)) {
    query = strndup(url + url_parse.field_data[UF_QUERY].off, url_parse.field_data[UF_QUERY].len);
    ZITI_NODEJS_LOG(DEBUG, "query: [%s]", query);
    char* expanded_path = calloc(1, strlen(addon_data->path)+strlen(query)+2);

    strcat(expanded_path, addon_data->path);
    strcat(expanded_path, "?");
    strcat(expanded_path, query);
    ZITI_NODEJS_LOG(DEBUG, "expanded_path: [%s]", expanded_path);
    free(addon_data->path);
    addon_data->path = expanded_path;
  }
  else {
    ZITI_NODEJS_LOG(DEBUG, "URL: no query found");
  }
  ZITI_NODEJS_LOG(DEBUG, "adjusted path: [%s]", addon_data->path);

  addon_data->scheme_host_port = strndup(url + url_parse.field_data[UF_SCHEMA].off, url_parse.field_data[UF_PATH].off);

  ZITI_NODEJS_LOG(DEBUG, "scheme_host_port: %s", addon_data->scheme_host_port);


  // Obtain method length
  size_t method_len;
  status = napi_get_value_string_utf8(env, args[1], NULL, 0, &method_len);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "method is not a string");
  }
  // Obtain method
  addon_data->method = calloc(1, method_len+2);
  status = napi_get_value_string_utf8(env, args[1], addon_data->method, method_len+1, &result);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to obtain method");
  }

  ZITI_NODEJS_LOG(DEBUG, "method: %s", addon_data->method);

  // Obtain ptr to JS on_req callback function
  napi_value js_cb = args[3];
  napi_value work_name;


  HttpsReq* httpsReq = calloc(1, sizeof(HttpsReq));
  addon_data->httpsReq = httpsReq;
  httpsReq->addon_data = addon_data;

  // Create a string to describe this asynchronous operation.
  rc = napi_create_string_utf8(env, "on_req", NAPI_AUTO_LENGTH, &work_name);
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
    CallJs_on_req,
    &(addon_data->tsfn_on_req)
  );
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create threadsafe_function");
  }
  ZITI_NODEJS_LOG(DEBUG, "napi_create_threadsafe_function addon_data->tsfn_on_req() : %p", addon_data->tsfn_on_req);

  // Obtain ptr to JS on_resp callback function
  napi_value js_cb2 = args[4];

  // Create a string to describe this asynchronous operation.
  rc = napi_create_string_utf8(env, "on_resp", NAPI_AUTO_LENGTH, &work_name);
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
    CallJs_on_resp,
    &(addon_data->tsfn_on_resp)
  );
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create threadsafe_function");
  }
  ZITI_NODEJS_LOG(DEBUG, "napi_create_threadsafe_function addon_data->tsfn_on_resp() : %p", addon_data->tsfn_on_resp);


  // Obtain ptr to JS on_resp_data callback function
  napi_value js_cb3 = args[5];

  // Create a string to describe this asynchronous operation.
  rc = napi_create_string_utf8(env, "on_resp_data", NAPI_AUTO_LENGTH, &work_name);
  if (rc != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to create string");
  }

  // Convert the callback retrieved from JavaScript into a thread-safe function (tsfn) 
  // which we can call from a worker thread.
  rc = napi_create_threadsafe_function(
    env,
    js_cb3,
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

  //
  // Capture headers
  //
  uint32_t i;
  status = napi_get_array_length(env, args[2], &addon_data->headers_array_length);
  if (status != napi_ok) {
    napi_throw_error(env, "EINVAL", "Failed to obtain headers array");
  }
  ZITI_NODEJS_LOG(DEBUG, "headers_array_length: %d", addon_data->headers_array_length);
  for (i = 0; i < addon_data->headers_array_length; i++) {

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
    ZITI_NODEJS_LOG(DEBUG, "element_len: %zd", element_len);

    if (element_len > 8*1024) {
      ZITI_NODEJS_LOG(DEBUG, "skipping header element; length too long");
      addon_data->httpsReq->on_resp_has_fired = true;
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

  //
  // Queue the HTTP request.  First thing that happens in the flow is to allocate a client from the pool
  //
  addon_data->uv_req.data = addon_data;
  uv_queue_work(thread_loop, &addon_data->uv_req, allocate_client, on_client);

  ZITI_NODEJS_LOG(DEBUG, "uv_queue_work of allocate_client returned req: %p", &(addon_data->uv_req));

  //
  // We always return zero.  The real results/status are returned via the multiple callbacks
  //
  status = napi_create_int64(env, (int64_t)0, &jsRetval);
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
