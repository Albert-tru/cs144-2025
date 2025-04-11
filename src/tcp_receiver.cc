#include "tcp_receiver.hh"
#include "debug.hh"
#include "byte_stream.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  //reset为啥要set_error？
  //const_cast: 去除对象的常量性，也就是说把const writer&转变为writer&
  if(message.RST == true){
    const_cast<Writer&>(reassembler_.writer()).set_error();
  }

  uint64_t stream_index;

  if(message.SYN){
    is_syn = true;
    _isn = message.seqno;
    stream_index = 0;
  }
  else{
    //checkpoint(参考点的设置）：reassembler_.writer().bytes_pushed()表示当前写入输出流的字数
    //                       新接收到的序列号通常会接近这个位置 
    stream_index = message.seqno.unwrap(message.seqno, reassembler_.writer().bytes_pushed());
  }
  reassembler_.insert(stream_index, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  bool rst_flag = reassembler_.writer().has_error();
  //处理确认号(希望接受的字节的绝对序列号),流关闭的话需要加上FIN
  uint16_t abs_ackno = reassembler_.writer().bytes_pushed() + 1;
  if(reassembler_.writer().is_closed()){
    abs_ackno += 1;
  }
  Wrap32 ackno = _isn + abs_ackno;

  // 计算窗口大小
  uint16_t window_size = static_cast<uint16_t>(
    min(reassembler_.writer().available_capacity(), static_cast<uint64_t>(UINT16_MAX))
  );

  if (!is_syn) {
    return {
      .ackno = std::optional<Wrap32>{},
      .window_size = window_size,
      .RST = rst_flag
    };
  }

  return {
    .ackno = ackno,
    .window_size = window_size,
    .RST = rst_flag
  };
}
