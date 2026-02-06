/*
Copyright NetFoundry Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <uv.h>
#include "utils.h"
#include <ziti/ziti_log.h>
#include <build_config.h>


#if !defined(BUILD_DATE)
#define BUILD_DATE unknown
#endif

#if !defined(ZITI_OS)
#define ZITI_OS unknown
#endif

#if !defined(ZITI_ARCH)
#define ZITI_ARCH unknown
#endif

#if !defined(ZITI_VERSION)
#define ZITI_VERSION unknown
#endif

#if !defined(ZITI_BRANCH)
#define ZITI_BRANCH no-branch
#define ZITI_COMMIT sha
#endif

const char* ziti_nodejs_get_version(int verbose) {
    if (verbose) {
        return "\n\tVersion:\t" to_str(ZITI_NODEJS_VERSION)
               "\n\tBuild Date:\t" to_str(BUILD_DATE)
               "\n\tGit Branch:\t" to_str(ZITI_NODEJS_BRANCH)
               "\n\tGit SHA:\t" to_str(ZITI_NODEJS_COMMIT)
               "\n\tOS:     \t" to_str(ZITI_OS)
               "\n\tArch:   \t" to_str(ZITI_ARCH)
               "\n\t";

    }
    return to_str(ZITI_NODEJS_VERSION);
}

const char* ziti_nodejs_git_branch() {
    return to_str(ZITI_BRANCH);
}

const char* ziti_nodejs_git_commit() {
    return to_str(ZITI_COMMIT);
}

#if _WIN32
LARGE_INTEGER frequency;
LARGE_INTEGER start;
LARGE_INTEGER end;
#else
struct timespec starttime;
#endif

void init_nodejs_debug(uv_loop_t *loop) {
    char *level = getenv("ZITI_NODEJS_LOG");
    if (level != NULL) {
        int l = (int) strtol(level, NULL, 10);
        ziti_log_set_level(l, ZITI_LOG_MODULE);
    }
    ziti_log_init(loop, ZITI_LOG_DEFAULT_LEVEL, NULL);

#if _WIN32
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);
#else
    clock_gettime(CLOCK_MONOTONIC, &starttime);
#endif
}

