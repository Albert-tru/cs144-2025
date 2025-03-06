#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

void get_URL( const string& host, const string& path )
{
    // 创建一个 TCP 套接字
    TCPSocket socket;

    try {
        // 连接到指定的主机和端口（默认 HTTP 端口 80）
        socket.connect(Address(host, "http"));

        // 构建 HTTP 请求
        string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Connection: close\r\n\r\n";

        // 发送 HTTP 请求
        socket.write(request);

        // 接收并打印响应
        std::string buffer;
        while (not socket.eof()) {
            socket.read(buffer);
            cout << buffer;
        }

        // 关闭套接字
        socket.close();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    // 修改为显式指定命名空间
    auto args = std::span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
