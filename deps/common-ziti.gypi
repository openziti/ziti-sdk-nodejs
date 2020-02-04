{
  'variables': {
      "toolset%":'',
  },
  'target_defaults': {
    'default_configuration': 'Release',
    'conditions': [
      [ 'toolset!=""', {
        'msbuild_toolset':'<(toolset)'
      }]
    ],
    'configurations': {
      'Debug': {
        'defines!': [
          'NDEBUG'
        ],
        'cflags_cc!': [
          '-O3',
          '-Os',
          '-DNDEBUG',
        ],
        'xcode_settings': {
          'OTHER_CPLUSPLUSFLAGS!': [
            '-O3',
            '-Os',
            '-DDEBUG',
          ],
          'GCC_OPTIMIZATION_LEVEL': '0',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES'
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': 1, # /EHsc
          }
        }
      },
      'Release': {
        'defines': [
          'NDEBUG'
        ],
        'xcode_settings': {
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

          'OTHER_CPLUSPLUSFLAGS!': [
            '-Os',
            '-O2',
            '-fno-strict-aliasing',
            '-g',
            '-fno-pie'
          ],
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-W',
            '-Wno-unused-parameter',
            '-Wno-unused-variable',
            '-Wno-unused-function',
            '-Wno-pointer-sign',
          ],
          'GCC_OPTIMIZATION_LEVEL': '3',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
          'DEAD_CODE_STRIPPING': 'YES',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES'
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': 1, # /EHsc
          }
        }
      }
    }
  }
}
