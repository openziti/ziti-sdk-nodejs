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
 * listen()   
 * 
 * @param {*} identityPath 
 */
const listen = ( serviceName, js_arb_data, on_listen, on_listen_client, on_client_connect, on_client_data ) => {

  ziti.ziti_listen( serviceName, js_arb_data, on_listen, on_listen_client, on_client_connect, on_client_data );

};

exports.listen = listen;
