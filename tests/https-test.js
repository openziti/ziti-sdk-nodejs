
var binary = require('@mapbox/node-pre-gyp');
var path = require('path')
// var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')), {debug: true});
var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')));
var ziti = require(binding_path);
require('assert').equal(ziti.ziti_hello(),"ziti");



const Ziti_http_request = async (url, method, headers) => {
    return new Promise((resolve, reject) => {
        try {

            console.log('headers (%o)', headers);

            let req = ziti.Ziti_http_request(
                url,
                method,
                headers,
                // on_req callback
                (obj) => {
                    console.log('----------- Now inside Ziti_http_request on_req callback ----------, obj is: \n%o', obj);
                },
                // on_resp callback
                (obj) => {
                    console.log('----------- Now inside Ziti_http_request on_resp callback ----------, obj is: \n%o', obj);
                    // resolve(obj);
                },
                // on_resp_data callback
                (obj) => {
                    console.log('----------- Now inside Ziti_http_request on_resp_data callback ----------, obj is: \n%o', obj);
                    if (obj.body) {
                        console.log('----------- obj.body is: \n%o', obj.body.toString());
                    }
                },
            );

            console.log('inside JS Ziti_http_request(), req is (%o)', req);
            resolve(req);

        }
        catch (e) {
            reject(e);
        }
    });
}

const Ziti_http_request_data = async (req, buffer) => {
    return new Promise((resolve, reject) => {
        ziti.Ziti_http_request_data(
            req,
            buffer,
            // on_req_data callback
            (obj) => {
                console.log('----------- Now inside Ziti_http_request_data on_req_data callback ----------, obj is: \n%o', obj);
                resolve(obj);
            }
        );
    });
}

const NF_init = async () => {
    return new Promise((resolve) => {
        ziti.ziti_init(process.argv[2], () => {
            resolve();
        });
    });
};

let ctr = 0;
const spin = () => {
    setTimeout( () => {
        ctr++;
        console.log("1-sec wait, ctr=%d", ctr); 
        if (ctr < 60) {
            spin();
        } else {
            process.exit(0);
        }
    }, 1000);
}

// let chunkctr = 0;
let chunkBody = process.argv[5];

// const sendChunk = (req) => {
//     setTimeout( (req) => {
//         chunkctr++;
//         console.log("chunkctr=%d", chunkctr); 
//         if (chunkctr < 10) {
            
//             buffer = Buffer.from(chunkBody + "-" + chunkctr);

//             console.log("sending chunk %d", chunkctr);

//             Ziti_http_request_data(req, buffer);

//             sendChunk(req);

//         } else {
//             console.log("======== calling Ziti_http_request_end");
//             ziti.Ziti_http_request_end( req );
//         }
//     }, 500, req);
// }

const sendChunk = (req) => {
    setTimeout( (req) => {
        buffer = Buffer.from(chunkBody);
        console.log("sending chunk");
        Ziti_http_request_data(req, buffer);
    }, 500, req);
}



(async () => {

    let url = process.argv[3];
    let method = process.argv[4];
    let body = process.argv[5];
    let buffer;
    let results;
    let headersArray = [
        "Content-Length:1",
        "Transfer-Encoding:chunked",
    ];


    await NF_init();

    if (typeof body !== 'undefined') {

        buffer = Buffer.from("1");

        console.log("Sending 'body' of: %o", buffer);

        let req = await Ziti_http_request(url, method, headersArray).catch((err) => {
            console.log('Ziti_http_request failed with error (%o)', err);
            process.exit(-1);
        });

        console.log("Ziti_http_request results is:\n\n%o", req);
   
    
        for (let i = 0; i < 10; i++) {
            console.log("queueing chunk %d", i);

            results = await Ziti_http_request_data(req, buffer).catch((err) => {
                console.log('Ziti_http_request_data failed with error (%o)', err);
                process.exit(-1);
            });
    
        }

        console.log('========================================================================================================');
        console.log('======================================== Ziti_http_request_end called ==================================');
        ziti.Ziti_http_request_end( req );
        console.log('========================================================================================================');

    } else {

        console.log("No 'body' will be sent");


        let req = await Ziti_http_request(url, method, []).catch((err) => {
            console.log('Ziti_http_request failed with error (%o)', err);
            process.exit(-1);
        });

        // console.log('inside JS main(), req is (%o)', req);


        // setTimeout( async () => {
        
        // for (let i=0; i<3; i++ ) {

        //     let req2 = await Ziti_http_request(url, method, []).catch((err) => {
        //         console.log('Ziti_http_request failed with error (%o)', err);
        //         process.exit(-1);
        //     });
    
        //     console.log('inside JS main() setTimeout(), req is (%o)', req2);

        // }

        // }, 100);
    
        // let req2 = await Ziti_http_request(url, method, []).catch((err) => {
        //     console.log('Ziti_http_request failed with error (%o)', err);
        //     process.exit(-1);
        // });

        // console.log("Ziti_http_request results is:\n\n%o", req2);

        // console.log('========================================================================================================');
        // console.log('======================================== Ziti_http_request_end called ==================================');
        // ziti.Ziti_http_request_end( req );
        // console.log('========================================================================================================');


    }


    spin();

})();
