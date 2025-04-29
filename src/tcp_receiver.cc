#include "tcp_receiver.hh"
#include "debug.hh"
#include "byte_stream.hh"
#include "iostream"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  //reset为啥要set_error？
  //const_cast: 去除对象的常量性，也就是说把const writer&转变为writer&
  if(message.RST == true){
    const_cast<Writer&>(reassembler_.writer()).set_error();
    return;
  }

  uint64_t stream_index = 0;

  if(message.SYN && !is_syn){
    is_syn = true;
    _isn = message.seqno;
  }
  //如果是非SYN消息的话，需要unwrap转成stream_index
  if(!message.SYN){
    //unwrap(Wrap32 zero_point, uint64_t checkpoint) 
    //zero_point：起始序列号，checkpoint：参考点，通常是当前写入输出流的字节数
    //checkpoint(参考点的设置）：reassembler_.writer().bytes_pushed()表示当前写入输出流的字数
    //                       新接收到的序列号通常会接近这个位置 
    stream_index = message.seqno.unwrap(_isn, reassembler_.writer().bytes_pushed()) - 1;
    cerr<<"stream_index: "<<stream_index<<"   data: "<<message.payload<<endl;
  }
  reassembler_.insert(stream_index, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  bool rst_flag = reassembler_.writer().has_error();
  uint64_t abs_ackno = 0;
  if (is_syn) {
      // SYN 标志占一个序列号，所以 abs_ackno 初始化为 1
      abs_ackno = 1;
      // 累加已经写入的字节数
      abs_ackno += reassembler_.writer().bytes_pushed();
      cerr<<"abs_ackno: "<<reassembler_.writer().bytes_pushed()<<endl;
  }

  // 若流关闭，说明收到了 FIN 标志，FIN 占一个序列号，abs_ackno 加 1
  if (reassembler_.writer().is_closed()) {
    cerr<<"FIN"<<endl;
    abs_ackno += 1;
  }
  Wrap32 ackno = is_syn ? _isn + abs_ackno : Wrap32{0};

  // 计算窗口大小
  uint16_t window_size = static_cast<uint16_t>(
      std::min(reassembler_.writer().available_capacity(), static_cast<uint64_t>(UINT16_MAX))
  );

  if (!is_syn) {
      return {
          .ackno = std::nullopt,
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
