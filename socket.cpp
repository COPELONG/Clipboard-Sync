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

// 修改后的剪贴板设置函数
void setClipboardText(const std::string &utf8Text) {
  if (OpenClipboard(nullptr)) {
    EmptyClipboard();

    // UTF-8 → UTF-16
    int wideSize =
        MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, nullptr, 0);
    if (wideSize == 0) {
      std::cerr << "[ERROR] UTF-8 转 UTF-16 失败" << std::endl;
      CloseClipboard();
      return;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wideSize * sizeof(wchar_t));
    if (!hMem) {
      std::cerr << "[ERROR] 内存分配失败" << std::endl;
      CloseClipboard();
      return;
    }

    wchar_t *wstr = static_cast<wchar_t *>(GlobalLock(hMem));
    MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, wstr, wideSize);
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();

    std::cout << "[INFO] 剪贴板已更新: " << utf8Text << std::endl;
  } else {
    std::cerr << "[ERROR] 无法打开剪贴板!" << std::endl;
  }
}

void handleClient(websocket::stream<tcp::socket> ws) {
  try {
    std::cout << "[INFO] 开始处理客户端请求..." << std::endl;
    ws.accept();

    beast::flat_buffer buffer;
    while (true) {
      std::cout << "[INFO] 等待接收消息..." << std::endl;
      try {
        ws.read(buffer);
        std::string receivedText = beast::buffers_to_string(buffer.data());
        std::cout << "[INFO] 收到消息: " << receivedText << std::endl;
        setClipboardText(receivedText);
        buffer.consume(buffer.size());
      } catch (const beast::system_error &se) {
        if (se.code() == websocket::error::closed) {
          std::cout << "[INFO] 客户端正常断开连接" << std::endl;
          break;
        }
        throw;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[ERROR] 客户端处理异常: " << e.what() << std::endl;
  }

  // 优雅关闭连接
  if (ws.is_open()) {
    try {
      ws.close(websocket::close_code::normal);
    } catch (...) {
    }
  }
}

void websocketServer() {
  try {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));

    std::cout << "[INFO] 服务器已启动，开始监听端口 8080..." << std::endl;

    while (true) {
      try {
        tcp::socket socket(ioc);
        std::cout << "[INFO] 等待新客户端连接..." << std::endl;
        acceptor.accept(socket);
        std::cout << "[INFO] 客户端已连接! 远程地址: "
                  << socket.remote_endpoint().address().to_string()
                  << std::endl;

        // 创建新 WebSocket 会话
        websocket::stream<tcp::socket> ws(std::move(socket));
        handleClient(std::move(ws));

        std::cout << "[INFO] 客户端连接已关闭，等待下一个连接..." << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "[ERROR] 连接处理异常: " << e.what() << std::endl;
        // 继续等待新连接
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[ERROR] 服务器致命错误: " << e.what() << std::endl;
  }
}

int main() {
  SetConsoleOutputCP(65001);
  websocketServer();
  return 0;
}
