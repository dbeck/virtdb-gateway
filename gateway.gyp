{
  'variables': {
    'gateway_sources': [
                         'src/gateway/simple_gateway.cc',   'src/gateway/simple_gateway.hh',
                         'src/gateway/zmq_gateway.cc',      'src/gateway/zmq_gateway.hh',
                         'src/gateway/virtdb_gateway.cc',   'src/gateway/virtdb_gateway.hh',
                         # header only helpers
                         'src/gateway/exception.hh',
                       ],
  },
  'conditions': [
    ['OS=="mac"', {
      'target_defaults': {
        'default_configuration': 'Debug',
        'configurations': {
          'Debug': {
            'defines':  [ 'DEBUG', '_DEBUG', ],
            'cflags':   [ '-O0', '-g3', ],
            'ldflags':  [ '-g3', ],
            'xcode_settings': {
              'OTHER_CFLAGS':  [ '-O0', '-g3', ],
              'OTHER_LDFLAGS': [ '-g3', ],
            },
          },
          'Release': {
            'defines':  [ 'NDEBUG', 'RELEASE', ],
            'cflags':   [ '-O3', ],
            'xcode_settings': {
              'OTHER_CFLAGS':  [ '-O3', ],
            },
          },
        },
        'include_dirs': [ './src/', '/usr/local/include/', '/usr/include/', ],
        'cflags':   [ '-Wall', '-fPIC', '-std=c++11', ],
        'defines':  [ 'PIC', 'STD_CXX_11', '_THREAD_SAFE', 'GATEWAY_MAC_BUILD', 'NO_IPV6_SUPPORT', ],
        'xcode_settings': {
          'OTHER_CFLAGS':  [ '-std=c++11', ],
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        },
      },
    },],
    ['OS=="linux"', {
      'target_defaults': {
        'default_configuration': 'Debug',
        'configurations': {
          'Debug': {
            'defines':  [ 'DEBUG', '_DEBUG', ],
            'cflags':   [ '-O0', '-g3', ],
            'ldflags':  [ '-g3', ],
          },
          'Release': {
            'defines':  [ 'NDEBUG', 'RELEASE', ],
            'cflags':   [ '-O3', ],
          },
        },
        'include_dirs': [ './src/', '/usr/local/include/', '/usr/include/', ],
        'cflags':   [ '-Wall', '-fPIC', '-std=c++11', ],
        'defines':  [ 'PIC', 'STD_CXX_11', '_THREAD_SAFE', 'GATEWAY_LINUX_BUILD', ],
        'link_settings': {
          'ldflags':   [ '-Wl,--no-as-needed', ],
          'libraries': [ '-lrt', ],
        },
      },
    },],
  ],
  'targets' : [
    {
      'target_name':     'gateway',
      'type':            'static_library',
      'defines':       [ 'USING_GATEWAY_LIB',  ],
      'sources':       [ '<@(gateway_sources)', ],
    },
    {
      'target_name':     'gateway_test',
      'type':            'executable',
      'dependencies':  [ 'gateway', 'deps_/gtest/gyp/gtest.gyp:gtest_lib', ],
      'include_dirs':  [ './deps_/gtest/include/', ],
      'sources':       [ 'test/gateway_test.cc', ],
    },
  ],
}

