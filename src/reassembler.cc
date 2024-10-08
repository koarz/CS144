#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
// #include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // when the slice first_index overflow output's capacity
  if ( first_index >= current_idx_ + output_.writer().available_capacity()
       || first_index + data.size() < current_idx_ ) {
    // cerr << "overflow the bytestream or slice repeat\n";
    return;
  }
  if ( is_last_substring ) {
    last_idx_ = first_index + data.size();
  }
  if ( first_index <= current_idx_ ) {
    // cerr << "first idx <= current idx\n";
    auto origin_size = output_.reader().bytes_buffered();
    output_.writer().push( data.substr( current_idx_ - first_index ) );
    // cerr << data.substr( current_idx_ - first_index ) << '\n';
    current_idx_ += output_.reader().bytes_buffered() - origin_size;
  } else {
    // cerr << "first idx > current idx\n";
    buffer_.emplace( first_index, std::move( data ) );
  }

  if ( output_.writer().available_capacity() == 0 ) {
    buffer_.clear();
  }

  MoveBufferToOuput();

  if ( current_idx_ >= last_idx_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  if ( buffer_.empty() ) {
    return 0;
  }
  uint64_t begin_idx { buffer_.begin()->first }, idx { begin_idx },
    curidx_add_avacapacity { output_.writer().available_capacity() + current_idx_ },
    length { ( idx + buffer_.begin()->second.size() ) > ( curidx_add_avacapacity )
               ? ( curidx_add_avacapacity - idx )
               : buffer_.begin()->second.size() },
    res {};
  std::for_each( std::next( buffer_.begin() ), buffer_.end(), [&]( auto& it ) {
    if ( it.first > idx + length ) {
      res += length;
      idx = it.first;
      length = ( res + length ) > ( curidx_add_avacapacity ) ? ( curidx_add_avacapacity - idx ) : it.second.size();
    } else {
      if ( it.first + it.second.size() < idx + length ) {
        return;
      }
      length = it.first + it.second.size() - idx;
    }
  } );
  return res + length;
}

void Reassembler::MoveBufferToOuput()
{
  if ( buffer_.empty() )
    return;
  uint64_t available_capacity { output_.writer().available_capacity() }, buf_add_curidx { current_idx_ };
  // cerr << "Avaliable capacity: " << available_capacity << '\n';
  std::string buffer;
  buffer.reserve( available_capacity );
  auto it = buffer_.begin();
  while ( it != buffer_.end() && it->first <= buf_add_curidx ) {
    if ( it->first + it->second.size() < buf_add_curidx ) {
      buffer_.erase( it++ );
      continue;
    }
    // cerr << it->second.size() << "      " << available_capacity - buffer.size() << '\n';
    uint64_t substr_start_idx { it->first < buf_add_curidx ? buf_add_curidx - it->first : 0 };
    buffer.replace( buffer.size(),
                    std::min( it->second.size(), available_capacity - buffer.size() ),
                    it->second,
                    substr_start_idx,
                    std::min( it->second.size() - current_idx_ + it->first, available_capacity - buffer.size() ) );
    buffer_.erase( it++ );
    buf_add_curidx = current_idx_ + buffer.size();
  }
  // cerr << "Move to output: " << buffer << '\n';
  current_idx_ += buffer.size();
  output_.writer().push( buffer );
  if ( output_.writer().available_capacity() == 0 ) {
    buffer_.clear();
  }
}