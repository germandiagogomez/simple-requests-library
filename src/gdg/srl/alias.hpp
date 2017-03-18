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
#include <boost/utility/string_view.hpp>

namespace gdg {
namespace srl {
using string_view_t = boost::string_view;
}
}
#elif USE_CPP17_STRING_VIEW
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
