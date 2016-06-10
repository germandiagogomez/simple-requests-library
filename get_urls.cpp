#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <fstream>
#include <algorithm>
#include <future>
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
      get_urls --urlsfile=<urlsfile> [--async]
      get_urls (-h | --help)

    Options:
      -h --help             Show this screen.
      --urlsfile=<urlsfile> Get files from an urls file.
)";


vector<unsigned char> get_url(ba::io_service & i, boost::string_ref url) {
  ip::tcp::socket socket(i);
  string const urlstr(url.begin(), url.end());

  ip::tcp::resolver::query q(urlstr, "http");
  ip::tcp::resolver resolver(i);
  ba::connect(socket, resolver.resolve(q));

  string const request = "GET / HTTP/1.1\r\nHost: "
    + urlstr + "\r\nConnection: close\r\n\r\n";
  ba::write(socket, ba::buffer(request), ba::transfer_all());

  std::vector<unsigned char> vec(1024 * 1024);
  boost::system::error_code ec{};
  std::size_t write_pos{0u};
  std::size_t bytes_read{0u};
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
    throw std::runtime_error("Error reading");
}


std::future<vector<unsigned char>> get_url_async(ba::io_service & i, boost::string_ref url) {
  std::promise<vector<unsigned char>> result_promise;
  auto result = result_promise.get_future();

  boost::asio::spawn
    (i,
     [&i, urlstr=string(url.begin(), url.end()),
      result_promise=move(result_promise)]
     (boost::asio::yield_context yield) mutable {
      try {
        ip::tcp::socket socket(i);

        ip::tcp::resolver::query q(urlstr, "http");
        ip::tcp::resolver resolver(i);
        ba::async_connect(socket, resolver.async_resolve(q, yield), yield);

        string const request = "GET / HTTP/1.1\r\nHost: "
          + urlstr + "\r\nConnection: close\r\n\r\n";

        ba::async_write(socket, ba::buffer(request), ba::transfer_all(), yield);

        std::vector<unsigned char> vec(1024 * 1024);
        boost::system::error_code ec{};
        std::size_t write_pos{0u};
        std::size_t bytes_read{0u};
        while (!ec) {
          bytes_read = ba::async_read(socket,
                                      ba::buffer(&vec[write_pos],
                                                 vec.size() - write_pos)
                                      , yield[ec]);
          write_pos += bytes_read;
          if (write_pos == vec.size()) vec.resize(vec.size() * 2);
        }

        if (ec == ba::error::eof) {
          vec.resize(write_pos);
          result_promise.set_value(move(vec));
        }
        else
          throw std::runtime_error("Error reading");
      }
      catch (std::exception const & e) {
        cerr << e.what() << '\n';
      }
    });
  return result;
}


vector<string> const get_urls_strings(string const & filename) {
  ifstream stream(filename);
  if (!stream) throw std::logic_error("File " + filename + " not found.");

  vector<string> result;
  result.reserve(10);
  string url;
  while (stream >> url) {
    result.push_back(url);
  }
  return result;
}


vector<vector<unsigned char>> get_all_urls_async(vector<string> const & urls) {
  ba::io_service i;
  vector<future<vector<unsigned char>>> tasks;
  tasks.reserve(urls.size());
  for (auto const & url : urls)
    tasks.push_back(get_url_async(i, url));
  i.run();

  vector<vector<unsigned char>> urls_contents;
  urls_contents.reserve(urls.size());
  for (auto & task : tasks) urls_contents.push_back(task.get());

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
  std::map<std::string, docopt::value> args
    = docopt::docopt(USAGE,
                     { argv + 1, argv + argc },
                     true,
                     "Get urls");
  vector<string> urls;

  for (auto const & arg: args) {
    if (arg.first == "<urls>" && arg.second) {
      for (auto & url : arg.second.asStringList()) {
        urls.push_back(move(url));
      }
    }
    if (arg.first == "--urlsfile" && arg.second) {
      urls = get_urls_strings(arg.second.asString());
    }
  }

  auto async_execution = args["--async"].asBool();
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
