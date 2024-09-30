#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return number_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( sequence_numbers_in_flight() == 0 && !first_receive_ ) {
    data_slice_.emplace( isn_,
                         TCPSenderMessage { isn_, true, "", input_.writer().is_closed(), input_.has_error() } );
    data_key_.push_back( isn_ );
    transmit( data_slice_[isn_] );
    number_in_flight_ += data_slice_[isn_].sequence_length();
    next_seqno_ += data_slice_[isn_].sequence_length();
    return;
  }

  if ( input_.reader().bytes_buffered() == 0 ) {
    // transmit( make_empty_message() );
    if ( !send_fin_ && ( ( window_size_ == 1 && first_receive_ ) || input_.writer().is_closed() ) ) {
      auto mess = make_empty_message();
      mess.FIN = true;
      send_fin_ = true;
      data_slice_.emplace( mess.seqno, mess );
      data_key_.push_back( mess.seqno );
      next_seqno_ += mess.sequence_length();
      number_in_flight_ += mess.sequence_length();
      window_size_--;
      transmit( mess );
    }
    return;
  }

  init_data_slice_and_push( transmit );
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage { isn_ + next_seqno_, false, "", false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.set_error();
  }
  if ( !msg.ackno.has_value() ) {
    return;
  }
  if ( msg.ackno.value() < isn_ + input_.reader().bytes_popped() + 1
       || msg.ackno.value().unwrap( isn_, next_seqno_ ) > next_seqno_ ) {
    return;
  }
  if ( !first_receive_ ) {
    first_receive_ = true;
    number_in_flight_--;
    data_slice_.erase( isn_ );
    data_key_.pop_front();
  }
  window_size_ = *reinterpret_cast<const uint32_t*>( &msg.ackno.value() ) + msg.window_size
                 - ( *reinterpret_cast<uint32_t*>( &isn_ ) + next_seqno_ );

  is_zero_window = window_size_ == 0;

  if ( msg.ackno.value() == isn_ + next_seqno_ && msg.window_size == 0 ) {
    window_size_ = 1;
  }

  if ( data_key_.empty() ) {
    return;
  }
  uint64_t pop_size {};
  while ( !data_key_.empty()
          && data_key_.front() + data_slice_[data_key_.front()].sequence_length() <= msg.ackno.value() ) {
    auto ite = data_key_.front();
    number_in_flight_ -= data_slice_[ite].sequence_length();
    pop_size += data_slice_[ite].payload.size();
    data_slice_.erase( ite );
    data_key_.pop_front();
    ms_since_last_tick_ = 0;
  }
  input_.reader().pop( pop_size );
  RTO_ms_ = initial_RTO_ms_;
  retransmissions_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  ms_since_last_tick_ += ms_since_last_tick;
  if ( ms_since_last_tick_ >= RTO_ms_ && !data_key_.empty() ) {
    transmit( data_slice_[data_key_.front()] );
    retransmissions_++;
    if ( window_size_ != 0 || !first_receive_ || !input_.writer().is_closed() || !is_zero_window ) {
      RTO_ms_ <<= 1;
    }
    ms_since_last_tick_ = 0;
  }
}

void TCPSender::init_data_slice_and_push( const TransmitFunction& transmit )
{
  uint64_t pos { number_in_flight_ };
  if ( !send_fin_ && input_.writer().is_closed()
       && ( window_size_ > input_.reader().bytes_buffered() - number_in_flight_
            || ( window_size_ == 0 && number_in_flight_ == 0 ) ) ) {
    auto mess = make_empty_message();
    mess.FIN = true;
    send_fin_ = true;
    if ( window_size_ > 1 ) {
      auto data_size = std::min( { input_.reader().bytes_buffered() - pos,
                                   TCPConfig::MAX_PAYLOAD_SIZE,
                                   static_cast<uint64_t>( window_size_ ) } );
      if ( data_size == 0 ) {
        goto FLAG;
      }
      if ( pos + data_size < input_.reader().bytes_buffered() ) {
        goto FLAG2;
      }
      mess.payload = std::string( input_.reader().peek().substr( pos, data_size ) );
      if ( data_size >= window_size_ ) {
        mess.FIN = false;
        send_fin_ = false;
      }
    }
  FLAG:
    data_key_.push_back( mess.seqno );
    data_slice_.emplace( mess.seqno, mess );
    number_in_flight_ += mess.sequence_length();
    next_seqno_ += mess.sequence_length();
    if ( window_size_ > 0 )
      window_size_ -= mess.sequence_length();
    transmit( mess );
    return;
  }
FLAG2:
  while ( true ) {
    auto data_size = std::min( { input_.reader().bytes_buffered() - pos,
                                 TCPConfig::MAX_PAYLOAD_SIZE,
                                 static_cast<uint64_t>( window_size_ ) } );
    if ( data_size == 0 ) {
      return;
    }
    if ( pos >= input_.reader().bytes_buffered() ) {
      return;
    }
    auto seqno = isn_ + next_seqno_;
    auto mess = TCPSenderMessage { seqno,
                                   false,
                                   std::string( input_.reader().peek().substr( pos, data_size ) ),
                                   input_.writer().is_closed()
                                     && ( window_size_ > input_.reader().bytes_buffered() - number_in_flight_ )
                                     && !( pos + data_size < input_.reader().bytes_buffered() ),
                                   input_.has_error() };
    pos += mess.sequence_length();
    next_seqno_ += mess.sequence_length();
    number_in_flight_ += mess.sequence_length();
    window_size_ -= mess.sequence_length();

    transmit( mess );
    data_key_.push_back( seqno );
    data_slice_.emplace( std::move( seqno ), std::move( mess ) );
  }
}