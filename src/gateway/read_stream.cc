#include <gateway/read_stream.hh>

namespace virtdb { namespace gateway {
    
  read_stream::read_stream(queue::simple_subscriber::sptr subptr,
                           fsm::state_machine::sptr smptr)
  : subscriber_{subptr},
    fsm_{smptr}
  {
  }
  
  read_stream::~read_stream()
  {
  }
  
}}
