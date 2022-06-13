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




const getWrappedExpressApp = ( express, serviceName ) => {

  var wrappedExpressApp = express();

  /**
   * Listen for connections.
   *
   * A node `http.Server` is returned, with this
   * application (which is a `Function`) as its
   * callback. If you wish to create both an HTTP
   * and HTTPS server you may do so with the "http"
   * and "https" modules as shown here:
   *
   *    var http = require('http')
   *      , https = require('https')
   *      , express = require('express')
   *      , app = express();
   *
   *    http.createServer(app).listen(80);
   *    https.createServer({ ... }, app).listen(443);
   *
   * @return {http.Server}
   * @public
   */  
  wrappedExpressApp.listen = function() {

    console.log('=======================> wrappedExpressApp.listen() entered: arguments: ', arguments);

    // var server = http.createServer(this);
    // console.log('=======================> wrappedExpressApp.listen() 1 server: ', server);

    Object.setPrototypeOf(Server.prototype, net.Server.prototype);
    Object.setPrototypeOf(Server, net.Server);
    var server = new Server(this);


    net.Server.call(
      server,
      serviceName,
      { 
        // allowHalfOpen: true, 
        // noDelay: options.noDelay,
        // keepAlive: options.keepAlive,
        // keepAliveInitialDelay: options.keepAliveInitialDelay 
      });
  

    // zitiListen( serviceName, on_listen, on_listen_client, on_listen_client_connect, on_listen_client_data );

    // return server.listen.apply(server, serviceName, arguments);
    return server.listen(serviceName, arguments);

  };

  return wrappedExpressApp;

};


/**
 * express()   
 * 
 * @param {*} express 
 * @param {*} serviceName 
 */
const express = ( express, serviceName ) => {

  var app = getWrappedExpressApp( express, serviceName);

  // console.log('wrappedExpressApp: ', app);

  // const wrappedExpressResponse = Object.create( app.response, {

  //   data: {
  //     value: function(data) {
  //       return this.status(200).json({status: true, data: data});
  //     },
  //   },

  //   message: {
  //     value: function(msg) {
  //       return this.status(200).json({status: true, message: msg});
  //     },
  //   },

  // });

  // app.response = Object.create(wrappedExpressResponse);

  return app;

};

exports.express = express;



