include(FetchContent)

if (WIN32)
    # nodejs contains all needed libuv stuff
    # trick TLSUV to link against it
    add_library(libuv::uv SHARED IMPORTED)
    set_target_properties(libuv::uv PROPERTIES
            IMPORTED_LOCATION ${CMAKE_JS_LIB}
            IMPORTED_IMPLIB ${CMAKE_JS_LIB}
    )
    set(TLSUV_LIBUV_LIB libuv::uv)
    set(TLSUV_TLSLIB openssl)

    # make sure on WIN32 tlsuv uses our definition for libuv
    FetchContent_Declare(tlsuv
            GIT_REPOSITORY https://github.com/openziti/tlsuv.git
            GIT_TAG v0.40.9
    )
    FetchContent_MakeAvailable(tlsuv)
endif (WIN32)

FetchContent_Declare(ziti
        GIT_REPOSITORY https://github.com/openziti/ziti-sdk-c.git
        GIT_TAG 1.10.9
)
FetchContent_MakeAvailable(ziti)


