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

var binding;

function importAll (r) {
  r.keys().forEach(key => {
    binding = r(key); // Load the addon
  });
}

if (typeof require.context == 'function') {

  importAll( require.context("../build/", true, /\.node$/) );

} else {

    const binary = require('@mapbox/node-pre-gyp');
    const path = require('path')
    const binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')), {debug: false});

    binding = require(binding_path);
    
}

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
 * Initialize the Ziti session and authenticate with control plane.
 * @function init
 * @param {string} identityPath - File system path to the identity file.
 * @returns {number} A status value ranging from 0 to 255.
 */
exports.init              = require('./init').init;

// Internal use only
exports.listen            = require('./listen').listen;

/**
 * Set the logging level.
 * @function setLogLevel
 * @param {number} level - 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
 * @returns {void} No return value.
 */
exports.setLogLevel       = require('./setLogLevel').setLogLevel;

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
