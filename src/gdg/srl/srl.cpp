#include "gdg/srl/srl.hpp"
#include "gdg/srl/exceptions.hpp"
#include <unordered_map>
#include <boost/regex.hpp>
#include <regex>
#include <iostream>

#if !DOCTEST_CONFIG_DISABLE
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#endif
#include "doctest/doctest.h"


using namespace std;
using namespace gdg::srl;

namespace {


string build_request(gdg::srl::string_view_t host, gdg::srl::string_view_t resource) {
    return "GET " + string(resource) + " HTTP/1.1\r\nHost: " + string(host)
        + "\r\nConnection: close\r\n\r\n";
}


TEST_CASE("build request") {
    CHECK(build_request("www.vandal.net", "/") ==
          "GET / HTTP/1.1\r\nHost: www.vandal.net\r\nConnection: close\r\n\r\n");

    CHECK(build_request("www.publico.es", "/") ==
          "GET / HTTP/1.1\r\nHost: www.publico.es\r\nConnection: close\r\n\r\n");

    CHECK(build_request("www.boost.org", "/LICENSE_1_0.txt") ==
          "GET /LICENSE_1_0.txt HTTP/1.1\r\nHost: www.boost.org\r\nConnection: close\r\n\r\n");

}


vector<gdg::srl::byte_t> async_read_fixed_body(gdg::srl::net::ip::tcp::socket & s,
                                               gdg::srl::net::streambuf & buf,
                                               size_t sizeInBytes,
                                               gdg::srl::net::yield_context yield) {
    boost::system::error_code ec{};

    auto bytesRead =
        net::async_read
        (s, buf,
         net::transfer_exactly(sizeInBytes),
         yield[ec]);
    if (ec && ec != net::error::eof)
        throw boost::system::system_error{ec};

    istream is(&buf);
    is >> noskipws;
    vector<byte_t> result(sizeInBytes);

    std::size_t i = 0;
    copy_n(istream_iterator<byte_t>{is},
           sizeInBytes, result.begin());

    return result;
}


vector<byte_t> async_read_chunked_body(net::ip::tcp::socket & s,
                                       net::streambuf & buf,
                                       net::yield_context yield) {
    size_t bytesRead{};
    size_t chunkSize{};
    istream is(&buf);
    is >> noskipws;
    boost::system::error_code ec{};
    vector<byte_t> result;
    while (true) {
        //Read the chunk size into the buffer
        bytesRead =
            net::async_read_until(s, buf, "\r\n",
                                  yield[ec]);

        if (ec && ec != net::error::eof)
            throw boost::system::system_error{ec};
        string chunkSizeStr;

        //Put chunk size in a string
        getline(is, chunkSizeStr);

        //Convert hex chunksize into a decimal number
        chunkSize = stoi(chunkSizeStr,
                         nullptr, 16);
        if (!chunkSize) {
            bytesRead = net::async_read
                (s, buf,
                 net::transfer_exactly(2),
                 yield[ec]);
            if (ec && ec != net::error::eof)
                throw boost::system::system_error{ec};
            break;
        }

        auto chunkBytesToRead = chunkSize;
        //read exactly chunkSize
        bytesRead = net::async_read
            (s, buf,
             net::transfer_exactly(chunkBytesToRead),
             yield[ec]);
        if (ec && ec != net::error::eof)
            throw boost::system::system_error{ec};

        copy_n(istream_iterator<char>{is},
               chunkBytesToRead,
               back_inserter(result));

        //Read trailing \r\n after the data
        bytesRead = net::async_read
            (s, buf,
             net::transfer_exactly(2),
             yield[ec]);
        if (ec && ec != net::error::eof)
            throw boost::system::system_error{ec};

        //Discard the \r\n after the data
        is.ignore(2);
    }
    return result;
}


vector<byte_t> async_read_body(net::ip::tcp::socket & s,
                               net::streambuf & buf,
                               bool readInChunks,
                               net::yield_context yield,
                               int contentLength = 0) {
    vector<byte_t> result;
    if (!readInChunks) {
        result = async_read_fixed_body(s, buf, contentLength, yield);
    }
    else {
        result = async_read_chunked_body(s, buf, yield);
    }
    return result;
}


unordered_map<string, string> get_headers(istream & input) {
    static const regex spaces("\\s*");
    string line;
    unordered_map<string, string> headers;

    while (getline(input, line) && !regex_match(line, spaces)) {
        auto const separatorIt = find(line.begin(), line.end(), ':');
        auto fieldName = string(line.begin(), separatorIt);
        transform(fieldName.begin(), fieldName.end(), fieldName.begin(), &::tolower);
        headers[fieldName] =
            string(find_if(next(separatorIt),
                           line.end(),
                           [](auto c) {
                               return !isspace(c); }),
                   line.end());
    }
    return headers;
}


TEST_CASE("get headers") {
    string const headersStr =
        R"--(Date: Mon, 27 Jan 2017 12:28:53 GMT
Server: Apache/2.2.14 (Win32)
Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT
Content-Length: 88
Content-Type: text/html
Connection: Closed)--";

