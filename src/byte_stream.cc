#include "byte_stream.hh"
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( data_.size() == capacity_ ) {
    return;
  } else if ( data.size() + data_.size() <= capacity_ ) {
    data_ += data;
    pushed_ += data.size();
  } else {
    pushed_ += capacity_ - data_.size();
    data_ += data.substr( 0, available_capacity() );
  }
  return;
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - data_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_;
}

bool Reader::is_finished() const
{
  return closed_ && data_.size() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return poped_;
}

string_view Reader::peek() const
{
  return data_;
}

void Reader::pop( uint64_t len )
{
  if ( len >= data_.size() ) {
    poped_ += data_.size();
    data_.clear();
  } else {
    poped_ += len;
    data_ = data_.substr( len, data_.size() - len );
  }
}

uint64_t Reader::bytes_buffered() const
{
  return data_.size();
}
