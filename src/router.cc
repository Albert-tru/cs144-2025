#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  Route entry{
    .prefix = route_prefix,
    .prefix_length = prefix_length,
    .next_hop = next_hop,
    .interface_num = interface_num
  };
  routing_table_.push_back(entry);
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // 遍历每个网络接口
  for (auto& iface : interfaces_) {
    auto& datagrams = iface->datagrams_received();

    // 逐个处理收到的数据报
    while (!datagrams.empty()) {
      InternetDatagram dgram = datagrams.front();
      datagrams.pop();

      const uint32_t dst_ip = dgram.header.dst;

      // 最长前缀匹配查找最佳路由项
      Route* best_match = nullptr;
      uint8_t longest_match = 0;

      for (auto& entry : routing_table_) {
        uint32_t mask = (entry.prefix_length == 0) ? 0 : (~0U << (32 - entry.prefix_length));

        if ((dst_ip & mask) == (entry.prefix & mask)) {
          if (entry.prefix_length >= longest_match) {
            longest_match = entry.prefix_length;
            best_match = &entry;
          }
        }
      }

      // 没有匹配项：丢包
      if (best_match == nullptr) {
        continue;
      }

      // TTL 检查
      if (dgram.header.ttl <= 1) {
        continue;
      }

      // TTL 减一
      dgram.header.ttl--;

      //重新计算校验和
      dgram.header.compute_checksum();

      // 计算下一跳地址：若是直连路由，用目标 IP；否则用指定的 next_hop
      Address next_hop_ip = best_match->next_hop.value_or(Address::from_ipv4_numeric(dst_ip));

      // 通过接口转发数据报 —— NetworkInterface 会自动处理 ARP 等细节
      interface(best_match->interface_num)->send_datagram(dgram, next_hop_ip);

      
    }
  }
}




