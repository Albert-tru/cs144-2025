#include "reassembler.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  bool pushed = false;
  cerr<<"---------------------------------------------------------"<<endl;
  cerr<<"now_index: "<<now_index<<" first_index: "<<first_index<<"  string: "<<data<<endl;
  cerr<<"output_capa: "<<output_.writer().available_capacity()<<" capa: "<<available_capacity<<endl;
  //如果是紧挨着当前索引的字符串
  //且不是最后一个字符串（为了防止数据流中只有一个字符串时的重复写入）
  if ( first_index == now_index && !is_last_substring ) {
    //容量足够则写入
    cerr<<"Is next————"<<now_index<<" "<<data.size()<<" "<<available_capacity<<endl;
    if ( data.size() <= available_capacity ){
      output_.writer().push( data );
      pushed = true;
      now_index = first_index + data.size();
      //available_capacity -= data.size();
      cerr<<"wrted '%s'"<<data<<endl;
    }
  }

  //循环查看缓冲区中的是否是下一个字符串
  while( !buffer.empty() && buffer.begin()->first == now_index ){
    string temp = buffer.begin()->second;
    //容量足够则写入
    if ( temp.size() <= available_capacity){
      output_.writer().push( temp );
      now_index = first_index + temp.size();
      available_capacity += temp.size();
    }
    //删除缓冲区中已写入的字符串
    buffer.erase( buffer.begin() );
  }

  //如果是最后一个字符串
  if ( is_last_substring ) {
    cerr << "Is last: " << is_last_substring << endl;
    cerr << "Current available capacity: " << output_.writer().available_capacity() << endl;
    //容量足够则写入
    if ( data.size() <= available_capacity){
      output_.writer().push( data );
      now_index = first_index + data.size();

    }
    //关闭写入流
    output_.writer().close();
  }

  //写入缓冲区：都有哪些情况，记得捋一下（覆盖、重叠、不连续）？还是说一开始没push的都要写到缓冲区？
  if(!pushed){ //这个条件怎么设置？
  //先缓存吧
    //容量足够则写入缓冲区
    if(data.size() <= available_capacity){
      buffer.insert( pair<size_t, string>( first_index, data ) );
      available_capacity -= data.size();
      cerr<<"wrted '%s' to buffer"<<data<<endl;
    }
  }
  
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return buffer.size();
}


