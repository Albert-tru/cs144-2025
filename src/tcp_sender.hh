#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "queue"

#include <functional>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
  : syn_sent_(false), fin_sent_(false)
  ,input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  , next_seqno_( isn_ ), rto_( initial_RTO_ms_ ), time_since_last_tick_( 0 )
  , outstanding_segments_(), consecutive_retransmissions_count_( 0 )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  bool syn_sent_;
  bool fin_sent_;
  ByteStream input_; // 输出流，用于存储要发送的数据
  Wrap32 isn_; // 初始序列号
  uint64_t acked_seqno_ = 0;//接收方已接收的最大序列号
  uint64_t initial_RTO_ms_; // 初始重传超时时间（毫秒）
  Wrap32 next_seqno_; // 下一个要发送的序列号
  uint16_t window_size_ = 0; // 窗口大小（接收来的）
  uint64_t rto_; // 当前重传超时时间（毫秒）
  uint64_t time_since_last_tick_; // 自上次调用 tick 函数以来经过的时间（毫秒）
  std::queue<TCPSenderMessage> outstanding_segments_; // 未确认的段队列
  uint64_t consecutive_retransmissions_count_; // 连续重传次数
  bool timer_running_ = false; // 计时器是否正在运行的标志
};
