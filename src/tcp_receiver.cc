#include "tcp_receiver.hh"
#include "wrapping_integers.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( message.SYN ) {
    reassembler_.insert( 0, message.payload, message.FIN );
    zero_point_ = message.seqno;
    have_zero_point_ = true;
  } else {
    reassembler_.insert( message.seqno.unwrap( zero_point_, reassembler_.reader().bytes_buffered() ) - 1,
                         message.payload,
                         message.FIN );
  }
  if ( message.FIN ) {
    have_fin_ = true;
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  /*
   * std::optional<Wrap32> ackno {};
   * uint16_t window_size {};
   * bool RST {};
   */
  return TCPReceiverMessage {
    have_zero_point_ ? zero_point_.wrap( 1 + ( have_fin_ && reassembler_.writer().is_closed() )
                                           + static_cast<uint32_t>( reassembler_.reader().bytes_buffered()
                                                                    + reassembler_.reader().bytes_popped() ),
                                         zero_point_ )
                     : std::optional<Wrap32> {},
    static_cast<uint16_t>( std::min( 0xffffUL, reassembler_.writer().available_capacity() ) ),
    reassembler_.reader().has_error() };
}
