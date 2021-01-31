`ziti-sdk-nodejs`
=====================

A NodeJS-based SDK for delivering secure applications over a [Ziti Network](https://ziti.dev)

<img src="https://ziti.dev/wp-content/uploads/2020/02/ziti.dev_.logo_.png" width="200" />

Learn about Ziti at [ziti.dev](https://ziti.dev)


[![Build Status](https://github.com/openziti/ziti-sdk-nodejs/workflows/Build/badge.svg?branch=main)]()
[![npm](https://img.shields.io/npm/v/ziti-sdk-nodejs.svg)](https://www.npmjs.com/package/ziti-sdk-nodejs)
[![npm](https://img.shields.io/npm/l/ziti-sdk-nodejs.svg)](https://www.npmjs.com/package/ziti-sdk-nodejs)
[![npm](https://img.shields.io/npm/dm/ziti-sdk-nodejs.svg)](https://www.npmjs.com/package/ziti-sdk-nodejs)


## Supported platforms

The `ziti-sdk-nodejs` module works with Node.js v11.x, v12.x, v13.x, v14.x

Binaries for most Node versions and platforms are provided by default via [node-pre-gyp](https://github.com/mapbox/node-pre-gyp).

# Usage

**Note:** the module must be [installed](#installing) before use.

``` js
var ziti = require('ziti-sdk-nodejs');

const NF_init = async (identity) => {
    return new Promise((resolve) => {
        ziti.NF_init(identity, () => {
            resolve();
        });
    });
};

const NF_service_available = (service) => {
    return new Promise((resolve) => {
        ziti.NF_service_available(service, (status) => {
            resolve(status);
        });
    });
};

function NF_dial(service) {
    return new Promise((resolve, reject) => {
        ziti.NF_dial(
            service,
            (conn) => {
                resolve(conn);
            },
            (data) => {
                // Do something with data...
            },
        );
    });
}

const NF_write = (conn, data) => {
    return new Promise((resolve) => {
        ziti.NF_write(conn, data, () => {
            resolve();
        });
    });
};

(async () => {

    await NF_init(LOCATION_OF_IDENTITY_FILE);

    let status = await NF_service_available(YOUR_SERVICE_NAME);

    if (status === 0) {

        const conn = await NF_dial(YOUR_SERVICE_NAME);

        let data = SOME_KIND_OF_DATA;

        let buffer = Buffer.from(data);

        await NF_write(conn, buffer);

        ...etc
    }

})();
```


# Ziti NodeJS SDK - Setup for Development

The following steps should get your NodeJS SDK for Ziti building. The Ziti NodeJS SDK is a native addon for Node JS,
and is written in C. C development is specific to your operating system and tool chain used. These steps should work 
properly for you but if your OS has variations you may need to adapt these steps accordingly.


## Prerequisites

### Build

* [Cmake (3.12+)](https://cmake.org/install/)


## Build

### Linux/MacOS

Building the NodeJS SDK on linux/mac can be accomplished with:

```bash
$ npm run build
```


Getting Help
------------
Please use these community resources for getting help. We use GitHub [issues](https://github.com/NetFoundry/ziti-sdk-nodejs/issues) 
for tracking bugs and feature requests and have limited bandwidth to address them.

- Read the [docs](https://netfoundry.github.io/ziti-doc/ziti/overview.html)
- Join our [Developer Community](https://developer.netfoundry.io)
- Participate in discussion on [Discourse](https://openziti.discourse.group/)


Copyright&copy; 2018-2020. NetFoundry, Inc.
