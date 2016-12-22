project('asio-experiments', 'cpp', default_options : ['cpp_std=c++14'])

boost_dep = dependency('boost', modules : ['system', 'context', 'coroutine', 'regex', 'thread'])

compiler = meson.get_compiler('cpp')

docopt_lib = compiler.find_library('docopt', dirs : [
    join_paths(meson.current_source_dir(), 'deps/mac_os/lib')])
                                   


docopt_dep = declare_dependency(include_directories :
                                include_directories(['deps/mac_os/include']),
                                dependencies : docopt_lib)

executable('get_urls', 'get_urls.cpp', dependencies :
           [boost_dep,
            docopt_dep])



