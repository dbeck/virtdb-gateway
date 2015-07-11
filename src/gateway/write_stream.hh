#pragma once

#include <string>
#include <queue/simple_queue.hh>
#include <gateway/writer_fsm.hh>
#include <gateway/message.hh>

namespace virtdb { namespace gateway {
  
  class write_stream
  {
  public:
    typedef std::shared_ptr<write_stream> sptr;
    
    // TODO:
    // - events
    //   = send one
    //   = start, next, end
    //   = fix
    //   = stop
    //   = error
    // - states
    
    // = comm types
    // - push1, pull1
    // - push1, pull1-n
    // - push1-n, pull1-n
    // - push1-n, pull1
    
  private:
    queue::simple_publisher::sptr   publisher_;
    fsm::state_machine::sptr        fsm_;
    uint8_t                         channel_id_;
    
    // no default construction
    write_stream() = delete;
    
    // disable copying until properly implemented
    write_stream(const write_stream &) = delete;
    write_stream& operator=(const write_stream &) = delete;
    
  public:
    write_stream(queue::simple_publisher::sptr pubptr,
                 fsm::state_machine::sptr smptr,
                 uint8_t channel_id);
    
    virtual ~write_stream();
    
    void send(const single_message & m);
    void start(const start_message & m /* TODO */ );
  };
  
}}
