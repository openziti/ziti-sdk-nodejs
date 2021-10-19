
var binary = require('@mapbox/node-pre-gyp');
var path = require('path')
var binding_path = binary.find(path.resolve(path.join(__dirname,'../package.json')));
var ziti = require(binding_path);
require('assert').equal(ziti.ziti_hello(),"ziti");



function NF_dial(service) {
    console.log('----------- inside NF_dial() ---------- service is: ', service);
    return new Promise((resolve, reject) => {
        ziti.ziti_dial(
        service,
        false, // NOT a wabsocket
        (conn) => {
          console.log('----------- Now inside NF_dial connect callback ----------, conn is: ' + conn);
          resolve(conn);
        },
        (data) => {
          console.log('----------- Now inside NF_dial data callback ----------, data is: [===\n%s\n===]', data);
        },
      );
    });
  }

const rand = max => Math.floor(Math.random() * max);
const delay = (ms, value) => new Promise(resolve => setTimeout(resolve, ms, value));

const NF_init = async () => {
    return new Promise((resolve) => {
        ziti.ziti_init(process.argv[2], () => {
            resolve();
        });
    });
};

const NF_service_available = (service) => {
    return new Promise((resolve) => {
        ziti.ziti_service_available(service, (status) => {
            resolve(status);
        });
    });
};

const NF_write = (conn, data) => {
    return new Promise((resolve) => {
        ziti.ziti_write(conn, data, () => {
            resolve();
        });
    });
};

let ctr = 0;
const spin = () => {
    setTimeout( () => {
        ctr++;
        console.log("1-sec wait, ctr=%d", ctr); 
        if (ctr < 5) {
            spin();
        } else {
            process.exit(0);
        }
    }, 1000);
}


function NF_write_callback(item) {
    if (item.status < 0) {
        // console.log("NF_write_callback(): request performed on conn '%o' failed to submit status '%o'", item.conn, item.status);
    }
    else {
        // console.log("NF_write_callback(): request performed on conn '%o' successful: '%o' bytes sent", item.conn, item.status);
    }
}

function NF_dial_connect_callback(conn) {

    console.log("NF_dial_connect_callback(): received connection '%o'; now initiating Write to service", conn);

    let data = 
        "GET / " +
        "HTTP/1.1\r\n" +
        "Accept: */*\r\n" +
        "Connection: keep-alive\r\n" +
        "Host: mattermost.ziti.netfoundry.io\r\n" +
        "User-Agent: curl/7.54.0\r\n" +
        "\r\n";

    ziti.ziti_write(
        conn, 
        data, 
        NF_write_callback
    );
}

function NF_dial_data_callback(data) {
    console.log("NF_dial_data_callback(): received the following data: \n\n", data);
}

(async () => {

    await NF_init();

    let status = await NF_service_available('mattermost.ziti.netfoundry.io');

    let conn = await NF_dial('mattermost.ziti.netfoundry.io');

    let data = 
        "GET / " +
        "HTTP/1.1\r\n" +
        "Accept: */*\r\n" +
        "Connection: keep-alive\r\n" +
        "Host: mattermost.ziti.netfoundry.io\r\n" +
        "User-Agent: curl/7.54.0\r\n" +
        "\r\n";

    let buffer = Buffer.from(data);

    await NF_write(conn, buffer);

    spin();

})();
