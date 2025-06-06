#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <iostream>
using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstanding_bytes;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_nums;
}


void TCPSender::push(const TransmitFunction& transmit)
{
  // 首先检查Writer是否存在错误并设置错误状态，有错误的话停止push，并返回空的message
  if (writer().has_error()) {
    _has_error = true;
    cerr << "DEBUG: writer has error, setting _has_error = true" << endl;
  }

  if (_has_error) {
    cerr << "DEBUG: _has_error is true in push(), sending RST message" << endl;
    TCPSenderMessage rst_msg = make_empty_message();
    transmit(rst_msg);
    return;
  }
  
  // 如果没有错误，正常处理...
  // 如果可接收的窗口大小为0且没有要重传的消息，则设置窗口大小为1
  uint64_t effective_window = (received_msg.window_size == 0 && outstanding_bytes == 0) ? 1 : received_msg.window_size;
  
  //如果当前的窗口大小可以容纳待重传的消息，则处理数据
  while (outstanding_bytes < effective_window) {
    TCPSenderMessage msg;
    //发送SYN消息
    if (isSent_ISN == false) {
      msg.SYN = true;
      msg.seqno = isn_;
      isSent_ISN = true;  // 立即设置标志
    } else {
      msg.seqno = Wrap32::wrap(abs_seqno, isn_);
    }
    
    // 计算可用窗口大小（考虑已发送但未确认的字节）
    size_t remaining_window = effective_window - outstanding_bytes;
    // 如果是SYN消息，需要减去一个字节，因为SYN占用一个序列号
    if (msg.SYN) {
      remaining_window = remaining_window > 0 ? remaining_window - 1 : 0;
    }
    
    // 计算可以发送的数据大小
    size_t payload_size = min(remaining_window, TCPConfig::MAX_PAYLOAD_SIZE);
    payload_size = min(payload_size, writer().reader().bytes_buffered());
    
    // 读取数据
    read(writer().reader(), payload_size, msg.payload);
    
    // 修改FIN逻辑：只有当发送完所有数据后，且确保FIN的一个字节也能放入窗口时才添加FIN
    if (writer().is_closed() && !isSent_FIN && 
        writer().reader().bytes_buffered() == 0 && 
        outstanding_bytes + msg.sequence_length()  < effective_window) {
      isSent_FIN = true;
      msg.FIN = true;
    }
    
    if (!msg.sequence_length()) break;

    outstanding_collections.push_back(msg);
    outstanding_bytes += msg.sequence_length();  // 确保正确计算序列号占用
    abs_seqno += msg.sequence_length();
    
    // 立即发送创建的消息
    transmit(msg);
    
    // 如果有未确认的数据，启动计时器
    if (outstanding_bytes > 0 && !is_start_timer) {
      is_start_timer = true;
      cur_RTO_ms = initial_RTO_ms_;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(abs_seqno, isn_);
  
  // 检查是否有错误，无论是来自内部标志还是Writer
  bool has_error = _has_error || writer().has_error();
  cerr << "DEBUG: make_empty_message called, _has_error = " << (_has_error ? "true" : "false") 
       << ", writer().has_error() = " << (writer().has_error() ? "true" : "false") << endl;
  
  if (has_error) {
    msg.RST = true;
  }
  
  return msg;
}

void TCPSender::receive(const TCPReceiverMessage& msg)
{
  // 检查收到的RST标志
  if (msg.RST) {
    _has_error = true;
    // 还需要设置底层writer的错误状态
    const_cast<Writer&>(writer()).set_error();
    return;
  }

  if (_has_error) {
    return;  // 如果有错误，不执行任何操作
  }
  
  received_msg = msg;
  primitive_window_size = msg.window_size;
  if (msg.ackno.has_value() == true) {
    uint64_t ackno_unwrapped = static_cast<uint64_t>(msg.ackno.value().unwrap(isn_, abs_seqno));
    if (ackno_unwrapped > abs_seqno) return;
    while (outstanding_bytes != 0 && 
           static_cast<uint64_t>(outstanding_collections.front().seqno.unwrap(isn_, abs_seqno)) + 
           outstanding_collections.front().sequence_length() <= ackno_unwrapped) {
      outstanding_bytes -= outstanding_collections.front().sequence_length();
      outstanding_collections.pop_front();
      consecutive_retransmissions_nums = 0;
      cur_RTO_ms = initial_RTO_ms_;
      if (outstanding_bytes == 0) is_start_timer = false;
      else is_start_timer = true;
    }
  }
}

void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction& transmit)
{
  if (_has_error) {
    return;  // 如果有错误，不执行任何操作
  }
  
  // 只有当有未确认的数据且计时器启动时才减少时间
  if (is_start_timer) {
    if (cur_RTO_ms <= ms_since_last_tick) {
      // 超时，重传第一个未确认的段
      transmit(outstanding_collections.front());
      consecutive_retransmissions_nums++;
      // 有空间的话指数退避
      if (primitive_window_size) 
        cur_RTO_ms = (1UL << consecutive_retransmissions_nums) * initial_RTO_ms_;
      // 否则需要重置定时器
      else 
        cur_RTO_ms = initial_RTO_ms_;
      
    } else {
      //减掉一个tick经过的时间
      cur_RTO_ms -= ms_since_last_tick;
    }
  }
}