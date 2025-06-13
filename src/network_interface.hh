#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

#include <memory>
#include <queue>
#include <map>
#include <iostream>

using namespace std;

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  //hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( EthernetFrame frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};

  // 内部时钟，记录从开始到现在的总毫秒数
  size_t time_ms_ {0};

  // ARP 缓存表：存储 IP 地址 -> {MAC 地址, 过期时间点} 的映射
  // key: 32位的IP地址 (uint32_t)
  // value: 一个包含 MAC 地址和此条目过期绝对时间的 pair
  std::map<uint32_t, std::pair<EthernetAddress, size_t>> arp_table_;

  // 等待 ARP 回复的 IP 数据报队列
  // 当我们不知道下一跳的 MAC 地址时，把目标是这个 IP 的数据报暂存在这里
  // key: 32位的下一跳IP地址 (uint32_t)
  // value: 等待发送到该 IP 的数据报列表和时间戳
  std::map<uint32_t, std::vector<std::pair<InternetDatagram, size_t>>> pending_datagrams_;

  // ARP 请求节流阀：防止对同一个 IP 地址频繁发送 ARP 请求
  // key: 32位的目标IP地址 (uint32_t)
  // value: 上次为这个 IP 发送 ARP 请求的时间点
  std::map<uint32_t, size_t> arp_request_timer_;

  // --- 定义一些常量，方便代码编写和阅读 ---
  static constexpr size_t ARP_MAPPING_TTL_MS = 30000;      // ARP 映射的存活时间：30秒
  static constexpr size_t ARP_REQUEST_COOLDOWN_MS = 5000; // ARP 请求的冷却时间：5秒
};
