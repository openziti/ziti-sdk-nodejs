cmake_minimum_required(VERSION 3.19)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project (ziti_sdk_nodejs)

# add_definitions(-DNAPI_VERSION=4)


file(GLOB SOURCE_FILES ./src/*.c ./src/.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
  # Generate node.lib
  execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

set(ZITI_BUILD_PROGRAMS OFF)
set(ZITI_BUILD_TESTS OFF)
option(USE_OPENSSL "use OpenSSL" ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/ziti-sdk-c)

target_link_libraries(${PROJECT_NAME} PRIVATE ziti)
