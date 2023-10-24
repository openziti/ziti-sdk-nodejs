
var binary = require('@mapbox/node-pre-gyp');
var path = require('path')
var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')), {debug: true});
// var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')));
var ziti = require(binding_path);

console.log("using ziti version: " + ziti.ziti_sdk_version())
// require('assert').equal(ziti.ziti_sdk_version(),"ziti");



const ziti_enroll = async (jwt_path) => {
    return new Promise((resolve, reject) => {
        let rc = ziti.ziti_enroll(jwt_path, (data) => {
            if (data.identity) {
                resolve(data);
            } else {
                reject(data);
            }
        });
    });
};


(async () => {

    let jwt_path = process.argv[2];

    let data = await ziti_enroll(jwt_path).catch((data) => {
        console.log('ziti_enroll failed with error code (%o/%s)', data.status, data.err);
    });

    if (data && data.identity) {
        console.log("data.identity is:\n\n%s\n", data.identity);
    }

    process.exit(0);

})();
