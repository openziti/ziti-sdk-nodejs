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
 * Initialize the Ziti session and authenticate with control plane.
 *
 * @param {string} identityPath - File system path to the identity file.
 * @param {function} [onAuthEvent] - Optional callback for authentication events.
 *   Called with an object containing { action, type, detail } when auth events occur.
 *   - action: 'login_external', 'select_external', 'cannot_continue', 'prompt_totp', 'prompt_pin'
 *   - type: The authentication type (e.g., 'oidc')
 *   - detail: Additional details (e.g., the OIDC provider URL)
 * @returns {Promise<void>} Resolves when initialization is complete.
 */
const init = ( identityPath, onAuthEvent ) => {

  return new Promise((resolve, reject) => {
      try {
          ziti.ziti_init( identityPath, ( result ) => {
              if (result instanceof Error) {
                  return reject(result);
              }
              return resolve( result );
          }, onAuthEvent);
      } catch (e) {
          reject(e);
      }
  });
};

const shutdown = () => {
    ziti.ziti_shutdown();
}

exports.init = init;
exports.shutdown = shutdown;