    istringstream headersStream(headersStr);
    auto headers = get_headers(headersStream);

    CHECK(headers.size() == 6);
    auto it = headers.find("content-length");
    CHECK(it->second == "88");

    auto it2 = headers.find("date");
    CHECK(it2->second == "Mon, 27 Jan 2017 12:28:53 GMT");
}

} //anon namespace

namespace gdg {

namespace srl {


net::io_service & get_default_loop() {
    static net::io_service & svc =
        []() -> net::io_service & {
        static auto io = std::make_unique<net::io_service>();
        static net::io_service::work work{*io};
        shared_ptr<future<void>> f = make_shared<future<void>>();

        *f = std::async(std::launch::async,
                        [f] {
                            io->run();
                        });
        return *io;
    }();
    return svc;
}

future<pair<int, vector<byte_t>>> async_http_get(string_view_t host,
                                                 string_view_t resource,
                                                 chrono::steady_clock::duration timeOut) {
    return async_http_get(get_default_loop(),
                          host, resource, timeOut);
}


future<pair<int, vector<byte_t>>> async_http_get(net::io_service & io,
                                                 string_view_t host,
                                                 string_view_t resource,
                                                 chrono::steady_clock::duration timeOut) {
    promise<pair<int, vector<byte_t>>> result_promise;
    auto result = result_promise.get_future();
    net::spawn
        (io,
         [&io, hoststr=string(host), timeOut = timeOut,
          result_promise = move(result_promise), host, resource]
         (net::yield_context yield) mutable {
            ip::tcp::socket socket(io);

            try {
                atomic<bool> timeoutReached{false};
                net::steady_timer timer(io);
                timer.expires_from_now(timeOut);
                timer.async_wait([&timeoutReached](auto ec) {
                        if (ec != net::error::operation_aborted) {
                            timeoutReached = true;
                        }
                    });
                ip::tcp::resolver::query q(hoststr, "http");
                ip::tcp::resolver resolver(io);
                net::async_connect(socket, resolver.async_resolve(q, yield), yield);
                string const request = build_request(host, resource);


                net::async_write(socket, net::buffer(request), yield);
                net::streambuf buf;

                net::async_read_until(socket, buf, "\r\n\r\n",
                                      yield);

                if (timeoutReached) {
                    result_promise.set_exception(make_exception_ptr(timeout_exception{}));
                    return;
                }
                istream response(&buf);
                string httpVersion, retCode, retString;
                response >> httpVersion >> retCode;
                getline(response, retString);
                auto retCodeInt = std::stoi(retCode);
                if (retCodeInt != 200)
                    throw bad_request_exception(retCodeInt);
                string line;
                getline(response, line);

                auto const headers = get_headers(response);
                bool const readInChunks = [&headers]() {
                    if (headers.find("transfer-encoding") != headers.end())
                        return true;
                    else
                        return false;
                }();

                if (readInChunks) {
                    if (timeoutReached) {
                        result_promise.set_exception(make_exception_ptr(timeout_exception{}));
                        return;
                    }
                    result_promise.set_value({200, async_read_body(socket, buf, readInChunks, yield)});

                }
                else {
                    auto itContentLength = headers.find("content-length");
                    if (itContentLength == headers.end())
                        throw runtime_error
                            ("Cannot parse HTTP response -> "
                             "not supported: missing both content-length and transfer-encoding headers");
                    if (stoi(itContentLength->second) == 0) {
                        result_promise.set_value({200, {}});
                    }
                    else {
                        if (timeoutReached) {
                            result_promise.set_exception(make_exception_ptr(timeout_exception{}));
                            return;
                        }
                        result_promise.set_value
                            ({200,async_read_body(socket, buf, readInChunks, yield,
                                                  std::stoi(itContentLength->second))});
                    }
                }
                timer.cancel();
                if (timeoutReached)
                    result_promise.set_exception(make_exception_ptr(timeout_exception{}));
            }
            catch (...) {
                socket.close();
                result_promise.set_exception(current_exception());
            }
        });
    return result;
}


TEST_CASE("async http get without timeout") {
    net::io_service svc;
    net::io_service::work wk{svc};
    thread t([&svc] { svc.run(); });
    CHECK_NOTHROW((async_http_get(svc, "www.publico.es", "/", 8s).get()));
    CHECK_NOTHROW((async_http_get(svc, "www.elmundo.es", "/", 8s).get()));
    auto licenseText = async_http_get(svc, "www.boost.org",
                                      "/LICENSE_1_0.txt",
                                      8s).get();
    CHECK(licenseText.first == 200);
    CHECK(licenseText.second.size() == 1338);

    svc.stop();
    t.join();
}

TEST_CASE("async http get timeouts") {
    net::io_service svc;
    net::io_service::work wk{svc};
    thread t([&svc] { svc.run(); });
    CHECK_THROWS_AS(async_http_get("www.boost.org",
                                   "/LICENSE_1_0.txt",
                                   1ms, svc).get(),
                    timeout_exception);
    svc.stop();
    t.join();
}



} //namespace srl

} //namespace gdg
