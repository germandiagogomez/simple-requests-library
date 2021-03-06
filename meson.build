project('simple-requests-library', 'cpp', default_options : ['cpp_std=c++14'])

if get_option('network-library') == 'boost_asio'
  add_project_arguments('-DBOOST_COROUTINES_NO_DEPRECATION_WARNING',
                        '-DBOOST_COROUTINE_NO_DEPRECATION_WARNING', language : 'cpp')
  network_dep = dependency('boost', modules : ['system', 'chrono', 'context', 'coroutine', 'regex'])
endif


doctest_includes = include_directories('thirdParty/doctest/')
doctest_dep = declare_dependency(include_directories : doctest_includes)


executable('functional_tests', ['src/gdg/srl/alias.hpp',
                                'src/gdg/srl/exceptions.hpp',
                                'src/gdg/srl/srl.cpp',
                                'src/gdg/srl/srl.hpp'],
           dependencies :
             [network_dep,
              doctest_dep,
              dependency('threads')],
           include_directories : include_directories('src'),
           cpp_args : ['-DUSE_' + get_option('network-library').to_upper(),
                       '-DUSE_' + get_option('string-view-library').to_upper()],
           build_by_default : false)



simple_requests = static_library('simple_requests', ['src/gdg/srl/alias.hpp',
                                                     'src/gdg/srl/exceptions.hpp',
                                                     'src/gdg/srl/srl.cpp',
                                                     'src/gdg/srl/srl.hpp'],
                                 dependencies :
                                   [network_dep,
                                    doctest_dep],
                                 include_directories : include_directories('src'),
                                 cpp_args : ['-DUSE_' + get_option('network-library').to_upper(),
                                             '-DUSE_' + get_option('string-view-library').to_upper(),
                                             '-DDOCTEST_CONFIG_DISABLE'])


simple_requests_dep = declare_dependency(link_with : simple_requests,
                                         include_directories : include_directories('src'),
                                         compile_args : ['-DUSE_' + get_option('network-library').to_upper(),
                                                         '-DUSE_' + get_option('string-view-library').to_upper(),
                                                         '-DDOCTEST_CONFIG_DISABLE'],
                                         dependencies : [network_dep, doctest_dep,
                                                         dependency('threads')])


executable('get_urls', 'examples/get_urls.cpp',
           dependencies : [simple_requests_dep],
           cpp_args : ['-DUSE_' + get_option('network-library').to_upper(),
                       '-DUSE_' + get_option('string-view-library').to_upper(),
                       '-DDOCTEST_CONFIG_DISABLE'],
           build_by_default : false)
