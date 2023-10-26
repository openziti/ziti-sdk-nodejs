# TODO move this into CMake process
# --BUILD_DATE="$BUILD_DATE" --ZITI_BRANCH="`git branch --show-current`"
# --ZITI_COMMIT="`git rev-parse --short HEAD`" --ZITI_VERSION="`jq -r .version package.json`"
# --ZITI_OS="$RUNNER_OS" --ZITI_ARCH="x64"
find_package(Git)

if(NOT Git_FOUND)
    message(FATAL_ERROR "git not found")
endif ()

execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE ZITI_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE ZITI_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP BUILD_DATE UTC)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/package.json PACKAGE_JSON)
string(JSON ZITI_VERSION GET "${PACKAGE_JSON}" version)
unset(PACKAGE_JSON)