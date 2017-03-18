project('simple-requests-library', 'cpp', default_options : ['cpp_std=c++14'])

asio_dependency = get_option('network-library')

if asio_dependency == 'boost_asio'
   add_project_arguments('-DBOOST_COROUTINES_NO_DEPRECATION_WARNING',
                      '-DBOOST_COROUTINE_NO_DEPRECATION_WARNING', language : 'cpp')
   network_dep = dependency('boost', modules : ['system', 'context', 'coroutine', 'regex'])
endif


doctest_includes = include_directories('thirdParty/doctest/')
doctest_dep = declare_dependency(include_directories : doctest_includes)



executable('simple_requests', ['src/gdg/srl/alias.hpp',
                               'src/gdg/srl/srl.cpp',
                               'src/gdg/srl/srl.hpp'],
           dependencies :
           [network_dep,
            doctest_dep],
           include_directories : include_directories('src'),
           cpp_args : ['-DUSE_' + get_option('network-library').to_upper(),
                       '-DUSE_' + get_option('string-view-library').to_upper()])
