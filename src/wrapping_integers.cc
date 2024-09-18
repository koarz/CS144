#include "wrapping_integers.hh"

using namespace std;

constexpr uint64_t TWO31 = 1UL << 31;
constexpr uint64_t TWO32 = 1UL << 32;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( n + zero_point.raw_value_ ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t dis = raw_value_ - static_cast<uint32_t>( ( checkpoint + zero_point.raw_value_ ) );
  return dis <= TWO31 || checkpoint + dis < TWO32 ? checkpoint + dis : checkpoint + dis - TWO32;
}
