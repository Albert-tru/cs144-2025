#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "deque"

#include <functional>

class TCPSender
{
  public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , isSent_ISN( 0 )
    , isSent_FIN( 0 )
    , cur_RTO_ms( initial_RTO_ms )
    , is_start_timer( 0 )
    , received_msg()
    , abs_seqno( 0 )
    , primitive_window_size( 1 )
    , outstanding_collections()
    , outstanding_bytes( 0 )
    , consecutive_retransmissions_nums( 0 )
  {
    received_msg.ackno = isn_;
    received_msg.window_size = 1;
  }
  
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
  Reader& reader() { return input_.reader(); }
  
  void set_error() { _has_error = true; }
  bool has_error() const { return _has_error; }
  
  private:
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  bool isSent_ISN;
  bool isSent_FIN;
  uint64_t cur_RTO_ms;
  bool is_start_timer;
  TCPReceiverMessage received_msg;  //接收来的TCPReceierMessage，用于记录可接收的窗口大小、ACK、
  uint64_t abs_seqno;   //当前待发送的字节的绝对序列号
  uint16_t primitive_window_size;
  std::deque<TCPSenderMessage> outstanding_collections; //已发送但未确认的消息
  uint64_t outstanding_bytes;  //需要重传的消息所占的字节
  uint64_t consecutive_retransmissions_nums;  //连续重传次数
  bool _has_error = false;   //错误判别
};