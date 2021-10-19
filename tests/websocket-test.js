
var binary = require('@mapbox/node-pre-gyp');
var path = require('path')
var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')), {debug: true});
// var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')));
var ziti = require(binding_path);
require('assert').equal(ziti.ziti_hello(),"ziti");



const do_ziti_websocket_connect = async (url) => {
    return new Promise((resolve, reject) => {
        try {

            console.log('----------- url (%o)', url);

            let rc = ziti.ziti_websocket_connect(
                url,

                // on_connect callback
                (ws) => {
                    console.log('----------- Now inside ziti_websocket_connect on_connect callback ----------, ws is: %o', ws);
                    resolve(ws);
                },

                // on_data callback
                (obj) => {
                    console.log('----------- Now inside ziti_websocket_connect on_data callback ----------, obj is: \n%o', obj);
                    console.log('----------- obj.body is: \n%o', obj.body.toString());
                },
            );

            // console.log('inside JS Ziti_http_request(), req is (%o)', rc);
            // resolve(req);

        }
        catch (e) {
            reject(e);
        }
    });
}


const do_ziti_websocket_write = async (ws, buffer) => {
    return new Promise((resolve, reject) => {
        try {

            console.log('----------- ws (%o)', ws);

            let rc = ziti.ziti_websocket_write(
                ws,
                buffer,

                // on_write callback
                (obj) => {
                    console.log('----------- Now inside ziti_websocket_write on_write callback ----------, obj is: %o', obj);
                    resolve(obj);
                },

            );

        }
        catch (e) {
            reject(e);
        }
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
        console.log("----------- 1-sec wait, ctr=%d", ctr); 
        if (ctr < 10) {
            spin();
        } else {
            process.exit(0);
        }
    }, 1000);
}




(async () => {

    let url = process.argv[3];

    await NF_init();


    let ws = await do_ziti_websocket_connect(url).catch((err) => {
        console.log('----------- do_ziti_websocket_connect failed with error (%o)', err);
        process.exit(-1);
    });

    console.log('----------- do_ziti_websocket_connect() returned, ws is (%o)', ws);

    let buffer = Buffer.from("this is some data");

    let obj = await do_ziti_websocket_write(ws, buffer).catch((err) => {
        console.log('----------- do_ziti_websocket_write failed with error (%o)', err);
        process.exit(-1);
    });

    console.log('----------- do_ziti_websocket_write() returned, obj is (%o)', obj);

    spin();

})();
