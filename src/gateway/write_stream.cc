#include <gateway/write_stream.hh>

namespace virtdb { namespace gateway {
  
  write_stream::write_stream(queue::simple_publisher::sptr pubptr,
                             fsm::state_machine::sptr smptr,
                             uint8_t channel_id)
  : publisher_{pubptr},
    fsm_{smptr},
    channel_id_{channel_id}
  {
  }
  
  write_stream::~write_stream()
  {
  }
  
}}
