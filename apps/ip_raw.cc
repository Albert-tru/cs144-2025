#include "socket.hh"

using namespace std;

class RawSocket : public DatagramSocket
{
public:
  RawSocket() : DatagramSocket( AF_INET, SOCK_RAW, IPPROTO_UDP) {}
  //发送UDP数据报，需要包含发送的套接字、IP地址、端口号、发送的消息
  void sendUDPPacket(const Address& ip, const string& message)
  {
    sendto(ip,message);
  }
};

int main()
{
  // construct an Internet or user datagram here, and send using the RawSocket as in the Jan. 10 lecture
  RawSocket socket;
  Address add ("127.0.0.1",12345);
  string message = "Hello UDP!";
  socket.sendUDPPacket(add, message);
  return 0;
}
