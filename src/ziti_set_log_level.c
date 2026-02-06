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
#include <stc/cstr.h>

struct log_msg {
    int level;
    cstr source;
    cstr msg;
};

static napi_threadsafe_function logger_tsfn = NULL;

/**
 * 
 */
static napi_value set_log_level(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  if (argc < 1) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  int64_t js_log_level;
  NAPI_CHECK(env, "get level", napi_get_value_int64(env, args[0], &js_log_level));

  ZITI_NODEJS_LOG(DEBUG, "js_log_level: %lld", (long long)js_log_level);

  ziti_log_set_level((int)js_log_level, NULL);

  NAPI_UNDEFINED(env, jsRetval);
  return jsRetval;
}

static void log_msg_drop(struct log_msg *m) {
  cstr_drop(&m->source);
  cstr_drop(&m->msg);
  free(m);
}

static void js_logger_cb(napi_env env, napi_value js_cb, void* context, void* data) {
  struct log_msg *m = data;

  if (env != NULL) {
    NAPI_UNDEFINED(env, undefined);
    NAPI_GLOBAL(env, global);
    napi_value proto;
    NAPI_CHECK(env, "get prototype", napi_get_prototype(env, global, &proto));

    napi_value names[3];
    napi_value values[3];

    napi_create_string_utf8(env, "level", NAPI_AUTO_LENGTH, &names[0]);
    napi_create_string_utf8(env, "source", NAPI_AUTO_LENGTH, &names[1]);
    napi_create_string_utf8(env, "message", NAPI_AUTO_LENGTH, &names[2]);

    napi_create_int32(env, m->level, &values[0]);
    napi_create_string_utf8(env, cstr_str(&m->source), NAPI_AUTO_LENGTH, &values[1]);
    napi_create_string_utf8(env, cstr_str(&m->msg), NAPI_AUTO_LENGTH, &values[2]);

    // Prepare the argument for the JavaScript callback function.
    napi_value js_log_msg;
    NAPI_CHECK(env, "create log message",
               napi_create_object(env, &js_log_msg));
    for (int i = 0; i < 3; i++) {
      NAPI_CHECK(env, "set log message property",
                 napi_set_property(env, js_log_msg, names[i], values[i]));
    }

    // Call the JavaScript callback function.
    napi_value result;
    NAPI_CHECK(env, "call logger", napi_call_function(env, undefined, js_cb, 1, &js_log_msg, &result));
  }

  // Free the log message allocated in the worker thread.
  log_msg_drop(m);
}

static void js_log_writer(int level, const char *loc, const char *msg, size_t msg_len) {
  struct log_msg *m = calloc(1, sizeof(*m));
  m->level = level;
  cstr_assign(&m->source, loc);
  cstr_assign_n(&m->msg, msg, (int)msg_len);

  if (napi_call_threadsafe_function(logger_tsfn, m, napi_tsfn_nonblocking) == napi_queue_full) {
    log_msg_drop(m);
  }
}

static napi_value set_logger(napi_env env, napi_callback_info info) {
  napi_value jsRetval;

  size_t argc = 1;
  napi_value args[1];
  NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  if (argc < 1) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  napi_valuetype arg_type = napi_undefined;
  NAPI_CHECK(env, "get arg type", napi_typeof(env, args[0], &arg_type));
  if (arg_type == napi_undefined) {
    // unset logger
    if (logger_tsfn != NULL) {
      napi_release_threadsafe_function(logger_tsfn, napi_tsfn_release);
      logger_tsfn = NULL;
    }
    ziti_log_set_logger(NULL);
  } else {
    if (arg_type != napi_function) {
      napi_throw_error(env, NULL, "Argument must be a function");
    }
    if (logger_tsfn != NULL) {
      napi_release_threadsafe_function(logger_tsfn, napi_tsfn_release);
    }
    napi_value logger_name;
    NAPI_CHECK(env, "create logger name",
               napi_create_string_utf8(env, "ZitiLogger", NAPI_AUTO_LENGTH, &logger_name));

    NAPI_CHECK(env, "create threadsafe func",
               napi_create_threadsafe_function(env, args[0], NULL, logger_name, 0, 1,
                                               NULL, NULL, NULL, js_logger_cb, &logger_tsfn));

    napi_unref_threadsafe_function(env, logger_tsfn);
    ziti_log_set_logger(js_log_writer);
    ZITI_LOG(INFO, "Ziti logger set");
  }

  NAPI_CHECK(env, "get undefined", napi_get_undefined(env, &jsRetval));
  return jsRetval;
}

ZNODE_EXPOSE(ziti_set_log_level, set_log_level)
ZNODE_EXPOSE(ziti_set_logger, set_logger)