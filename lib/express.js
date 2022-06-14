/*
Copyright Netfoundry, Inc.

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

const net = require('./express-listener');
const { Server } = require('_http_server'); // from NodeJS internals



/**
 * express()   
 * 
 * @param {*} express 
 * @param {*} serviceName 
 */
const express = ( express, serviceName ) => {

  var wrappedExpressApp = express();

  /**
   * Listen for connections.
   *
   * A node `http.Server` is returned, with this
   * application (which is a `Function`) as its
   * callback. 
   *
   * @return {http.Server}
   * @public
   */  
  wrappedExpressApp.listen = function() {

    Object.setPrototypeOf(Server.prototype, net.Server.prototype);
    Object.setPrototypeOf(Server, net.Server);
    var server = new Server(this);

    net.Server.call( server, serviceName, { } );

    return server.listen(serviceName, arguments);

  };

  return wrappedExpressApp;

};

exports.express = express;
