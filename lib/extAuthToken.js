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

/**
 * Provide an external JWT token to authenticate with the Ziti Controller.
 *
 * This function is used when the Ziti identity requires external authentication
 * (e.g., OIDC). The auth event callback provided to init() will be called with
 * the authentication details, and this function should be called with the JWT
 * obtained from the external identity provider.
 *
 * @param {string} jwtToken - The JWT token obtained from the external identity provider
 * @returns {number} 0 on success, -22 (ZITI_INVALID_STATE) if context not initialized
 * @throws {Error} If jwtToken is not a non-empty string
 */
const extAuthToken = (jwtToken) => {
    if (typeof jwtToken !== 'string' || jwtToken.length === 0) {
        throw new Error('JWT token must be a non-empty string');
    }
    return ziti.ziti_ext_auth_token(jwtToken);
};

exports.extAuthToken = extAuthToken;
