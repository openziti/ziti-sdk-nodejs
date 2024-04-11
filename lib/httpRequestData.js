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
 * on_req_data()   
 * 
 */
const on_req_data = ( obj ) => {

    console.log('on_req_data entered: ', obj);
    console.log('as string: ', obj.body.toString('utf8'));

};

const httpRequestData = ( req, buffer, on_req_data_cb ) => {

    console.log('httpRequestData entered: ', req, buffer);

    let _on_req_data_cb;

    if (typeof on_req_data_cb === 'undefined') {
        _on_req_data_cb = on_req_data;
    } else {
        _on_req_data_cb = on_req_data_cb;
    }

    ziti.Ziti_http_request_data( req, buffer, _on_req_data_cb );
};


exports.httpRequestData = httpRequestData;

