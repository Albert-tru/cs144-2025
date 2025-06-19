#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , datagrams_received_()
  , time_ms_{0}
  , arp_table_()
  , pending_datagrams_()
  , arp_request_timer_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_hop_ip = next_hop.ipv4_numeric();

  // 1. 在 ARP 表中查找下一跳 IP 对应的 MAC 地址
  auto arp_entry = arp_table_.find( next_hop_ip );

  if ( arp_entry != arp_table_.end() ) {
    // --- 情况 A: 找到了 (Cache Hit) ---
    const EthernetAddress& dest_mac = arp_entry->second.first;

    // 封装成以太网帧并发送
    EthernetFrame frame;
    frame.header.src = ethernet_address_;
    frame.header.dst = dest_mac;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize(dgram); ////
    transmit( frame );

  } else {
    // --- 情况 B: 没找到 (Cache Miss) ---

    // 2. 检查是否在冷却时间内发送过 ARP 请求
    if ( arp_request_timer_.find( next_hop_ip ) == arp_request_timer_.end() ) {
      // 如果不在冷却期 (即没找到计时器记录)，则发送一个新的 ARP 请求
      ARPMessage arp_request;
      arp_request.opcode = ARPMessage::OPCODE_REQUEST;
      arp_request.sender_ethernet_address = ethernet_address_;
      arp_request.sender_ip_address = ip_address_.ipv4_numeric();
      arp_request.target_ip_address = next_hop_ip;
      // target_ethernet_address 默认为 00:00:00:00:00:00，不需要设置

      // 封装 ARP 请求到以太网帧中，目的地是广播地址
      EthernetFrame frame;
      frame.header.src = ethernet_address_;
      frame.header.dst = ETHERNET_BROADCAST; // 广播
      frame.header.type = EthernetHeader::TYPE_ARP;
      frame.payload = serialize(arp_request);
      transmit( frame );

      // 记录本次请求时间，启动 5 秒冷却
      arp_request_timer_[next_hop_ip] = time_ms_ + ARP_REQUEST_COOLDOWN_MS;
    }

    // 3. 无论是否发送了新的 ARP 请求，都需要把这个 IP 数据报暂存起来
    pending_datagrams_[next_hop_ip].emplace_back( dgram, time_ms_ );
  }
}

void NetworkInterface::recv_frame( EthernetFrame frame ){
  // 1. 过滤：只处理发往本接口或广播的帧
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  // 2. 根据帧类型分流处理
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    // --- 情况 A: 收到 IPv4 数据报 ---
    InternetDatagram dgram;
    if ( parse(dgram, frame.payload)) {
      // 解析成功，放入接收队列，供上层协议栈处理
      datagrams_received_.push( move( dgram ) );
    }

  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    // --- 情况 B: 收到 ARP 消息 ---
    ARPMessage arp_msg;
    if (parse( arp_msg, frame.payload ) ){
      // a. 学习地址：将发送方的 IP-MAC 映射存入 ARP 表，并设置 30 秒过期时间
      arp_table_[arp_msg.sender_ip_address] = { arp_msg.sender_ethernet_address, time_ms_ + ARP_MAPPING_TTL_MS };

      // b. 发送待处理的数据报：检查是否有数据报正在等待这个刚学到的地址
      if ( pending_datagrams_.count( arp_msg.sender_ip_address ) ) {
        // 遍历所有等待这个 IP 的数据报
        for ( const auto& [dgram, timestamp] : pending_datagrams_.at( arp_msg.sender_ip_address ) ) {
          // 现在我们知道 MAC 地址了，直接发送
          //只发送没过期的
          if(time_ms_ - timestamp <= ARP_MAPPING_TTL_MS){
            EthernetFrame pending_frame;
            pending_frame.header.src = ethernet_address_;
            pending_frame.header.dst = arp_msg.sender_ethernet_address;
            pending_frame.header.type = EthernetHeader::TYPE_IPv4;
            pending_frame.payload = serialize(dgram);
            transmit( pending_frame );
          }

        }
        // 发送完毕，清空等待队列
        pending_datagrams_.erase( arp_msg.sender_ip_address );
      }

      // c. 响应 ARP 请求：如果这是一个对我的 ARP 请求，则回复
      if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage arp_reply;
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
        arp_reply.target_ip_address = arp_msg.sender_ip_address;

        // 封装并发送 ARP 回复
        EthernetFrame reply_frame;
        reply_frame.header.src = ethernet_address_;
        reply_frame.header.dst = arp_msg.sender_ethernet_address; // 单播回复
        reply_frame.header.type = EthernetHeader::TYPE_ARP;
        reply_frame.payload = serialize(arp_reply);
        transmit( reply_frame );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 1. 更新内部时钟
  time_ms_ += ms_since_last_tick;

  // 2. 清理过期的 ARP 映射
  // // C++20 的 erase_if 写法，非常简洁
  // std::erase_if( arp_table_, [this]( const auto& item ) {
  //   // item 是一个 map 里的键值对，item.first 是 IP, item.second 是 {MAC, 过期时间}
  //   auto const& [ip_addr, pair] = item;
  //   return time_ms_ >= pair.second; // 如果当前时间超过了过期时间，就删除
  // } );
  
  
  // (也可以用传统的迭代器方式删除，效果一样)
  for (auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if (time_ms_ >= it->second.second) {
      it = arp_table_.erase(it);
    } else {
      ++it;
    }
  }

  // 2. 新增：清理过期的 ARP 请求计时器
  for (auto it = arp_request_timer_.begin(); it != arp_request_timer_.end(); ) {
    if (time_ms_ >= it->second) {
      pending_datagrams_.erase(it->first);
      it = arp_request_timer_.erase(it);
    } else {
      ++it;
    }
    cerr<<"debug"<<endl;
  }
}
