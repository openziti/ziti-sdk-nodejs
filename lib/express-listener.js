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

const zitiListen = require('./listen').listen;
const EventEmitter = require('events');
const { ZitiSocket } = require('./ziti-socket');


const normalizedArgsSymbol = Symbol('normalizedArgs');


var _serversIndex = 1;
var _servers = new Map();

/**
 * on_listen()   
 * 
 * @param {*} status 
 */
Server.prototype.on_listen = ( status ) => {

};
  
/**
 * on_listen_client()   
 * 
 * @param {*} obj 
 */
 Server.prototype.on_listen_client = ( obj ) => {
  
};
  
/**
 * on_client_write()   
 * 
 * @param {*} obj 
 */
 Server.prototype.on_client_write = ( obj ) => {
    
};

/**
 * on_listen_client_connect()   
 * 
 * @param {*} obj 
 */
 Server.prototype.on_listen_client_connect = ( obj ) => {
  
    let self = _servers.get(obj.js_arb_data);

    const socket = new ZitiSocket({ client: obj.client });

    self._socket = socket;
        
    self.emit('connection', socket);
};

/**
 * on_listen_client_data()   
 * 
 * @param {*} obj 
 */
 Server.prototype.on_listen_client_data = ( obj ) => {
    
    let self    = _servers.get(obj.js_arb_data);
    let socket  = self._socket;

    socket.captureData(obj.app_data);
};
  

/**
 * 
 * @param {*} args 
 */
function normalizeArgs(args) {
    let arr;
  
    if (args.length === 0) {
      arr = [{}, null];
      arr[normalizedArgsSymbol] = true;
      return arr;
    }
  
    const arg0 = args[0];
    let options = {};
    if (typeof arg0 === 'object' && arg0 !== null) {
      // (options[...][, cb])
      options = arg0;
    } else {
      // ([port][, host][...][, cb])
      options.port = arg0;
      if (args.length > 1 && typeof args[1] === 'string') {
        options.host = args[1];
      }
    }
  
    const cb = args[args.length - 1];
    if (typeof cb !== 'function')
      arr = [options, null];
    else
      arr = [options, cb];
  
    arr[normalizedArgsSymbol] = true;
    return arr;
}
  
function Server(serviceName, options, connectionListener) {

    if (!(this instanceof Server))
      return new Server(options, connectionListener);
  
    EventEmitter.call(this);
  
    if (typeof options === 'function') {
      connectionListener = options;
      options = {};
      this.on('connection', connectionListener);
    } else if (options == null || typeof options === 'object') {
      options = { ...options };
  
      if (typeof connectionListener === 'function') {
        this.on('connection', connectionListener);
      }
    } else {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
    }
    if (typeof options.keepAliveInitialDelay !== 'undefined') {
      validateNumber(
        options.keepAliveInitialDelay, 'options.keepAliveInitialDelay'
      );
  
      if (options.keepAliveInitialDelay < 0) {
        options.keepAliveInitialDelay = 0;
      }
    }

    this._serviceName = serviceName;

    this._connections = 0;
  
    // this[async_id_symbol] = -1;
    this._handle = null;
    this._usingWorkers = false;
    this._workers = [];
    this._unref = false;
  
    this.allowHalfOpen = options.allowHalfOpen || false;
    this.pauseOnConnect = !!options.pauseOnConnect;
    this.noDelay = Boolean(options.noDelay);
    this.keepAlive = Boolean(options.keepAlive);
    this.keepAliveInitialDelay = ~~(options.keepAliveInitialDelay / 1000);
}
Object.setPrototypeOf(Server.prototype, EventEmitter.prototype);
Object.setPrototypeOf(Server, EventEmitter);
  

Server.prototype.listen = function( serviceName, ...args ) {

    let normalized = normalizeArgs(args);
    normalized = normalizeArgs(normalized[0]);

    // let options = normalized[0];             // we currently ignore options (a.k.a. `port`)
    let cb = normalized[1];
    if (cb === null) { cb = this.on_listen; }   // Use our on_listen cb is necessary, else use cb from teh calling app

    let index = _serversIndex++;
    _servers.set(index, this);

    zitiListen( serviceName, index, cb, this.on_listen_client, this.on_listen_client_connect, this.on_listen_client_data );
};

Server.prototype.address = function() {
    if (this._handle && this._handle.getsockname) {
      const out = {};
      const err = this._handle.getsockname(out);
      if (err) {
        throw errnoException(err, 'address');
      }
      return out;
    } else if (this._pipeName) {
      return this._pipeName;
    }
    return null;
};
  
Server.prototype.close = function(cb) {
    if (typeof cb === 'function') {
      if (!this._handle) {
        this.once('close', function close() {
          cb(new ERR_SERVER_NOT_RUNNING());
        });
      } else {
        this.once('close', cb);
      }
    }
  
    if (this._handle) {
      this._handle.close();
      this._handle = null;
    }
  
    if (this._usingWorkers) {
      let left = this._workers.length;
      const onWorkerClose = () => {
        if (--left !== 0) return;
  
        this._connections = 0;
        this._emitCloseIfDrained();
      };
  
      // Increment connections to be sure that, even if all sockets will be closed
      // during polling of workers, `close` event will be emitted only once.
      this._connections++;
  
      // Poll workers
      for (let n = 0; n < this._workers.length; n++)
        this._workers[n].close(onWorkerClose);
    } else {
      this._emitCloseIfDrained();
    }
  
    return this;
};
  
Object.defineProperty(Server.prototype, 'listening', {
    __proto__: null,
    get: function() {
      return !!this._handle;
    },
    configurable: true,
    enumerable: true
});
  

module.exports = {
    Server,
};
  