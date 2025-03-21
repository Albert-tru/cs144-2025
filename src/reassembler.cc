#include "reassembler.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring) {
  
  cerr<<"first index: "<<first_index<<"  string: "<<data<<endl;

  size_t data_size = data.size();
  size_t _first_unassembled = output_.writer().bytes_pushed();
  size_t _first_unaccept = _first_unassembled + _capacity;

  cerr<<"first_unaccembled : "<<_first_unassembled<<endl;

  //【这里注释掉也没问题】
  // // 只处理落在感兴趣区间内的数据
  // if (first_index >= _first_unaccept || first_index + data_size <= _first_unassembled) {
  //     if (is_last_substring) {
  //         _is_eof = true;
  //         _eof_index = first_index + data_size;
  //     }
  //     return;
  // }

  // 计算实际需要处理的数据区间
  size_t start = std::max(first_index, _first_unassembled);
  size_t end = std::min(first_index + data_size, _first_unaccept);

  cerr<<" "<<start<<" : "<<end<<endl;

  // 插入或更新数据
  //【值得注意的是，这里的插入方式是以单个字节来插入的！】
  for (size_t i = start; i < end; ++i) {
      size_t index = i - _first_unassembled;
      buffer[index] = data[i - first_index];
      cerr<<"buffered "<<buffer[index]<<endl;
  }

  // 检查并输出完整数据
  //【buffer.begin()->first == 0保证只取出紧挨着的字节】
  while (!buffer.empty() && buffer.begin()->first == 0) {
      std::string output;
      size_t start_index = 0;
      while (!buffer.empty() && buffer.begin()->first == start_index) {
          output += buffer.begin()->second;
          buffer.erase(buffer.begin());
          start_index++;
      }
      output_.writer().push(output);
      cerr<<"writed "<<output<<endl;
      _unassembled_bytes -= output.size();
      _first_unassembled += output.size();
  }

  // 更新未组装字节计数
  _unassembled_bytes += (end - start);

  if(is_last_substring){
    _is_eof = true;
    _eof_index = first_index + data_size;
  }
  // 检查EOF并关闭写入
  //【为啥结束条件是_first_unassembled == _eof_index？】
  if (_is_eof && _first_unassembled == _eof_index) {
      output_.writer().close();
  }

  cerr<<"eof_index: "<<_eof_index<<"  first_unassembled: "<<_first_unassembled<<endl;
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return _unassembled_bytes;
}


