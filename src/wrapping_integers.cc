#include "wrapping_integers.hh"
#include<bitset>
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{   //【不用考虑result是否超出2^32-1吗？】
  
    // 计算相对于 zero_point 的偏移量
    //(1ULL << 32): 将无符号长整型常量左移32位，也就是相当于对2^32取模
    uint32_t offset = static_cast<uint32_t>(n % (1ULL << 32));
    // 加上 zero_point 的原始值
    uint32_t result = zero_point.raw_value_ + offset;
    return Wrap32(result);
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
    // 计算当前 Wrap32 对象相对于 zero_point 的 32 位偏移量
    uint32_t offset = raw_value_ - zero_point.raw_value_;

    // 计算 checkpoint 所在的 2^32 区间
    uint64_t base = checkpoint & ~((1ULL << 32) - 1);

    // 考虑三种可能的绝对序列号：当前区间、前一个区间、后一个区间
    uint64_t candidates[3] = {
        base + offset,
        base - (1ULL << 32) + offset,
        base + (1ULL << 32) + offset
    };

    // 选择最接近 checkpoint 的序列号
    uint64_t closest = candidates[0];
    uint64_t min_diff = (candidates[0] > checkpoint) ? (candidates[0] - checkpoint) : (checkpoint - candidates[0]);
    for (int i = 1; i < 3; ++i) {
        uint64_t diff = (candidates[i] > checkpoint) ? (candidates[i] - checkpoint) : (checkpoint - candidates[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest = candidates[i];
        }
    }

    return closest;
}
