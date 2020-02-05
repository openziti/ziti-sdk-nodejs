{
  # "includes": [ "deps/common-ziti.gypi" ],

  # The "cd" variable is passed in, and used, only during Windows builds
  'variables': {
    'cd%': '.'
	},

  "targets": [
    {
      "target_name": "<(module_name)",

      "product_dir": "<(module_path)",

      "sources": [ 
        "./src/ziti-add-on.c",
        # "./src/stack_traces.c",
        "./src/NF_hello.c",
        "./src/NF_init.c",
        "./src/NF_dial.c",
        "./src/NF_shutdown.c",
        "./src/NF_write.c",
        "./src/NF_service_available.c",
        "./src/NF_close.c",
      ],

      "include_dirs": [
          "deps/ziti-sdk-c/includes",
          "deps/ziti-sdk-c/deps/uv-mbed/include",
      ],

      "conditions": [

        ["OS=='mac'", {

          "libraries": [ 
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libhttp-parser.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/crypto/library/libmbedcrypto.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/libmbedtls.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/libmbedx509.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv/libuv_a.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv_link.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/libuv_mbed.a",
            "$(PWD)/deps/ziti-sdk-c/build/library/libziti.a",        
          ],

          "xcode_settings": {
            "ALWAYS_SEARCH_USER_PATHS": "NO",
            "GCC_CW_ASM_SYNTAX": "NO",                # No -fasm-blocks
            "GCC_DYNAMIC_NO_PIC": "NO",               # No -mdynamic-no-pic
                                                      # (Equivalent to -fPIC)
            "GCC_ENABLE_CPP_EXCEPTIONS": "NO",        # -fno-exceptions
            "GCC_ENABLE_CPP_RTTI": "NO",              # -fno-rtti
            "GCC_ENABLE_PASCAL_STRINGS": "NO",        # No -mpascal-strings
            "GCC_THREADSAFE_STATICS": "NO",           # -fno-threadsafe-statics
            "PREBINDING": "NO",                       # No -Wl,-prebind
            "MACOSX_DEPLOYMENT_TARGET": "10.13",      # -mmacosx-version-min=10.13
            "USE_HEADERMAP": "NO",
            "OTHER_CFLAGS": [
              "-fno-strict-aliasing",
              "-g",
              "-fno-pie"
            ],
            "OTHER_LDFLAGS": [
              "-g",
              "-mmacosx-version-min=10.13",
            ],
            "WARNING_CFLAGS": [
              "-Wall",
              "-Wendif-labels",
              "-W",
              "-Wno-unused-parameter",
              "-Wno-pointer-sign",
            ],
          }
        }],


        ['OS == "win"', {

          "include_dirs": [ 
            'deps\ziti-sdk-c\includes',
            'deps\ziti-sdk-c\deps\uv-mbed\include',
          ],

          "libraries": [
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/http-parser.lib",
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/crypto/library/mbedcrypto.lib",
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/mbedtls.lib",
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/mbedx509.lib",
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv/uv_a.lib",
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/uv_link.lib",
            "<(cd)/deps/ziti-sdk-c/build/deps/uv-mbed/uv_mbed.lib",
            "<(cd)/deps/ziti-sdk-c/build/library/ziti.lib",        
          ]

        }],


        ['OS == "linux"', {

          "libraries": [ 
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libhttp-parser.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/crypto/library/libmbedcrypto.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/libmbedtls.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/libmbedx509.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv/libuv_a.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv_link.a",
            "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/libuv_mbed.a",
            "$(PWD)/deps/ziti-sdk-c/build/library/libziti.a",        
          ],

          "cflags": ["-w", "-fpermissive", "-fPIC"]

        }]

      ]

    }
  ]
}