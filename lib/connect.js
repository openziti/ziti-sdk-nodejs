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
const https = require('node:https');
const net = require('node:net');

const connect = (dialInfo, cb) => {
    if (typeof dialInfo === 'string') {
        dialInfo = { service: dialInfo };
    }
    console.log('connect', dialInfo);
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

function doConnect(options, callback) {
    let dialInfo;
    try {
        dialInfo = getDialInfo('tcp', options.host, options.port);
    } catch (e) {
        // construct forwarding data just in case hosting side needs it
        const data = {
            dst_protocol: "tcp",
            dst_hostname: options.host,
            dst_port: options.port.toString(),
        }
        dialInfo = {
            service: options.host,
            dial_data: JSON.stringify(data),
        }
    }

    connect(dialInfo, callback)
    return undefined
}


class HttpAgent extends http.Agent {
    constructor(options) {
        super(options);
    }
    createConnection(options, callback) {
        console.log('HttpAgent createConnection', options);
        return doConnect(options, (err, sock) => {
            if (err) {
                console.error('Error from ziti.ziti_connect:', err);
                return super.createConnection(options, callback);
            }

            callback(undefined, sock);
        });
    }
}

class HttpsAgent extends https.Agent {
    createConnection(options, callback) {
        return doConnect(options, (err, sock) => {
            if (err) {
                console.error('Error from ziti.ziti_connect:', err);
                // fallback to default connect
                return super.createConnection(options, callback);
            }

            callback(undefined, sock);
        });
    }
}

function mkAgent(url) {
    if (typeof url === 'string') {
        return url.startsWith('http://') ? new HttpAgent() : new HttpsAgent();
    }
    return url === http ? new HttpAgent() : new HttpsAgent();
}

exports.connect = connect;
exports.httpAgent = mkAgent;