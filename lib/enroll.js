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
 * on_enroll()   
 * 
 */
 const on_enroll = ( status ) => {

};


/**
 * enroll()   
 * 
 * @param {*} jwt_path 
 * @param {*} on_enroll_cb callback 
 */
const enroll = ( jwt_path, on_enroll_cb ) => {

  let enroll_cb;

  if (typeof on_enroll_cb === 'undefined') {
    enroll_cb = on_enroll;
  } else {
    enroll_cb = on_enroll_cb;
  }

  ziti.ziti_enroll(jwt_path, enroll_cb);

};


exports.enroll = enroll;
