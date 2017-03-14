#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
#include <fstream>
#include <algorithm>
#include <future>
#include <unordered_map>
#include <boost/regex.hpp>
#include <regex>
#include <boost/utility/string_view.hpp>
#include <vector>


#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

using namespace std;
namespace net = boost::asio;
namespace ip = net::ip;

using string_view_t = boost::string_view;
using byte_t = unsigned char;


namespace {

string build_request(string_view_t url) {
    return "GET / HTTP/1.1\r\nHost: " + string(url) + "\r\nConnection: close\r\n\r\n";
}


TEST_CASE("build request") {
    CHECK(build_request("www.vandal.net") ==
          "GET / HTTP/1.1\r\nHost: www.vandal.net\r\nConnection: close\r\n\r\n");
}


vector<byte_t> async_read_fixed_body(net::ip::tcp::socket & s,
                                     net::streambuf & buf,
                                     size_t sizeInBytes,
                                     net::yield_context yield) {
    boost::system::error_code ec{};
    auto bytesRead =
        net::async_read
        (s, buf,
         net::transfer_exactly(sizeInBytes),
         yield[ec]);

    if (ec != net::error::eof)
        throw boost::system::system_error{ec};
    istream is(&buf);
    vector<byte_t> result(bytesRead);

    std::size_t i = 0;
    for (auto it = istream_iterator<byte_t>();
         it != istream_iterator<byte_t>{}; ++it)
        result[i++] = *it;
    return result;
}


vector<unsigned char> async_read_chunked_body(net::ip::tcp::socket & s,
                                              net::streambuf & buf,
                                              net::yield_context yield) {
    size_t bytesRead{};
    size_t chunkSize{};
    istream is(&buf);
    is >> noskipws;
    boost::system::error_code ec{};
    vector<unsigned char> result;
    static const boost::regex chunkSizeRe{"\\s+[[:digit:]]+\r\n"};
    while (1) {
        bytesRead =
            net::async_read_until(s, buf, chunkSizeRe,
                                  yield[ec]);
        if (ec != net::error::eof)
            throw boost::system::system_error{ec};

        string chunkSizeStr;
        getline(is, chunkSizeStr);

        chunkSize = stoi(chunkSizeStr, nullptr, 16);
        if (!chunkSize)
            break;

        bytesRead = net::async_read
            (s, buf,
             net::transfer_exactly(chunkSize),
             yield[ec]);
        if (ec != net::error::eof)
            throw boost::system::system_error{ec};

        copy_n(istream_iterator<char>{is},
               bytesRead,
               back_inserter(result));

        bytesRead = net::async_read
            (s, buf,
             net::transfer_exactly(2),
             yield[ec]);
        if (ec != net::error::eof)
            throw boost::system::system_error{ec};
        is.ignore(2);
    }
    return result;
}


vector<byte_t> async_read_body(net::ip::tcp::socket & s,
                               net::streambuf & buf,
                               bool readInChunks,
                               net::yield_context yield,
                               chrono::steady_clock::duration timeout = 10s,
                               int contentLength = 0) {
    atomic<bool> operationTimeout{false};
    net::steady_timer timer(s.get_io_service());
    timer.expires_from_now(timeout);
    timer.async_wait
        ([&s, &timeout]
         (boost::system::error_code const & e) {
            if (e != net::error::operation_aborted) {
                s.close();
            }
        });

    vector<byte_t> result;
    if (!readInChunks) {
        result = async_read_fixed_body(s, buf, contentLength, yield);

    }
    else {
        result = async_read_chunked_body(s, buf, yield);
    }
    timer.cancel();
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


net::io_service & get_default_loop() {
    static net::io_service & svc =
        []() -> net::io_service & {
        static net::io_service io;
        shared_ptr<future<void>> f = make_shared<future<void>>();

        *f = std::async(std::launch::async,
                        [f] {
                            io.run();
                        });
        return io;
    }();


    return svc;
}


} //anon namespace


future<vector<byte_t>> async_get_url(string_view_t url,
                                     chrono::steady_clock::duration timeOut = 30s,
                                     net::io_service & i = get_default_loop()) {
    promise<vector<unsigned char>> result_promise;
    auto result = result_promise.get_future();

    net::spawn
        (i,
         [&i, urlstr=string(url), timeOut = timeOut,
          result_promise = move(result_promise), url]
         (net::yield_context yield) mutable {
            ip::tcp::socket socket(i);
            try {
                ip::tcp::resolver::query q(urlstr, "http");
                ip::tcp::resolver resolver(i);
                net::async_connect(socket, resolver.async_resolve(q, yield), yield);
                string const request = build_request(url);

                net::async_write(socket, net::buffer(request), yield);
                net::streambuf buf;
                net::async_read_until(socket, buf, "\r\n\r\n",
                                      yield);


                istream response(&buf);
                string httpVersion, retCode, retString;
                response >> httpVersion >> retCode;
                getline(response, retString);
                if (retCode[0] == '4') {
                    throw runtime_error("URL returned code " + retCode + ". Closing...");
                }
                else if (retCode[0] == '3') {
                    throw runtime_error("URL returned code " + retCode + ". Closing...");
                }
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
                    result_promise.set_value(async_read_body(socket, buf, readInChunks, yield,
                                                             timeOut));
                }
                else {
                    auto itContentLength = headers.find("content-length");
                    if (itContentLength == headers.end())
                        throw runtime_error
                            ("Cannot parse HTTP response -> "
                             "not supported: missing both content-length and transfer-encoding headers");
                    if (stoi(itContentLength->second) == 0) {
                        result_promise.set_value({});
                    }
                    else {
                        result_promise.set_value
                            (async_read_body(socket, buf, readInChunks, yield,
                                             timeOut, std::stoi(itContentLength->second)));
                    }
                }

            }
            catch (...) {
                socket.close();
                result_promise.set_exception(current_exception());
            }
        });
    return result;
}


TEST_CASE("async get url") {
    net::io_service svc;
    net::io_service::work wk{svc};
    thread t([&svc] { svc.run(); });

    CHECK_NOTHROW((async_get_url("www.publico.es", 10s, svc).get()));


    svc.stop();
    t.join();
}


vector<vector<byte_t>> async_get_urls(vector<string> const & urls,
                                      net::io_service & io =
                                      get_default_loop()) {
    vector<pair<string, future<vector<unsigned char>>>> tasks;
    tasks.reserve(urls.size());
    for (auto const & url : urls) {
        tasks.push_back({url, async_get_url(url)});
    }

    vector<vector<byte_t>> urls_contents;
    //vector<vector<byte_t>> exceptions;    TODO <- accumulate exceptions
    for (auto & task : tasks) {
        try {
            urls_contents.push_back(task.second.get());
        }
        catch (exception const & e) {
            throw; //placeholder
        }
    }
    return urls_contents;
}
