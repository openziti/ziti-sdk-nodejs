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

const EventEmitter = require('events');
const stream = require('stream');
const zitiWrite = require('./write').write;



class ZitiSocket extends EventEmitter {

    constructor(opts) {

        super();

        if (typeof opts !== 'undefined') {
            if (typeof opts.client !== 'undefined') {
                this.client = opts.client;
            }
        }

        this._writableState = new stream.Writable.WritableState({}, this, true);
         
        /**
         * This stream is where we'll put any data returned from Ziti (see on_listen_client_data cb)
         */
        let self = this;
        this.readableZitiStream = new ReadableStream({
            start(controller) {
                self.readableZitiStreamController = controller;
            }
        });
    }


    /**
     * 
     */
    captureData(data) {

        if ((typeof data !== 'undefined') && (data.byteLength > 0)) {

            this.readableZitiStreamController.enqueue(data);
            this.emit('data', data);

        } else {

            this.emit('close');

        }
    }
     

    /**
     * Implements the writeable stream method `_write` by pushing the data onto the underlying Ziti connection.
    */
    async write(chunk, encoding, cb) {

        let buffer;

        if (typeof chunk === 'string' || chunk instanceof String) {
            buffer = Buffer.from(chunk, 'utf8');
        } else if (Buffer.isBuffer(chunk)) {
            buffer = chunk;
        } else if (chunk instanceof Uint8Array) {
            buffer = Buffer.from(chunk, 'utf8');
        } else {
            throw new Error('chunk type of [' + typeof chunk + '] is not a supported type');
        }

        if (buffer.length > 0) {
            zitiWrite(this.client, buffer);
        }
        if (cb) {
            cb();
        }
    }

    /**
     *
     */
    _read() { /* NOP */ }
    read()  { /* NOP */ }
    destroy() { /* NOP */ } 
    cork() { /* NOP */ }
    uncork() { /* NOP */ }
    pause() { /* NOP */ }
    resume() { /* NOP */ }
    destroy() { /* NOP */ }
    end(data, encoding, callback) { /* NOP */ }
    _final(cb) { cb(); }
    setTimeout() { /* NOP */ }
    setNoDelay() { /* NOP */ }
    unshift(head) { /* NOP */ }
}

Object.defineProperty(ZitiSocket.prototype, 'writable', {
    get() {
      return (
        true
      );
    }
});


exports.ZitiSocket = ZitiSocket;
