#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include "boost/beast/websocket.hpp"
#include "boost/asio/connect.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/version.hpp"
#include <windows.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace websocket = beast::websocket;

#include <windows.h>
#include <string>
#include <vector>

std::string getClipboardText() {
  if (!OpenClipboard(nullptr)) {
    return "";
  }

  HANDLE hData = GetClipboardData(CF_UNICODETEXT);
  if (!hData) {
    CloseClipboard();
    return "";
  }

  wchar_t *pszText = static_cast<wchar_t *>(GlobalLock(hData));
  if (!pszText) {
    CloseClipboard();
    return "";
  }

  std::wstring wstr(pszText);
  GlobalUnlock(hData);
  CloseClipboard();

  // 计算 UTF-8 编码需要的字节数
  int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
                                     nullptr, nullptr);
  if (utf8Size <= 0) {
    return "";
  }

  // 使用 std::vector<char> 以获取非 const 的 char*
  std::vector<char> utf8Str(utf8Size);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8Str.data(), utf8Size,
                      nullptr, nullptr);

  // std::string 支持从 vector 初始化
  return std::string(utf8Str.data());
}

// 剪贴板设置函数
void setClipboardText(const std::string &utf8Text) {
  if (OpenClipboard(nullptr)) {
    EmptyClipboard();

    int wideSize =
        MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, nullptr, 0);
    if (wideSize == 0) {
      CloseClipboard();
      return;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wideSize * sizeof(wchar_t));
    if (!hMem) {
      CloseClipboard();
      return;
    }

    wchar_t *wstr = static_cast<wchar_t *>(GlobalLock(hMem));
    MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, wstr, wideSize);
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
  }
}

#include <atomic>
#include <memory>
#include <thread>

void handleClient(websocket::stream<tcp::socket> ws) {
  class ThreadGuard {
  public:
    ThreadGuard(std::shared_ptr<std::atomic<bool>> stop, std::thread &&t)
        : stopFlag_(stop), thread_(std::move(t)) {}

    ~ThreadGuard() {
      if (stopFlag_)
        *stopFlag_ = true;
      if (thread_.joinable())
        thread_.join();
    }

  private:
    std::shared_ptr<std::atomic<bool>> stopFlag_;
    std::thread thread_;
  };

  try {
    ws.accept();
    auto ws_ptr =
        std::make_shared<websocket::stream<tcp::socket>>(std::move(ws));
    auto stopFlag = std::make_shared<std::atomic<bool>>(false);

    // 剪贴板监控线程
    std::thread monitorThread([ws_ptr, stopFlag]() {
      DWORD lastSequence = GetClipboardSequenceNumber();
      int copyCount = 0;
      auto lastCopyTime = std::chrono::steady_clock::now();

      while (!*stopFlag) {
        DWORD currentSequence = GetClipboardSequenceNumber();
        if (currentSequence != lastSequence) {
          lastSequence = currentSequence;
          ++copyCount;
          std::cout << "[INFO] 检测到剪贴板变化，累计次数: " << copyCount
                    << std::endl;

          auto now = std::chrono::steady_clock::now();
          auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
              now - lastCopyTime);

          if (copyCount >= 2 && elapsed.count() <= 3) {
            std::string clipboardText = getClipboardText();
            if (!clipboardText.empty()) {
              try {
                ws_ptr->write(net::buffer(clipboardText));
                std::cout << "DEBUG 发送内容: [" << clipboardText << "]"
                          << std::endl; // 检查是否有\n
                std::cout << "[SUCCESS] 已成功发送剪贴板内容到客户端"
                          << std::endl;
                MessageBeep(MB_OK); // 默认"叮"
                Sleep(5);
                MessageBeep(MB_ICONERROR); // 默认"咚"

                copyCount = 0;
              } catch (...) {
                *stopFlag = true; // 发送失败时停止
              }
            }
          } else if (elapsed.count() > 3) {
            copyCount = 0;
          }
          lastCopyTime = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    });

    ThreadGuard guard(stopFlag, std::move(monitorThread));

    // 消息接收循环
    beast::flat_buffer buffer;
    while (!*stopFlag) {
      try {
        ws_ptr->read(buffer);
        std::string receivedText = beast::buffers_to_string(buffer.data());
        setClipboardText(receivedText);
        std::cout << "[SUCCESS] 剪贴板以收到客户端数据" << std::endl;
        MessageBeep(48);
        buffer.consume(buffer.size());
      } catch (const beast::system_error &e) {
        if (e.code() == websocket::error::closed) {
          std::cout << "[INFO] 客户端正常断开连接" << std::endl;
          *stopFlag = true;
        } else {
          throw;
        }
      }
    }
  } catch (const beast::system_error &e) {
    if (e.code() != websocket::error::closed) {
      std::cerr << "[ERROR] 连接异常: " << e.what() << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[ERROR] 处理异常: " << e.what() << std::endl;
  }
}

void websocketServer() {
  try {
    net::io_context ioc{1};
    tcp::acceptor acceptor{ioc, {tcp::v4(), 8080}};

    std::cout << "✅ 服务器已启动，监听端口 8080" << std::endl;

    while (true) {
      try {
        tcp::socket socket{ioc};
        acceptor.accept(socket);

        std::cout << "🔌 新客户端连接: "
                  << socket.remote_endpoint().address().to_string()
                  << std::endl;

        // 为每个连接创建独立线程处理
        std::thread([s = std::move(socket)]() mutable {
          try {
            websocket::stream<tcp::socket> ws{std::move(s)};
            handleClient(std::move(ws));
          } catch (const std::exception &e) {
            std::cerr << "⚠️ 连接处理异常: " << e.what() << std::endl;
          }
        }).detach();

      } catch (const std::exception &e) {
        std::cerr << "⚠️ 接受连接异常: " << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "❌ 服务器致命错误: " << e.what() << std::endl;
  }
}

int main() {
  SetConsoleOutputCP(65001);
  websocketServer();
  return 0;
}