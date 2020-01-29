
const binary = require('node-pre-gyp');
const path = require('path')
const binding_path = binary.find(path.resolve(path.join(__dirname,'./package.json')));
const ziti = require(binding_path);
require('assert').equal(ziti.NF_hello(),"ziti");
