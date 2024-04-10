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

#ifndef ZITI_TLS_UTILS_H
#define ZITI_TLS_UTILS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *ziti_nodejs_get_version(int verbose); 
extern const char *ziti_nodejs_git_branch();
extern const char *ziti_nodejs_git_commit();
extern void nodejs_hexDump(char *desc, void *addr, int len);


typedef const char *(*fmt_error_t)(int);
typedef int *(*cond_error_t)(int);

#define __FILENAME_NODEJS__ (__FILENAME__)


extern void init_nodejs_debug();

extern int ziti_nodejs_debug_level;
extern FILE *ziti_nodejs_debug_out;


/// for windows compilation NOGDI needs to be set:
// #define DEBUG_LEVELS(XX) \
//     XX(NONE) \
//     XX(ERROR) /*WINDOWS - see comment above wrt NOGDI*/ \
//     XX(WARN) \
//     XX(INFO) \
//     XX(DEBUG) \
//     XX(VERBOSE) \
//     XX(TRACE)


// enum DebugLevel {
// #define _level(n) n,
//     DEBUG_LEVELS(_level)
// #undef _level
// };

// #define container_of(ptr, type, member) ((type *) ((ptr) - offsetof(type, member)))

// TEMP: skip logging on windows
#ifdef WIN32
#define ZITI_NODEJS_LOG
#else
#define ZITI_NODEJS_LOG(level, fmt, ...) do { \
if (level <= ziti_nodejs_debug_level) {\
    long elapsed = get_nodejs_elapsed();\
    fprintf(ziti_nodejs_debug_out, "[%9ld.%03ld] " #level "\tziti-sdk-nodejs/%s:%d %s(): " fmt "\n",\
        elapsed/1000, elapsed%1000, __FILENAME_NODEJS__, __LINE__, __func__, ##__VA_ARGS__);\
}\
} while(0)
#endif

long get_nodejs_elapsed();


#ifdef __cplusplus
}
#endif

#endif //ZITI_TLS_UTILS_H
