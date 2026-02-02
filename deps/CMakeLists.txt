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
endif (WIN32)

FetchContent_Declare(ziti
        GIT_REPOSITORY https://github.com/openziti/ziti-sdk-c.git
        GIT_TAG 1.10.9
)
FetchContent_MakeAvailable(ziti)


