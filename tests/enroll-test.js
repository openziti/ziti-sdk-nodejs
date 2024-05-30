const bindings = require('bindings')('ziti_sdk_nodejs')

const result = bindings.ziti_sdk_version();
const binary = require('@mapbox/node-pre-gyp');
const path = require('path')
const binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')));
const ziti = require(binding_path);
require('assert').notEqual(result,"");

console.log("using ziti version: " + ziti.ziti_sdk_version())



const ziti_Enroll = async (jwt_path) => {
    console.log("JS ziti_Enroll() entered ")
    return new Promise((resolve, reject) => {
        let rc = ziti.ziti_enroll(
            jwt_path,
            (data) => {
              return resolve(data);
            }
          );
    });
};


(async () => {

    let jwt_path = process.argv[2];

    let data = await ziti_Enroll(jwt_path).catch((data) => {
        console.log('JS ziti_enroll failed with error code (%o/%s)', data.status, data.err);
    });

    if (data && data.identity) {
        console.log("data.identity is:\n\n%s\n", data.identity);
    }

    process.exit(0);

})();
