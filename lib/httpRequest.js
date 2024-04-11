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


/**
 * on_req()   
 * 
 */
const on_req = ( obj ) => {

    console.log('on_req entered: ', obj);

};

/**
 * on_resp()   
 * 
 */
const on_resp = ( obj ) => {

    console.log('on_resp entered: ', obj);

};

/**
 * on_resp_data()   
 * 
 */
const on_resp_data = ( obj ) => {

    console.log('on_resp_data entered: ', obj);
    console.log('as string: ', obj.body.toString('utf8'));

};

const httpRequest = ( serviceName, schemeHostPort, method, path, headers, on_req_cb, on_resp_cb, on_resp_data_cb ) => {   

    let _on_req_cb;
    let _on_resp_cb;
    let _on_resp_data_cb;

    if (typeof on_req_cb === 'undefined') {
        _on_req_cb = on_req;
    } else {
        _on_req_cb = on_req_cb;
    }

    if (typeof on_resp_cb === 'undefined') {
        _on_resp_cb = on_resp;
    } else {
        _on_resp_cb = on_req_cb;
    }

    if (typeof on_resp_data_cb === 'undefined') {
        _on_resp_data_cb = on_resp_data;
    } else {
        _on_resp_data_cb = on_resp_data_cb;
    }

    ziti.Ziti_http_request( serviceName, schemeHostPort, method, path, headers, _on_req_cb, _on_resp_cb, _on_resp_data_cb );

};


exports.httpRequest = httpRequest;

