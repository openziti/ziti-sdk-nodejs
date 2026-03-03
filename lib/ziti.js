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
 * OpenZiti SDK for Node.js
 * 
 * @module @openziti/ziti-sdk-nodejs
 * 
 */

const logger = require('./logger');

function load_addon() {
    try {
        return require('../build/Debug/ziti_sdk_nodejs');
    } catch {
        return require('../build/Release/ziti_sdk_nodejs');
    }
}

const binding = load_addon();

ziti = module.exports = exports = binding;



/**
 *  Attach the external, app-facing, API to the 'ziti' object
 */

/**
 * Close a Ziti connection.
 * @function close
 * @param {number} conn - A Ziti connection handle.
 * @returns {void} No return value.
 */
exports.close             = require('./close').close;

/**
 * Create a connection to Ziti Service.
 * @async
 * @function dial
 * @param {string} serviceName - The name of the Ziti Service to connect to
 * @param {boolean} isWebSocket - True or False indicator concerning whether this connection if bi-directional.
 * @param {onConnectCallback} onConnect - The callback that receives the connection handle.
 * @param {onDataCallback} onData - The callback that receives incoming data from the connection.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `dial` API.
 * @callback onDataCallback - The callback that receives the connection handle.
 * @param {number} conn - A Ziti connection handle.
 */
/**
 * This callback is part of the `dial` API.
 * @callback onConnectCallback - The callback that receives incoming data from the connection.
 * @param {Buffer} data - Incoming data from the Ziti connection.
 * @returns {void} No return value.
 */
exports.dial              = require('./dial').dial;

/**
 * Enroll a Ziti Identity.
 * @async
 * @function enroll
 * @param {string} jwt_path - The path to the JWT
 * @param {onEnrollCallback} onEnroll - The callback that receives the enrollment status.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `enroll` API.
 * @callback onEnrollCallback - The callback that receives the enrollment status.
 * @param {object} obj - enrollment status.
 */
 exports.enroll           = require('./enroll').enroll;

/**
 * Wrap ExpressJS to facilitate hosting (listening) on a Ziti Service instead of a TCP port.
 * @function express
 * @param {*} express - The express() object.
 * @param {string} serviceName - The name of the Ziti Service being served (hosted).
 * @returns {*} The wrapped express() object.
 */
exports.express           = require('./express').express;

/**
 * Initiate an HTTP request to a Ziti Service.
 * @function httpRequest
 * @param {string} serviceName - The name of the Ziti Service to send the request. (mutually exclusive with url)
 * @param {string} schemeHostPort - The scheme/host/port (e.g. http://myserver.ziti:8080) of a Ziti service-config/intercept to send the request. (mutually exclusive with serviceName)
 * @param {string} method - The REST verb to use (e.g. `GET`, `POST`).
 * @param {string} path - The URL PATH to use on the request (can include HTTP query parms).
 * @param {string[]} headers - The HTTP Headers to use on the request.
 * @param {onRequestCallback} onRequest - The callback that receives the request handle.
 * @param {onResonseCallback} onResponse - The callback that receives the HTTP Response.
 * @param {onResonseDataCallback} onResponseData - The callback that receives the HTTP Response data.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `httpRequest` API.
 * @callback onRequestCallback - The callback that receives the request handle.
 * @param {number} req - A Ziti HttpRequest handle.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `httpRequest` API.
 * @callback onResonseCallback - The callback that receives response from the request.
 * @param resp - Incoming response from the HTTP request.
 * @param resp.req - The request handle.
 * @param resp.code - The HTTP status code.
 * @param resp.headers - The HTTP Headers on the response.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `httpRequest` API.
 * @callback onResonseDataCallback - The callback that receives incoming data from the request.
 * @param respData - Incoming response data from the HTTP request.
 * @param respData.req - The request handle.
 * @param respData.len - The length of the response body.
 * @param respData.body - The response body.
 * @returns {void} No return value.
 */
exports.httpRequest       = require('./httpRequest').httpRequest;

/**
 * Send payload data for HTTP POST request to a Ziti Service.
 * @function httpRequestData
 * @param {number} req - A Ziti HttpRequest handle.
 * @param {Buffer} data - The HTTP payload data to send.
 * @param {onRequestDataCallback} onRequestData - The callback that acknowleges the send.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `httpRequestData` API.
 * @callback onRequestDataCallback - The callback that acknowleges the send.
 * @param reqData - Incoming status data from the HTTP request.
 * @param respData.req - The request handle.
 * @param respData.status - positive value indicates successful transmit.
 * @returns {void} No return value.
*/
 exports.httpRequestData   = require('./httpRequestData').httpRequestData;

/**
 * Terminate payload data transmission for HTTP POST request to a Ziti Service.
 * @function httpRequestEnd
 * @param {number} req - A Ziti HttpRequest handle.
 * @returns {void} No return value.
 */
 exports.httpRequestEnd   = require('./httpRequestEnd').httpRequestEnd;

/**
 * Connect to a Ziti Service via WebSocket.
 * @function websocketConnect
 * @param {string} url - The WebSocket URL (e.g., 'wss://service.ziti/path')
 * @param {string[]} headers - Array of headers in "name:value" format
 * @param {onWebSocketConnectCallback} onConnect - Callback invoked on connection.
 * @param {onWebSocketDataCallback} onData - Callback invoked when data is received.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `websocketConnect` API.
 * @callback onWebSocketConnectCallback - Called when WebSocket connection is established.
 * @param {number} ws - The WebSocket handle for use with websocketWrite.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `websocketConnect` API.
 * @callback onWebSocketDataCallback - Called when data is received on the WebSocket.
 * @param {object} data - The received data object.
 * @param {number} data.len - Length of the data (negative indicates error/close).
 * @param {Buffer} data.data - The received data buffer.
 * @returns {void} No return value.
 */
