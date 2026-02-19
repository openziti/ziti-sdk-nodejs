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

const http = require('node:http');
const net = require('node:net');

const connect = (dialInfo, cb) => {
    if (typeof dialInfo === 'string') {
        dialInfo = { service: dialInfo };
    }
    ziti.ziti_connect(dialInfo.service, dialInfo.identity, dialInfo.dial_data, (err, sock) => {
        if (err) {
            return cb(err);
        }

        cb(undefined, new net.Socket({fd: sock}));
    });
}

function getDialInfo(protocol, host, port) {
    return ziti.get_ziti_service(protocol, host, port);
}

class HttpAgent extends http.Agent {
    createConnection(options, callback) {
        let dialInfo;
        try {
            dialInfo = getDialInfo('tcp', options.host, options.port);
        } catch (e) {
            dialInfo = {
                service: options.host,
            }
        }

        connect(dialInfo, (err, sock) => {
            if (err) {
                console.error('Error from ziti.ziti_connect:', err);
                // fallback to default connect
                return super.createConnection(options, callback);
            }

            callback(undefined, sock);
        })
        return undefined
    }
}

const mkAgent = () => {
    return new HttpAgent();
}

exports.connect = connect;
exports.httpAgent = mkAgent;