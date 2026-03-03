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
 * Initialize external authentication with a Ziti controller.
 *
 * @param {string} controllerHost - The hostname of the Ziti controller (e.g., "ctrl.example.com:1280")
 * @param {string} token - The external authentication token
 * @returns {number} 0 on success, error code on failure
 */
const initExternalAuth = ( controllerHost, token ) => {

    if (typeof controllerHost !== 'string' || controllerHost.length === 0) {
        throw new Error('controllerHost must be a non-empty string');
    }
    if (typeof token !== 'string' || token.length === 0) {
        throw new Error('token must be a non-empty string');
    }

    return new Promise((resolve, reject) => {   

      try {
        ziti.ziti_init_external_auth( controllerHost, async ( cfg ) => {

            if (cfg instanceof Error) {
                return reject(cfg);
            }

            function onAuthEvent(event) {
    
                if (event.action === 'login_external') {
                } else if (event.action === 'select_external') {
                    let ret = ziti.extAuthToken(token)
                } else if (event.action === 'cannot_continue') {
                } else if (event.action === 'prompt_totp') {
                } else if (event.action === 'prompt_pin') {
                } else {
                }
    
            }

            const cfgStr = JSON.stringify(cfg)
    
            let initResult = await ziti.init(cfgStr, onAuthEvent)              

            return resolve( initResult );
        });
      } catch (e) {
          reject(e);
      }

    });

};

exports.initExternalAuth = initExternalAuth;
