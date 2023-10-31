cmake_minimum_required(VERSION 3.19)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

include(cmake/variables.cmake)

message(NOTICE "branch: ${ZITI_BRANCH}/${ZITI_COMMIT}")

project (ziti_sdk_nodejs)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/build_config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/include/build_config.h
)

# add_definitions(-DNAPI_VERSION=4)

if (WIN32)
  # make sure TLSUV picks it up
  include_directories(${CMAKE_JS_INC})
endif (WIN32)

file(GLOB SOURCE_FILES ./src/*.c ./src/.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include
        PRIVATE ${CMAKE_JS_INC}
)

if (WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)

  #
  # nodejs contains all needed libuv stuff
  # trick TLSUV to link against it
  add_library(libuv::uv SHARED IMPORTED)
  set_target_properties(libuv::uv PROPERTIES
          IMPORTED_LOCATION ${CMAKE_JS_LIB}
          IMPORTED_IMPLIB ${CMAKE_JS_LIB}
  )
  set(TLSUV_LIBUV_LIB libuv::uv)
endif (WIN32)


if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
  # Generate node.lib
  execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/ziti-sdk-c)

target_link_libraries(${PROJECT_NAME} PRIVATE ziti ${CMAKE_JS_LIB})
if (WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp version ntdll)
endif (WIN32)