exports.websocketConnect  = require('./websocketConnect').websocketConnect;

/**
 * Write data to a Ziti WebSocket connection.
 * @function websocketWrite
 * @param {number} ws - The WebSocket handle from websocketConnect.
 * @param {Buffer} data - The data to send.
 * @param {onWebSocketWriteCallback} onWrite - Callback invoked when write completes.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `websocketWrite` API.
 * @callback onWebSocketWriteCallback - Called when the write operation completes.
 * @param {object} result - The write result object.
 * @param {number} result.ws - The WebSocket handle.
 * @param {number} result.status - Write status (0 = success, negative = error).
 * @returns {void} No return value.
 */
exports.websocketWrite    = require('./websocketWrite').websocketWrite;

/**
 * Close a Ziti WebSocket connection.
 * @function websocketClose
 * @param {number} ws - The WebSocket handle from websocketConnect.
 * @returns {void} No return value.
 */
exports.websocketClose    = require('./websocketClose').websocketClose;

/**
 * Send a ping frame on a Ziti WebSocket connection.
 * @function websocketPing
 * @param {number} ws - The WebSocket handle from websocketConnect.
 * @returns {number} 0 on success, error code on failure.
 */
exports.websocketPing     = require('./websocketPing').websocketPing;

/**
 * Provide an external JWT token for authentication.
 *
 * Use this function when external authentication is required (e.g., OIDC).
 * The auth event callback provided to init() will indicate when external
 * auth is needed along with the provider details.
 *
 * @function extAuthToken
 * @param {string} jwtToken - The JWT token obtained from the external identity provider
 * @returns {number} 0 on success, -22 (ZITI_INVALID_STATE) if context not initialized
 * @throws {Error} If jwtToken is not a non-empty string
 */
exports.extAuthToken      = require('./extAuthToken').extAuthToken;

/**
 * Initialize the Ziti session and authenticate with control plane.
 * @function init
 * @param {string} identityPath - File system path to the identity file.
 * @param {onAuthEventCallback} [onAuthEvent] - Optional callback for authentication events.
 * @returns {Promise<void>} Resolves when initialization is complete.
 */
/**
 * This callback is part of the `init` API.
 * @callback onAuthEventCallback - Called when authentication events occur.
 * @param {object} authEvent - The authentication event object.
 * @param {string} authEvent.action - The action required (e.g., 'login_external', 'select_external', 'cannot_continue', 'prompt_totp', 'prompt_pin').
 * @param {string} authEvent.type - The authentication type (e.g., 'oidc').
 * @param {string} authEvent.detail - Additional details (e.g., the OIDC provider URL).
 * @returns {void} No return value.
 */
exports.init              = require('./init').init;

/**
 * Initialize external authentication with a Ziti controller.
 * @function initExternalAuth
 * @param {string} controllerHost - The hostname of the Ziti controller (e.g., "ctrl.example.com:1280")
 * @param {onAuthEventCallback} onAuthEvent - Callback for authentication events.
 * @returns {number} 0 on success, error code on failure
 */
exports.initExternalAuth  = require('./initExternalAuth').initExternalAuth;

// Internal use only
exports.listen            = require('./listen').listen;

/**
 * Set the logging level.
 * @function setLogLevel
 * @param {number} level - 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
 * @returns {void} No return value.
 */
exports.setLogLevel       = logger.setLogLevel;

/**
 * Set the logger function.
 *
 * **Note**: passing log messages to your own logger
 * function will likely have a negative impact on performance. Use with caution.
 *
 * @function setLogger
 * @param {function} loggerFunc - A function that accepts a log message object
 * with the following properties: { level, source, message }
 * @returns {undefined} No return value.
 */
exports.setLogger         = logger.setLogger;

/**
 * Set the logging level.
 * @function serviceAvailable
 * @param {string} serviceName - The name of the Ziti Service being queried.
 * @param {onServiceAvailableCallback} onServiceAvailable - The callback that returns results of the query.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `serviceAvailable` API.
 * @callback onServiceAvailableCallback - The callback that returns results of the query.
 * @param availability - results of the query.
 * @param availability.status - 0 means `available and OK`, <0 means `unavailable`
 * @param availability.permissions - 1 means the identity can dial, 2 means the identity can bind
 * @returns {void} No return value.
 */
exports.serviceAvailable  = require('./serviceAvailable').serviceAvailable;

/**
 * write data to a Ziti connection.
 * @function write
 * @param {number} conn - A Ziti connection handle.
 * @param {Buffer} data - The data to send.
 * @param {onWriteCallback} onWrite - The callback that returns status of the write.
 * @returns {void} No return value.
 */
/**
 * This callback is part of the `write` API.
 * @callback onWriteCallback - The callback that returns results of the v.
 * @param status - 0 means success, <0 means failure.
 * @returns {void} No return value.
 */
 exports.write             = require('./write').write;

const connect = require('./connect')
exports.connect = connect.connect
exports.httpAgent = connect.httpAgent
