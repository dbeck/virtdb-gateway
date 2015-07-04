#pragma once

#include <queue/simple_queue.hh>
#include <queue/params.hh>
#include <fsm/state_machine.hh>
#include <cstdint>
#include <set>
#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>

namespace virtdb { namespace gateway {
  
  class simple_server;
  class simple_client;
  
  class simple_gateway
  {
  public:
    struct stream_part
    {
      uint64_t         id_;
      uint64_t         seqno_;
      const uint8_t *  buffer_;
      uint64_t         size_;
      uint64_t         position_;
      uint64_t         total_bytes_;
      uint8_t          event_;
      uint8_t          stream_type_;
      
      stream_part();
    };
    
    struct stream_info
    {
      int64_t    id_;
      int64_t    sent_seqno_;
      int64_t    received_seqno_;
      uint64_t   sent_pos_;
      uint64_t   received_pos_;
      
      stream_info();
      
      typedef std::shared_ptr<stream_info> sptr;
    };
  
    typedef std::function<bool(stream_part & part)>  feeder_fun;
    typedef std::set<uint16_t>                        state_set;
    
    // only 8 bits for internal events
    static const uint8_t EV_START   = 1;
    static const uint8_t EV_ONE     = 2;
    static const uint8_t EV_NEXT    = 3;
    static const uint8_t EV_END     = 4;
    static const uint8_t EV_STOP    = 5;
    static const uint8_t EV_FIX     = 6;
    static const uint8_t EV_ERROR   = 7;
    
  private:
    class make_base_path
    {
    private:
      make_base_path(const make_base_path &) = delete;
      make_base_path& operator=(const make_base_path &) = delete;
      make_base_path() = delete;
    public:
      make_base_path(const std::string & path);
    };
    
    make_base_path            base_path_;
    queue::simple_publisher   sender_;
    queue::simple_subscriber  receiver_;
    
    // disable default construction
    simple_gateway() = delete;
    
    // disable copying until properly implemented
    simple_gateway(const simple_gateway &) = delete;
    simple_gateway & operator=(const simple_gateway &) = delete;
    
  protected:
    simple_gateway(const std::string & base_path,
                   const std::string & sender_path,
                   const std::string & receiver_path,
                   const queue::params & prms);
    
    void send_data(const queue::simple_publisher::buffer_vector & data);
    uint64_t pull_data(uint64_t from,
                       queue::simple_subscriber::pull_fun f,
                       uint64_t timeout_ms);
    static bool get_varint64(const uint8_t * ptr,
                             uint64_t & result,
                             uint64_t & position,
                             uint64_t & remaining);
    
  public:
    virtual ~simple_gateway();
    
    static void set_event_names(fsm::state_machine & fsm);
    
    void seek_to_end();
    uint64_t sender_position() const;
    uint64_t receiver_position() const;
  };

  class simple_client : public simple_gateway
  {
    struct stream
    {
      feeder_fun                 upstream_feed_;
      fsm::state_machine::sptr   fsm_;
      state_set                  terminal_states_;
      stream_info::sptr          info_;
      
      typedef std::shared_ptr<stream> sptr;
    };
    
    typedef std::map<uint64_t, stream::sptr> stream_map;
    
    stream_map streams_;
    
  protected:
    friend class simple_server;
    simple_client(const std::string & path,
                  const queue::params & prms);

  public:
    typedef std::shared_ptr<simple_client> sptr;
    
    virtual ~simple_client();
    static sptr create(const std::string & path,
                       const queue::params & prms=queue::params());
    
    // start a new data stream
    void start(uint8_t stream_type,
               feeder_fun feeder,
               fsm::state_machine::sptr fsm,
               const state_set & terminal_states,
               stream_info::sptr info); // ???
    
    // the data stream's state machine may upcall to these
    void stop(uint64_t id);
    bool wait_data(uint64_t id,
                   uint64_t start_seqno);
    bool get_data(uint64_t id,
                  uint64_t seqno,
                  stream_part & part);
    void request_resend(uint64_t id,
                        uint64_t seqno);
    
    // pre-canned communication patterns:
    // - push1
    // - req1-rep1
    // - push-sub*
  };
  
  class simple_server : public simple_gateway
  {
  public:
    typedef std::shared_ptr<simple_server>                                           sptr;
    typedef std::function<fsm::state_machine::sptr(const stream_part & start,
                                                   fsm::state_machine::trace_fun)>   new_stream_fun;
    typedef std::function<stream_info::sptr(uint64_t id)>                            new_info_fun;

  private:
    // internal FSM states
    static const uint16_t ST_INIT            = 0;
    static const uint16_t ST_READY           = 1;
    static const uint16_t ST_STOPPED         = 2;
    
    // server only events
    static const uint16_t EV_START_SERVER        = 300;
    static const uint16_t EV_STOP_SERVER         = 301;
    static const uint16_t EV_END_STREAM          = 302;
    static const uint16_t EV_STREAM_INIT_FAILED  = 303;
    static const uint16_t EV_BAD_MESSAGE         = 304;
    
    struct handler
    {
      new_stream_fun   fsm_factory_;
      state_set        terminal_states_;
      new_info_fun     info_factory_;
      
      typedef std::shared_ptr<handler> sptr;
    };
    
    struct stream
    {
      fsm::state_machine::sptr   fsm_;
      state_set                  terminal_states_;
      stream_info::sptr          info_;
      uint16_t                   last_state_;
      uint8_t                    type_;
      
      typedef std::shared_ptr<stream> sptr;
    };
    
    typedef std::vector<handler::sptr>         handler_vector;
    typedef std::map<uint64_t, stream::sptr>   stream_map;
    
    handler_vector                   handlers_;
    stream_map                       streams_;
    std::atomic<bool>                stopped_;
    fsm::state_machine::trace_fun    trace_;
    fsm::state_machine               fsm_;
    uint16_t                         last_state_;
    stream_part                      act_message_;
    
  protected:
    friend class simple_client;
    simple_server(const std::string & path,
                  const queue::params & prms,
                  fsm::state_machine::trace_fun trace_cb);
    
  public:
    virtual ~simple_server();
    static sptr create(const std::string & path,
                       const queue::params & prms=queue::params(),
                       fsm::state_machine::trace_fun trace_cb=[](uint16_t seqno,
                                                                 const std::string & desc,
                                                                 const fsm::transition & trans,
                                                                 const fsm::state_machine & sm){});
    
    // start-event / state machine switch
    void add_handler(uint8_t stream_type,
                     new_stream_fun new_handler,
                     const state_set & terminal_states,
                     new_info_fun new_info);
    
    void run(uint64_t from=0);
    void stop();
    bool is_stopped() const;
    
    // interact with the handlers
    void push_event(uint64_t id,
                    uint16_t event,
                    bool if_empty=false);
    
  };
  
}}
