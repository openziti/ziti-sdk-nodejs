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
 * Default on_connect callback
 */
const defaultOnConnect = (ws) => {
    console.log('websocketConnect: connected, ws:', ws);
};

/**
 * Default on_data callback
 */
const defaultOnData = (data) => {
    console.log('websocketConnect: data received, len:', data.len);
};


/**
 * websocketConnect()
 *
 * Connect to a Ziti service via WebSocket.
 *
 * @param {string} url - The WebSocket URL (e.g., 'wss://service.ziti/path')
 * @param {string[]} headers - Array of headers in "name:value" format
 * @param {function} on_connect_cb - Callback invoked on connection. Receives the websocket handle.
 * @param {function} on_data_cb - Callback invoked when data is received. Receives {len, data}.
 * @returns {void}
 */
const websocketConnect = (url, headers, on_connect_cb, on_data_cb) => {

    const connect_cb = (typeof on_connect_cb === 'function') ? on_connect_cb : defaultOnConnect;
    const data_cb = (typeof on_data_cb === 'function') ? on_data_cb : defaultOnData;
    const hdrs = Array.isArray(headers) ? headers : [];

    ziti.ziti_websocket_connect(url, hdrs, connect_cb, data_cb);
};


exports.websocketConnect = websocketConnect;
