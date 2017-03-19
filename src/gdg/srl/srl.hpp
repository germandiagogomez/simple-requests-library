#ifndef GDG_SRL_HPP_
#define GDG_SRL_HPP_

#include "gdg/srl/alias.hpp"
#include <future>
#include <utility>

namespace gdg {

namespace srl {

/** Gets the default event loop

    Default event loop in which \ref async_http_get runs.
 */
net::io_service & get_default_loop();


/** Gets a resource from a given host through http asynchronously.

    @host[in] the host machine. No effort will be done to remove trailing slashes
    @resource[in] the resource
    @timeOut[in] max timeout. Default is 30s
    @io the default io_service in which to run the async call. Default is get_default_loop()

    Currently the library is just successful when a 200 error code is returned.
    Any other return code will result in an exception.

    @return a future with a pair status code, vector of byte_t containing the response body
 */
std::future<std::pair<int, std::vector<byte_t>>>
async_http_get(string_view_t host,
               string_view_t resource = "/",
               std::chrono::steady_clock::duration timeOut = std::chrono::seconds(30),
               net::io_service & io = get_default_loop());



} //ns srl

} //ns gdg


#endif
