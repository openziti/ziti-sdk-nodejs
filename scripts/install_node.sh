#!/usr/bin/env bash

do_node_install() {
    # if an existing nvm is already installed we need to unload it
    nvm unload || true

    # here we set up the node version on the fly based on the matrix value.
    # This is done manually so that the build works the same on OS X
    rm -rf ./__nvm/ && git clone --depth 1 https://github.com/creationix/nvm.git ./__nvm
    source ./__nvm/nvm.sh
    nvm install ${NODE_VERSION}
    nvm use --delete-prefix ${NODE_VERSION}
    # node --version
    # npm --version
    which node
}

do_win_node_install() {
    # # install NVS
    # choco install nvs
    # Install NVS.
    - git clone --branch v$NVS_VERSION --depth 1 https://github.com/jasongin/nvs ~/.nvs
    - . ~/.nvs/nvs.sh
    - nvs --version

    - nvs add node/$NODEJS_VERSION
    - nvs use node/$NODEJS_VERSION
    - node --version
    - npm --version


    # ls -l ${LOCALAPPDATA}/nvs

    # # Install the selected version of Node.js using NVS.
    # ${LOCALAPPDATA}/nvs/nvs.cmd add  ${NODE_VERSION}
    # ${LOCALAPPDATA}/nvs/nvs.cmd use  ${NODE_VERSION}
    # ${LOCALAPPDATA}/nvs/nvs.cmd link ${NODE_VERSION}

    # export PATH=${LOCALAPPDATA}/nvs/default/:$PATH

    # echo "after nvs, PATH is now: $PATH"

    # ls -l ${LOCALAPPDATA}/nvs/default/node_modules/npm/bin

}

if [[ ${1:-false} == 'false' ]]; then
    echo "Error: pass node version as first argument"
    exit 1
fi

NODE_VERSION=$1

echo "OSTYPE is: $OSTYPE"

if [[ "$OSTYPE" == "linux-gnu" ]]; then
    echo "OSTYPE is supported";
    do_node_install;
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "OSTYPE is supported";
    do_node_install;
elif [[ "$OSTYPE" == "cygwin" ]]; then
    # POSIX compatibility layer and Linux environment emulation for Windows
    echo "OSTYPE is supported";
    do_win_node_install
elif [[ "$OSTYPE" == "msys" ]]; then
    # Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
    echo "OSTYPE is supported";
    do_win_node_install
elif [[ "$OSTYPE" == "win32" ]]; then
    # I'm not sure this can happen.
    echo "OSTYPE is unsupported";
elif [[ "$OSTYPE" == "freebsd"* ]]; then
    echo "OSTYPE is supported";
    do_node_install
else
    echo "OSTYPE is unsupported";
fi
