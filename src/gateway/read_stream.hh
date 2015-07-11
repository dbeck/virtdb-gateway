#pragma once

#include <string>
#include <queue/simple_queue.hh>
#include <fsm/state_machine.hh>

namespace virtdb { namespace gateway {
  
  class read_stream
  {
  public:
    typedef std::shared_ptr<read_stream> sptr;
    
    // TODO:
    // - events
    // - states
    
  private:
    queue::simple_subscriber::sptr  subscriber_;
    fsm::state_machine::sptr        fsm_;
    
    // no default construction
    read_stream() = delete;
    
    // disable copying until properly implemented
    read_stream(const read_stream &) = delete;
    read_stream& operator=(const read_stream &) = delete;
    
  public:
    read_stream(queue::simple_subscriber::sptr subptr,
                fsm::state_machine::sptr smptr);
    
    virtual ~read_stream();
  };
  
}}

