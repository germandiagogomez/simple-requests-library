#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
#include <fstream>
#include <algorithm>
#include <future>
#include <boost/regex.hpp>
#include <regex>
#include <boost/utility/string_ref.hpp>
#include <docopt/docopt.h>
#include <vector>
#include <iostream>


using namespace std;
namespace ba = boost::asio;
namespace ip = ba::ip;

static const char USAGE[] =
    R"(Get urls.

    Usage:
      get_urls <urls>... [--async]
      get_urls -n=<numrequests> --urlsfile=<urlsfile> [--async]
      get_urls (-h | --help)

    Options:
      -h --help             Show this screen.
      --urlsfile=<urlsfile> Get files from an urls file.
      --async               Use asynchronous requests
      -n=<numrequests>      Number of urls to retrieve
)";


vector<unsigned char> get_url(ba::io_service & io, boost::string_ref url) {
    ip::tcp::socket socket(io);
    string const urlstr(url.begin(), url.end());

    ip::tcp::resolver::query q(urlstr, "http");
    ip::tcp::resolver resolver(io);
    ba::connect(socket, resolver.resolve(q));

    string const request = "GET / HTTP/1.1\r\nHost: "
        + urlstr + "\r\nConnection: Close\r\n\r\n";
    ba::write(socket, ba::buffer(request), ba::transfer_all());

    vector<unsigned char> vec(1024 * 1024);
    boost::system::error_code ec{};
    size_t write_pos{0u};
    size_t bytes_read{0u};
    while (!ec) {
        bytes_read = ba::read(socket, ba::buffer(&vec[write_pos],
                                                 vec.size() - write_pos), ec);
        write_pos += bytes_read;
        if (write_pos == vec.size()) vec.resize(vec.size() * 2);
    }

    if (ec == ba::error::eof) {
        vec.resize(write_pos);
        return vec;
    }
    else
        throw runtime_error("Error reading");
}


std::vector<unsigned char> async_read_body(boost::asio::ip::tcp::socket & s,
                                                boost::asio::streambuf & buf,
                                                boost::asio::yield_context yield) {
    std::vector<unsigned char> result;
    std::atomic<bool> operationTimeout{false};
    try {
        std::istream is(&buf);
        is >> std::noskipws;
        static const boost::regex chunkSizeRe{"\\s+[[:digit:]]+\r\n"};
        std::size_t bytesRead{};
        std::size_t chunkSize{};
        boost::asio::steady_timer timer(s.get_io_service());
        while (1) {
            bytesRead =
                boost::asio::async_read_until(s, buf, chunkSizeRe,
                                              yield);
            std::string chunkSizeStr;
            std::getline(is, chunkSizeStr);
            chunkSize = std::stoi(chunkSizeStr, nullptr, 16);
            if (chunkSize == 0)
                break;

            timer.expires_from_now(10s);
            timer.async_wait
                ([&s, &operationTimeout]
                 (boost::system::error_code const & e) {
                    if (e != boost::asio::error::operation_aborted) {
                        operationTimeout = true;
                        s.close();
                    }
                });
            
            bytesRead = boost::asio::async_read(s, buf,
                                                boost::asio::transfer_exactly(chunkSize + 2),
                                                yield);
            timer.cancel();
            std::copy_n(std::istream_iterator<char>{is},
                        chunkSize
                        ,
                        std::back_inserter(result));
            is.ignore(2);
        }
    }
    catch (boost::system::system_error const & err) {
        if (err.code() != boost::asio::error::eof)
            if (operationTimeout)
                throw std::system_error{std::make_error_code(std::errc::timed_out)};
            else 
                throw;
    }
    return result;
}


std::future<vector<unsigned char>> get_url_async(ba::io_service & i, boost::string_ref url) {
    std::promise<vector<unsigned char>> result_promise;
    auto result = result_promise.get_future();
    boost::asio::spawn
        (i,
         [&i, urlstr=string(url.begin(), url.end()),
          result_promise=std::move(result_promise)]
         (boost::asio::yield_context yield) mutable {
            try {
                ip::tcp::socket socket(i);

                ip::tcp::resolver::query q(urlstr, "http");
                ip::tcp::resolver resolver(i);
                ba::async_connect(socket, resolver.async_resolve(q, yield), yield);

                string const request = "GET / HTTP/1.1\r\nHost: "
                    + urlstr + "\r\n\r\n";
                ba::async_write(socket, ba::buffer(request), yield);

                boost::asio::streambuf buf;
                boost::asio::async_read_until(socket, buf, "\r\n\r\n",
                                              yield);
                std::istream response(&buf);
                std::string httpVersion, retCode, retString;
                response >> httpVersion >> retCode >> retString;
                std::cout << httpVersion << ' ' << retCode << ' ' << retString << '\n';
                std::string line;
                std::getline(response, line);
                static const std::regex spaces("\\s*");

                //Discard headers
                while (std::getline(response, line) && !std::regex_match(line, spaces));
                result_promise.set_value(async_read_body(socket, buf, yield));
            }
            catch (...) {
                result_promise.set_exception(std::current_exception());
            }
        });
    return result;
}


vector<string> const get_urls_strings(string const & filename,
                                      size_t howMany) {
    ifstream stream(filename);
    if (!stream) throw logic_error("File " + filename + " not found.");

    vector<string> result;
    result.reserve(howMany);
    string url;
    size_t retrieved = 0;
    while (stream >> url && retrieved < howMany) {
        result.push_back(url);
        ++retrieved;
    }
    return result;
}


vector<vector<unsigned char>> get_all_urls_async(vector<string> const & urls) {
    for (auto const & url : urls) 
        std::cout << "Url " << url << '\n';
    ba::io_service io;
    vector<std::future<vector<unsigned char>>> tasks;
    tasks.reserve(urls.size());
    for (auto const & url : urls)
        tasks.push_back(get_url_async(io, url));
    io.run();

    vector<vector<unsigned char>> urls_contents;
    int i = 0;
    for (auto & task : tasks) {
        try {
            urls_contents.push_back(task.get());
            ++i;
        }
        catch (std::exception const & e) {
            std::cout << e.what() << " from " << urls[i] << std::endl;
        }
    }
    return urls_contents;
}


vector<vector<unsigned char>> get_all_urls(vector<string> const & urls) {
    ba::io_service i;

    vector<vector<unsigned char>> urls_contents;
    urls_contents.reserve(urls.size());
    for (auto const & url : urls)
        urls_contents.push_back(get_url(i, url));
    return urls_contents;
}


int main(int argc, char * argv[]) {
    map<string, docopt::value> args
        = docopt::docopt(USAGE,
                         { argv + 1, argv + argc },
                         true,
                         "Get urls");
    vector<string> urls;
    size_t howManyUrls = 1;
    string urlsFile;
    for (auto const & arg : args) {
        if (arg.first == "<urls>" && arg.second) {
            for (auto & url : arg.second.asStringList()) {
                urls.push_back(move(url));
            }
        }
        if (arg.first == "--urlsfile" && arg.second) {
            urlsFile = arg.second.asString();
        }
        if (arg.first == "-n" && arg.second) {
            howManyUrls = arg.second.asLong();
        }
    }

    auto async_execution = args["--async"].asBool();
    
    if (!urlsFile.empty())
        urls = get_urls_strings(urlsFile, howManyUrls);
    cout << "Retrieving " << urls.size() << " urls\n";
    
    if (async_execution) {
        cout << "Async execution\n";
        get_all_urls_async(urls);
    }
    else {
        cout << "Sync execution\n";
        get_all_urls(urls);
    }
}
