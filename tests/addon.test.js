const ziti = require("../ziti.js");
const assert = require("node:assert");
const test = require("node:test");


test("ziti.exports tests", () => {
    assert(typeof ziti.ziti_sdk_version === "function", "ziti_sdk_version should be a function");
    assert(typeof ziti.enroll === "function", "ziti_enroll should be a function");
    assert(typeof ziti.setLogger === "function", "ziti_set_logger should be a function");

})
test("ziti_sdk_version test", () => {
    const result = ziti.ziti_sdk_version();
    console.log("ziti_sdk_version() result is: ", result);
    assert(result !== "", "ziti_sdk_version should not return an empty string");
})





