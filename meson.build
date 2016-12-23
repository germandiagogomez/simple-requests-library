project('asio-experiments', 'cpp', default_options : ['cpp_std=c++14'])

boost_dep = dependency('boost', modules : ['system', 'context', 'coroutine', 'regex', 'thread'])

compiler = meson.get_compiler('cpp')

if host_machine.system() == 'darwin'
    docopt_basedir = 'deps/mac_os/'
else
    docopt_basedir = 'deps/linux/'
endif

docopt_lib = compiler.find_library('docopt', dirs : [
    join_paths(meson.current_source_dir(), join_paths(docopt_basedir, 'lib'))])
docopt_dep = declare_dependency(include_directories :
                                include_directories([join_paths(docopt_basedir, 'include')]),
                                dependencies : docopt_lib)

executable('get_urls', 'get_urls.cpp', dependencies :
           [boost_dep,
            docopt_dep])



