// Copyright 2020 Andreytorix
#include "nlohmann/json.hpp"
//#include <boost/algorithm/string/regex.hpp>
//#include <boost/regex.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
/*#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS*/
#include <filesystem>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <thread>
#include <shared_mutex>
//#include "suggestions.hpp"
#include "../include/suggestions.hpp"
namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
std::shared_mutex mt;
std::vector<suggestion> items;

class http_connection : public std::enable_shared_from_this<http_connection> {
public:
explicit http_connection(tcp::socket socket) : socket_(std::move(socket)) {}
  // Initiate the asynchronous operations associated with the connection.
  void start() {
    read_request();
    check_deadline();
  }

private:
  // The socket for the currently connected client.
  tcp::socket socket_;

  // The buffer for performing reads.
  beast::flat_buffer buffer_{8192};

  // The request message.
  http::request<http::dynamic_body> request_;

  // The response message.
  http::response<http::dynamic_body> response_;

  // The timer for putting a deadline on connection processing.
  net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

  // Asynchronously receive a complete request message.
  void read_request() {
    auto self = shared_from_this();

    http::async_read(
        socket_, buffer_, request_,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
          boost::ignore_unused(bytes_transferred);
          if (!ec)
            mt.lock();
          self->process_request();
          mt.unlock();
        });
  }

  // Determine what needs to be done with the request message.
  void process_request() {
    response_.version(request_.version());
    response_.keep_alive(false);
    if (request_.target() == "/v1/api/suggest") {
      if (request_.method() == http::verb::post) {
        // std::cout <<request_  << std::endl;
        response_.result(http::status::ok);
        std::string body{boost::asio::buffers_begin(request_.body().data()),
                         boost::asio::buffers_end(request_.body().data())};

        //std::vector<std::string> coll;
        //boost::algorithm::split_regex(coll, body, boost::regex(":"));
        nlohmann::json n = nlohmann::json::parse(body);
        std::string client = n["input"];
        // std::cout <<n.dump(4)<<std::endl;
        std::cout << client << std::endl; //<< "input: "
        nlohmann::json js;
        js["suggestions"] = nlohmann::json::array();
        // if (coll[0] == "\"input\"") {

        // std::string otvet = "";
        int position = 0;

        for (uint64_t i = 0; i < items.size(); ++i) {
          if (items[i].id == client) {
            js["suggestions"][position]["Position: "] =
                std::to_string(position);
            js["suggestions"][position]["Text"] = items[i].name;
            ++position;
          }
          // std::cout << "\"" + items[i].id + "\"" << std::endl;
        }
        //}

        beast::ostream(response_.body()) //ответ
            << js.dump(4);
      }
    }

    write_response();
  }

  // Asynchronously transmit the response message.
  void write_response() {
    auto self = shared_from_this();

    response_.content_length(response_.body().size());

    http::async_write(socket_, response_,
                      [self](beast::error_code ec, std::size_t) {
                        self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                        self->deadline_.cancel();
                      });
  }

  // Check whether we have spent enough time on this connection.
  void check_deadline() {
    auto self = shared_from_this();

    deadline_.async_wait([self](beast::error_code ec) {
      if (!ec) {
        // Close socket to cancel any outstanding operation.
        self->socket_.close(ec);
      }
    });
  }
};

// "Loop" forever accepting new connections.
void http_server(tcp::acceptor &acceptor, tcp::socket &socket) {
  acceptor.async_accept(socket, [&](beast::error_code ec) {
    if (!ec)
      std::make_shared<http_connection>(std::move(socket))->start();
    http_server(acceptor, socket);
  });
}
void update(std::string dpath) {
  while (true) {
    mt.lock();
    items = getFromJson(dpath);
    mt.unlock();
    std::this_thread::sleep_for(std::chrono::minutes(15));
  }
}
int main(int argc, char *argv[]) {
  try {
    // Check command line arguments.
    if (argc != 4) {
      std::cerr << "Usage: " << argv[0] << " <address> <port>\n";
      std::cerr << "  For IPv4, try:\n";
      std::cerr << "    receiver 0.0.0.0 80\n";
      std::cerr << "  For IPv6, try:\n";
      std::cerr << "    receiver 0::0 80\n";
      return EXIT_FAILURE;
    }

    if (std::filesystem::exists(argv[3])) {
      // items = getFromJson(argv[3]);
      //запуск потока обновления данных
      std::string dir(argv[3]);
      std::thread updater(update, dir);

      auto const address = net::ip::make_address(argv[1]);
      unsigned short port = static_cast<unsigned short>(std::atoi(argv[2]));

      net::io_context ioc{1};

      tcp::acceptor acceptor{ioc, {address, port}};
      tcp::socket socket{ioc};
      http_server(acceptor, socket);

      ioc.run();
    } else {
      std::cout << "file not found" << std::endl;
      return EXIT_FAILURE;
    }
  } catch (std::exception const &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
