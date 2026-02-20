import ziti from '../ziti.js';
import https from 'node:https';
import http from 'node:http';

// Usage node http-get.mjs <identity.json> <url>
// <url> can be a Ziti service intercept (e.g. http://myserver.ziti:8080) or http://<service-name> (port does not matter)
const IDENTITY_FILE = process.argv[2];
const URL    = process.argv[3];

if (!IDENTITY_FILE) {
    console.error('Set ZITI_IDENTITY_FILE to your identity JSON path');
    process.exit(1);
}

console.log('Initializing...');
await ziti.init(IDENTITY_FILE);
console.log('Init done');

const client = URL.startsWith('https://') ? https : http;

client.get(URL,
    { agent: ziti.httpAgent(client) },
    (res) => {
        console.log(`HTTP status code: ${res.statusCode} ${res.statusMessage}`);
        for (const k in res.headers) {
            const header = res.headers[k];
            console.log(`  ${k}: ${header}`);
        }
        res.on('data', (chunk) => {
            console.log('Received body chunk:', chunk.toString());
        }).on('end', ziti.ziti_shutdown)
    });
