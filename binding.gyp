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
        "./src/ziti_close.c",
        "./src/ziti_dial.c",
        "./src/ziti_enroll.c",
        "./src/ziti_hello.c",
        "./src/Ziti_https_request.c",
        "./src/Ziti_https_request_data.c",
        "./src/Ziti_https_request_end.c",
        "./src/ziti_init.c",
        "./src/ziti_service_available.c",
        "./src/ziti_shutdown.c",
        "./src/ziti_write.c",
        "./src/ziti_websocket_connect.c",
        "./src/ziti_websocket_write.c",
        # "./src/stack_traces.c",
        "./src/utils.c",
      ],

      "include_dirs": [
          "deps/ziti-sdk-c/includes",
          "deps/ziti-sdk-c/deps/uv-mbed/include",
          "deps/ziti-sdk-c/deps/uv-mbed/deps/http-parser",
          "deps/ziti-sdk-c/deps/uv-mbed/deps/uv_link_t/include",
      ],

      "conditions": [

        ["OS=='mac'", {

          "libraries": [ 

            "$(PWD)/deps/ziti-sdk-c/build/_deps/mbedtls-build/library/libmbedcrypto.a",
            "$(PWD)/deps/ziti-sdk-c/build/_deps/mbedtls-build/library/libmbedtls.a",
            "$(PWD)/deps/ziti-sdk-c/build/_deps/mbedtls-build/library/libmbedx509.a",
            "$(PWD)/deps/ziti-sdk-c/build/_deps/libuv-build/libuv_a.a",
            "$(PWD)/deps/ziti-sdk-c/build/_deps/libsodium-build/lib/libsodium.a",
            "$(PWD)/deps/ziti-sdk-c/build/_deps/uv-mbed-build/libuv_mbed.a",
            "$(PWD)/deps/ziti-sdk-c/build/library/libziti.a",        

            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libhttp-parser.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv_link.a",
            # "$(PWD)/deps/ziti-sdk-c/build/library/libziti.a",        


            # These are used when a local Xcode debug build is in play
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/Debug/libhttp-parser.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/crypto/library/Debug/libmbedcrypto.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/Debug/libmbedtls.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/Debug/libmbedx509.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv/Debug/libuv_a.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/Debug/libuv_link.a",
            # "$(PWD)/deps/ziti-sdk-c/build/deps/uv-mbed/Debug/libuv_mbed.a",
            # "$(PWD)/deps/ziti-sdk-c/build/library/Debug/libziti.a",        

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
            "MACOSX_DEPLOYMENT_TARGET": "10.15",      # -mmacosx-version-min=10.14
            "USE_HEADERMAP": "NO",
            "OTHER_CFLAGS": [
              "-fno-strict-aliasing",
              "-g",
              "-fno-pie",
              "-DSOURCE_PATH_SIZE=3"
            ],
            "OTHER_LDFLAGS": [
              "-g",
              "-mmacosx-version-min=10.15",
            ],
            "WARNING_CFLAGS": [
              "-Wall",
              "-Wendif-labels",
              "-W",
              "-Wno-unused-parameter",
              "-Wno-pointer-sign",
              "-Wno-unused-function",
            ],
          }
        }],


        ['OS == "win"', {

          'defines': [
            'WIN32',
            # we don't really want VC++ warning us about
            # how dangerous C functions are...
            '_CRT_SECURE_NO_DEPRECATE',
            # ... or that C implementations shouldn't use
            # POSIX names
            '_CRT_NONSTDC_NO_DEPRECATE',
            #
            'NOGDI',
            'DSOURCE_PATH_SIZE=3'
          ],

          "include_dirs": [ 
            "deps/ziti-sdk-c/includes",
            "deps/ziti-sdk-c/deps/uv-mbed/include"
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
            "<(cd)/deps/ziti-sdk-c/build/_deps/libsodium-src/x64/Release/v142/static/libsodium.lib",
            "-lws2_32.lib",
            "-lIphlpapi.lib",
            "-lpsapi",
            "-luserenv.lib"
          ],

          'msvs_settings': {
            'VCCLCompilerTool': {
              'RuntimeLibrary': 1, # static debug
              'Optimization': 0, # /Od, no optimization
            },
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/FORCE'
              ],
              'LinkTimeCodeGeneration': 1, # link-time code generation
              'GenerateDebugInformation': 'true',
              'IgnoreDefaultLibraryNames': [
                'libcmtd.lib', 'libcmt.lib', 'msvcrtd.lib'
              ],
              'SubSystem': 1, # /subsystem:console
            }
          },

        }],


        ['OS == "linux"', {

          "libraries": [
            "<(module_root_dir)/deps/ziti-sdk-c/build/library/libziti.a",
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/libuv_mbed.a",            
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libhttp-parser.a",
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv/libuv_a.a",
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/libuv_link.a",
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/libuv_mbed.a",                   
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/libmbedtls.a",
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/library/libmbedx509.a",         
            "<(module_root_dir)/deps/ziti-sdk-c/build/deps/uv-mbed/deps/mbedtls/crypto/library/libmbedcrypto.a", 
            "<(module_root_dir)/deps/ziti-sdk-c/build/_deps/libsodium-build/lib/libsodium.a",
          ],

          "link_settings": {
            "ldflags": [
              "-v",
               "-g",
            ]
          },

          "cflags": [
            "-fno-strict-aliasing",
            "-g",
            "-fno-pie",
            "-fPIC",
            "-DSOURCE_PATH_SIZE=3"
          ]

        }]

      ]

    }
  ]
}
