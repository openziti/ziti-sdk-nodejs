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
 * Default on_write callback
 */
const defaultOnWrite = (result) => {
    console.log('websocketWrite: write completed, status:', result.status);
};


/**
 * websocketWrite()
 *
 * Write data to a Ziti WebSocket connection.
 *
 * @param {number} ws - The WebSocket handle returned from websocketConnect
 * @param {Buffer} data - The data to send
 * @param {function} on_write_cb - Callback invoked when write completes. Receives {ws, status}.
 * @returns {void}
 */
const websocketWrite = (ws, data, on_write_cb) => {

    const write_cb = (typeof on_write_cb === 'function') ? on_write_cb : defaultOnWrite;

    // Ensure data is a Buffer
    const buffer = Buffer.isBuffer(data) ? data : Buffer.from(data);

    ziti.ziti_websocket_write(ws, buffer, write_cb);
};


exports.websocketWrite = websocketWrite;
