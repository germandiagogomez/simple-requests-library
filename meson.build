project('asio-experiments', 'cpp', default_options : ['cpp_std=c++14'])

boost_dep = dependency('boost', modules : ['context', 'coroutine'])
doctopt_sp = subproject('docopt')



executable('get_urls', 'get_urls.cpp', dependencies : boost_dep)



