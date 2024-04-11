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
 * on_connect()   
 * 
 */
 const on_connect = ( status ) => {

};

/**
 * on_data()   
 * 
 */
 const on_data = ( status ) => {

};


/**
 * dial()   
 * 
 * @param {*} serviceName 
 * @param {*} isWebSocket 
 * @param {*} on_connect_cb callback 
 * @param {*} on_data_cb callback 
 */
const dial = ( serviceName, isWebSocket, on_connect_cb, on_data_cb ) => {

  let connect_cb;
  let data_cb;

  if (typeof on_connect_cb === 'undefined') {
    connect_cb = on_connect;
  } else {
    connect_cb = on_connect_cb;
  }

  if (typeof on_data_cb === 'undefined') {
    data_cb = on_data;
  } else {
    data_cb = on_data_cb;
  }

  ziti.ziti_dial(serviceName, isWebSocket, connect_cb, data_cb);

};


exports.dial = dial;
