
const bindings = require('bindings')('ziti_sdk_nodejs')

console.log(bindings)
const result = bindings.ziti_sdk_version();
console.log("ziti_sdk_version() result is: ", result);



const binary = require('@mapbox/node-pre-gyp');
const path = require('path')
// const binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')), {debug: true});
const binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')));
console.log("binding_path is: ", binding_path);
const ziti = require(binding_path);
console.log("ziti native addon is: \n", ziti);
require('assert').notEqual(result,"");
console.log("SUCCESS");
