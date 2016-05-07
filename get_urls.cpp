#include <boost/asio.hpp>
#include <fstream>
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
      get_urls <urls>...
      get_urls --urlsfile=<urlsfile>
      get_urls (-h | --help)
      
    Options:
      -h --help             Show this screen.
      --urlsfile=<urlsfile> Get files from an urls file.
)";


void show_url(ba::io_service & i, boost::string_ref url) {
  ip::tcp::socket socket(i);
  string const urlstr(url.begin(), url.end());
  url.remove_prefix(4); //Remove "www."
  string const host(url.begin(), url.end());
  
  ip::tcp::resolver::query q(urlstr, "http");
  ip::tcp::resolver resolver(i);
  ba::connect(socket, resolver.resolve(q));

  std::vector<unsigned char> vec(1024 * 1024);
  boost::system::error_code ec;

  string const request = "GET / HTTP/1.1\r\nHost: "
    + urlstr + "\r\nConnection: close\r\n\r\n";
  
  ba::write(socket, ba::buffer(request), ba::transfer_all());  
  auto bytes_read = ba::read(socket, ba::buffer(vec),
                             ba::transfer_all(),
                             ec);

  if (ec == ba::error::eof) {
    for (auto i = 0; i < bytes_read; ++i)
      cout << vec[i];
  }
  else 
    cout << "Error reading\n";
}


void show_url_async(ba::io_service & i, boost::string_ref url) {
  ip::tcp::socket socket(i);
  string const urlstr(url.begin(), url.end());
  url.remove_prefix(4); //Remove "www."
  string const host(url.begin(), url.end());
  
  ip::tcp::resolver::query q(urlstr, "http");
  ip::tcp::resolver resolver(i);
  ba::connect(socket, resolver.resolve(q));

  std::vector<unsigned char> vec(1024 * 1024);
  boost::system::error_code ec;

  string const request = "GET / HTTP/1.1\r\nHost: "
    + urlstr + "\r\nConnection: close\r\n\r\n";
  
  ba::write(socket, ba::buffer(request), ba::transfer_all());  
  auto bytes_read = ba::read(socket, ba::buffer(vec),
                             ba::transfer_all(),
                             ec);

  if (ec == ba::error::eof) {
    for (auto i = 0; i < bytes_read; ++i)
      cout << vec[i];
  }
  else 
    cout << "Error reading\n";
}


vector<string> const get_url_strings(string const & filename) {
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

int main(int argc, char * argv[]) {
  std::map<std::string, docopt::value> args
    = docopt::docopt(USAGE,
                     { argv + 1, argv + argc },
                     true, 
                     "Get urls");

  ba::io_service i;
  
  for (auto const & arg: args) {
    if (arg.first == "<urls>") {
      for (auto const & url : arg.second.asStringList()) {
        show_url(i, url);
      }
    }
    if (arg.first == "--urlsfile") {
      auto urls = get_url_strings(arg.second.asString());
      for (auto const & url : urls) {
        show_url(i, url);
      }
    }
  }
}
