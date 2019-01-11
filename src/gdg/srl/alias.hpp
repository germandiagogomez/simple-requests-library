#ifndef GDG_SRL_ALIAS_HPP_
#define GDG_SRL_ALIAS_HPP_


#if USE_BOOST_ASIO
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

namespace gdg {
namespace srl {

namespace net = boost::asio;
}
}

#elif USE_STANDALONE_ASIO
#include <asio.hpp>

namespace gdg {
namespace srl {
namespace net = asio;
}
}

#endif

#if USE_BOOST_STRING_VIEW
#include <boost/version.hpp>
#if ((BOOST_VERSION / 100) % 1000) <= 60
#include <boost/utility/string_ref.hpp>
#else
#include <boost/utility/string_ref.hpp>
#endif

namespace gdg {
namespace srl {

using string_view_t =
#if ((BOOST_VERSION / 100) % 1000) <= 60
                                      boost::string_ref
#else
boost::string_view
#endif
;

}
}
#elif USE_STD_STRING_VIEW
#include <string_view>

namespace gdg {
namespace srl {
using string_view_t = std::string_view;
}
}
#endif

namespace gdg {
namespace srl {

namespace ip = net::ip;
using byte_t = unsigned char;


} //namespace srl
} //namespace gdg


#endif //include guards
