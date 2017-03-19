#ifndef GDG_SRL_EXCEPTIONS_HPP_
#define GDG_SRL_EXCEPTIONS_HPP_

#include <stdexcept>

namespace gdg {

namespace srl {

struct timeout_exception : std::runtime_error {
    timeout_exception() : std::runtime_error("timeout") {}
};

struct bad_request_exception : std::runtime_error {
    int errorCode;

    bad_request_exception(int eerrorCode) : std::runtime_error("Bad request"), errorCode(eerrorCode) {}
};


} //ns srl

} //ns gdg


#endif
