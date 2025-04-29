#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "wrapping_integers.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
    // 将 next_seqno_ 转换为绝对序列号
    uint64_t abs_next_seqno = next_seqno_.unwrap(isn_, 0);
    // 计算已发送但未确认的序列号数量
    return abs_next_seqno - acked_seqno_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  
  return consecutive_retransmissions_count_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
    // 发送 SYN 报文
    if (!syn_sent_) {
        TCPSenderMessage syn_msg;
        syn_msg.seqno = next_seqno_;
        syn_msg.SYN = true;
        transmit(syn_msg);
        // 将 SYN 报文添加到 outstanding_segments 中
        outstanding_segments_.push(syn_msg); 
        next_seqno_ += uint32_t(1);
        syn_sent_ = true;
    }

    // 计算可用窗口大小
    //如果类里面存的窗口大小为0，也就是说receiver可以接收的字节为0,发送方需要停止发送，
    //将新的窗口设为1，用来发送终止报文，来避免接收方死锁
    uint64_t window_size = window_size_ == 0 ? 1 : window_size_;
    uint64_t available_space = window_size - (next_seqno_.unwrap(isn_,0) - acked_seqno_);

    // 从字节流读取数据并发送
    while (available_space > 0 && !input_.is_closed()) {
        size_t read_size = std::min(available_space, TCPConfig::MAX_PAYLOAD_SIZE);
        string_view data = input_.reader().peek();

        TCPSenderMessage data_msg;
        data_msg.seqno = next_seqno_;
        data_msg.payload = data.substr(0, read_size);
        transmit(data_msg);
        // 将数据报文添加到 outstanding_segments 中
        outstanding_segments_.push(data_msg); 
        next_seqno_ += data_msg.sequence_length();
        available_space -= data_msg.sequence_length();
    }

    // 发送 FIN 报文
    if (input_.is_closed () && !fin_sent_ && available_space > 0) {
        TCPSenderMessage fin_msg;
        fin_msg.seqno = next_seqno_;
        fin_msg.FIN = true;
        transmit(fin_msg);
        // 将 FIN 报文添加到 outstanding_segments 中
        outstanding_segments_.push(fin_msg); 
        next_seqno_ += 1;
        fin_sent_ = true;
    }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
    // 创建一个空的消息，序列号为下一个要发送的序列号
    return {.seqno = next_seqno_, .SYN = false, .payload = {}, .FIN = false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
    // 更新窗口大小
    window_size_ = msg.window_size;

    // 检查是否有确认号
    if (!msg.ackno.has_value()) {
        return;
    }

    // 获取确认号并转换为绝对序列号
    uint64_t checkpoint = next_seqno_.unwrap(isn_, 0);
    uint64_t abs_ackno = msg.ackno.value().unwrap(isn_, checkpoint);

    // 只有当新的确认号大于当前已确认的序列号时才更新   
    if (abs_ackno > acked_seqno_) {
      acked_seqno_ = abs_ackno;
    }

    // 移除已确认的段
    while (!outstanding_segments_.empty()) 
    {
        const auto& seg = outstanding_segments_.front();
        uint64_t seg_start = seg.seqno.unwrap(isn_, checkpoint);
        uint64_t seg_end = seg_start + seg.sequence_length();

        // 如果段的结束序列号小于等于确认号，说明该段已确认
        if (seg_end <= abs_ackno) {
            outstanding_segments_.pop();
            // 重置重传超时时间
            rto_ = initial_RTO_ms_;
            // 重置连续重传次数
            consecutive_retransmissions_count_ = 0;
        } else {
            break;
        }
    }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
    // 更新自上次调用 tick 函数以来经过的时间
    time_since_last_tick_ += ms_since_last_tick;

    // 检查是否有未确认的报文段且已经超时
    if (!outstanding_segments_.empty() && time_since_last_tick_ >= rto_) {
        // 重传最早未确认的报文段
        TCPSenderMessage oldest_msg = outstanding_segments_.front();
        transmit(oldest_msg);

        // 如果接收方窗口大小大于 0，增加连续重传次数并加倍 RTO
        if (window_size_ > 0) {
            consecutive_retransmissions_count_++;
            rto_ *= 2;
        }

        // 重置时间计数器
        time_since_last_tick_ = 0;
    }
}
