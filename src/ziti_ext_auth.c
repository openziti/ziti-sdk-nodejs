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

/**
 * Provide an external JWT token to authenticate with the Ziti Controller.
 *
 * This is a synchronous wrapper for ziti_ext_auth_token().
 *
 * @param env The NAPI environment
 * @param info The callback info containing the JWT string argument
 * @return napi_value containing the status code (0 = success, ZITI_INVALID_STATE = -22 if not initialized)
 */
static napi_value _ziti_ext_auth_token(napi_env env, const napi_callback_info info) {
    napi_status status;
    napi_value jsRetval;
    int rc;

    ZITI_NODEJS_LOG(DEBUG, "ziti_ext_auth_token called");

    // Check if context is initialized
    if (ztx == NULL) {
        ZITI_NODEJS_LOG(ERROR, "ziti context not initialized");
        NAPI_CHECK(env, "create return value", napi_create_int32(env, ZITI_INVALID_STATE, &jsRetval));
        return jsRetval;
    }

    // Parse arguments
    size_t argc = 1;
    napi_value args[1];
    NAPI_CHECK(env, "parse args", napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 1) {
        napi_throw_error(env, "EINVAL", "JWT token argument is required");
        return NULL;
    }

    // Verify argument is a string
    napi_valuetype valuetype;
    status = napi_typeof(env, args[0], &valuetype);
    if (status != napi_ok || valuetype != napi_string) {
        napi_throw_error(env, "EINVAL", "JWT token must be a string");
        return NULL;
    }

    // Get the JWT string length
    size_t jwt_len;
    status = napi_get_value_string_utf8(env, args[0], NULL, 0, &jwt_len);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get JWT token length");
        return NULL;
    }

    if (jwt_len == 0) {
        napi_throw_error(env, "EINVAL", "JWT token must not be empty");
        return NULL;
    }

    // Allocate and copy the JWT string
    char *jwt = calloc(1, jwt_len + 1);
    if (jwt == NULL) {
        napi_throw_error(env, "ENOMEM", "Failed to allocate memory for JWT token");
        return NULL;
    }

    size_t copied;
    status = napi_get_value_string_utf8(env, args[0], jwt, jwt_len + 1, &copied);
    if (status != napi_ok) {
        free(jwt);
        napi_throw_error(env, "EINVAL", "Failed to copy JWT token");
        return NULL;
    }

    ZITI_NODEJS_LOG(DEBUG, "calling ziti_ext_auth_token with jwt of length %zu", jwt_len);

    // Call the C SDK function
    rc = ziti_ext_auth_token(ztx, jwt);

    ZITI_NODEJS_LOG(DEBUG, "ziti_ext_auth_token returned %d", rc);

    free(jwt);

    NAPI_CHECK(env, "create return value", napi_create_int32(env, rc, &jsRetval));
    return jsRetval;
}

ZNODE_EXPOSE(ziti_ext_auth_token, _ziti_ext_auth_token)
