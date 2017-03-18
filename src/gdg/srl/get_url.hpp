#ifndef GDG_SRL_HPP_
#define GDG_SRL_HPP_

#include "gdg/srl/alias.hpp"
#include <future>
#include <vector>
#include <string>


namespace gdg {

namespace srl {

/** Gets the default event loop

    Default event loop in which \ref async_get_url runs.
 */
net::io_service & get_default_loop();


std::future<std::vector<byte_t>>
async_get_url(string_view_t url,
              std::chrono::steady_clock::duration timeOut = std::chrono::seconds(30),
              net::io_service & i = get_default_loop());


std::vector<std::vector<byte_t>>
async_get_urls(std::vector<std::string> const & urls,
               net::io_service & io =
               get_default_loop());


} //ns srl

} //ns gdg


#endif
