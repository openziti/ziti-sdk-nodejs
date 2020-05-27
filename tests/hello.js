
const binary = require('node-pre-gyp');
const path = require('path')
const binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')), {debug: true});
console.log("binding_path is: ", binding_path);
const ziti = require(binding_path);
console.log("ziti native addon is: \n", ziti);
const result = ziti.NF_hello();
console.log("NF_hello() result is: ", result);
require('assert').equal(result,"ziti");
console.log("SUCCESS");
