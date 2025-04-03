#include <string>
#include <iostream>
#include "boost/beast/websocket.hpp"
#include "boost/asio/connect.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/version.hpp"
#include <windows.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = beast::websocket; // 应使用 boost::beast::websocket

namespace buried {

class WebSocketClient {
public:
  WebSocketClient(const std::string &host, const std::string &port)
      : host_(host), port_(port) {}

  bool ConnectAndSend() {
    try {
      boost::asio::io_context ioc;

      // 解析并连接到服务器
      tcp::resolver resolver(ioc);
      websocket::stream<tcp::socket> ws(ioc);

      auto const results = resolver.resolve(host_, port_);
      boost::asio::connect(ws.next_layer(), results.begin(), results.end());

      // 完成 WebSocket 握手
      ws.handshake(host_, "/");
      std::cout << "[INFO] WebSocket 握手成功" << std::endl;

      // 发送消息到 WebSocket 服务器
      std::string message = "Hello, WebSocket server!";
      ws.write(boost::asio::buffer(message));
      std::cout << "[INFO] 发送消息: " << message << std::endl;

      // 接收服务器的响应
      beast::flat_buffer buffer;
      ws.read(buffer);
      std::string receivedText = beast::buffers_to_string(buffer.data());
      std::cout << "[INFO] 收到消息: " << receivedText << std::endl;

      // 关闭 WebSocket 连接
      ws.close(websocket::close_code::normal);
      std::cout << "[INFO] WebSocket 连接已关闭" << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[ERROR] 发生错误: " << e.what() << std::endl;
      return false;
    }
    return true;
  }

private:
  std::string host_;
  std::string port_;
};

} // namespace buried

int main() {
  try {
    // 创建 WebSocket 客户端对象并连接到服务器
    buried::WebSocketClient client("localhost", "8080");
    if (client.ConnectAndSend()) {
      std::cout << "[INFO] WebSocket 客户端成功与服务器通信" << std::endl;
    } else {
      std::cerr << "[ERROR] WebSocket 客户端发生错误" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[ERROR] 程序发生异常: " << e.what() << std::endl;
  }
  return 0;
}
