#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity), bytes_pushed_(0), bytes_popped_(0), is_closed_(false) {}

//往缓存区写入数据
void Writer::push( string data )
{
  //只需考虑缓存区容量，缓存区容量大于data的长度，直接写入即可
  // if (buffer_.size() + data.size() <= available_capacity()) {
  //   buffer_ += data;
  //   bytes_pushed_ += data.size();
  // }
  uint64_t available = available_capacity();
  uint64_t to_push = std::min(available, static_cast<uint64_t>(data.size()));
  if (to_push > 0) {
      buffer_.append(data.substr(0, to_push));
      bytes_pushed_ += to_push;
  }
}

//关闭写入流
void Writer::close()
{
  is_closed_ = true;
}

//判断写入流是否关闭
bool Writer::is_closed() const
{
  return is_closed_;
}

//返回缓存区剩余容量
uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size(); 
}

//返回写入流写入的字节数
uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_; 
}

//查看缓冲区接下来的字节数据
string_view Reader::peek() const
{   
  //bytes_buffered()：缓冲区已缓冲的字节数
  return string_view(buffer_).substr(0, bytes_buffered());
}

//从缓冲区弹出len个字节
void Reader::pop( uint64_t len )
{
  //如果len大于缓冲区已缓冲的字节数，直接弹出缓冲区所有字节
  if (len >= bytes_buffered()) {
    buffer_.clear();
    bytes_popped_ += bytes_buffered();
  }
  //否则，弹出len个字节
  else {
    buffer_.erase(0, len);
    bytes_popped_ += len;
  } 
}

//检查读取流是否已完成
bool Reader::is_finished() const
{
  //是否所有数据都已读取且写入端已关闭
  return bytes_buffered() == 0 && is_closed_;
}

//返回缓冲区已缓冲的字节数
uint64_t Reader::bytes_buffered() const
{
  return bytes_pushed_ - bytes_popped_;
}

//返回读取流读取的字节数
uint64_t Reader::bytes_popped() const
{
  return bytes_popped_; 
}

