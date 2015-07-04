#include <gateway/simple_gateway.hh>
#include <gateway/exception.hh>
#include <queue/varint.hh>
#include <iostream>

// C libs
#include <sys/stat.h>

namespace virtdb { namespace gateway {
  
  simple_gateway::make_base_path::make_base_path(const std::string & path)
  {
    struct stat dir_stat;
    if( ::lstat(path.c_str(), &dir_stat) == 0 )
    {
      // is a directory
      if( !(dir_stat.st_mode & S_IFDIR) )
      {
        THROW_(std::string{"another non-folder object exists at the path given: "}+path);
      }
      
      // neither group or others can access
      if( ((dir_stat.st_mode & S_IRWXG) | (dir_stat.st_mode & S_IRWXO)) != 0 )
      {
        THROW_(std::string{"permissions allow group or others to access: "}+path);
      }
    }
    else
    {
      if( ::mkdir(path.c_str(), 0700) )
      {
        THROW_(std::string{"failed to create folder at: "}+path);
      }
    }
  }
  
  // start a new data stream
  void
  simple_client::start(uint8_t stream_type,
                       feeder_fun feeder,
                       fsm::state_machine::sptr fsm,
                       const state_set & terminal_states,
                       stream_info::sptr info)
  {
    using namespace virtdb::queue;
    
    uint16_t fsm_state = 0;
    bool send_more = true;
    
    while( true )
    {
      if( send_more )
      {
        ++(info->sent_seqno_);

        stream_part new_stream_part;
        new_stream_part.seqno_ = info->sent_seqno_;
        
        // set id in the message part
        if( info->id_ == -1 ) { new_stream_part.id_  = sender_position(); }
        else                  { new_stream_part.id_  = info->id_; }
        
        send_more = feeder(new_stream_part);
        
        // default is to send a single message
        uint8_t type_start[2] = { EV_ONE, stream_type };
        uint8_t ts_len = 2;
        
        // collect message parts
        simple_publisher::buffer_vector data_vec;
        
        if( info->id_ == -1 )
        {
          // first packet, but more to come
          if( send_more )
            type_start[0] = EV_START;
          
          // administer ourselves
          info->id_ = sender_position();

          // always add the header part
          data_vec.push_back(simple_publisher::buffer{type_start, ts_len});
          
          // add a client message id too
          varint v_id{(uint64_t)info->id_};
          data_vec.push_back(simple_publisher::buffer{v_id.buf(), v_id.len()});
          
          // may add data if available
          if( new_stream_part.size_ > 0 && new_stream_part.buffer_ != nullptr)
            data_vec.push_back(simple_publisher::buffer{new_stream_part.buffer_, new_stream_part.size_});
          
          // shoot the data
          send_data(data_vec);
        }
        else
        {
          // we don't need to send the stream_type
          ts_len = 1;
          
          // tell the receiver if we have more to be sent
          if( send_more )
            type_start[0] = EV_NEXT;
          else
            type_start[0] = EV_END;
          
          // always add the header part
          data_vec.push_back(simple_publisher::buffer{type_start, ts_len});
          
          // add identifiers too
          varint v_id{(uint64_t)info->id_};
          varint v_seqno{(uint64_t)info->sent_seqno_};
          
          data_vec.push_back(simple_publisher::buffer{v_id.buf(), v_id.len()});
          data_vec.push_back(simple_publisher::buffer{v_seqno.buf(), v_seqno.len()});

          // may add data if available
          if( new_stream_part.size_ > 0 && new_stream_part.buffer_ != nullptr)
            data_vec.push_back(simple_publisher::buffer{new_stream_part.buffer_, new_stream_part.size_});

          // shoot the data
          send_data(data_vec);
          info->sent_pos_ = sender_position();
        }
      }
      
      fsm_state = fsm->run(fsm_state);
      if( terminal_states.count(fsm_state) )
      {
        break;
      }
    };
  }
  
  simple_server::simple_server(const std::string & path,
                               const queue::params & prms,
                               fsm::state_machine::trace_fun trace_cb)
  : simple_gateway{path, path+"/1", path+"/0", prms},
    handlers_{256, handler::sptr()},
    stopped_{false},
    trace_{trace_cb},
    fsm_{std::string("SERVER:")+path, trace_cb},
    last_state_{ST_INIT}
  {
    using namespace virtdb::fsm;
    {
      // server states
      fsm_.state_name(ST_INIT,     "INIT");
      fsm_.state_name(ST_READY,    "READY");
      fsm_.state_name(ST_STOPPED,  "STOPPED");

      // client events
      set_event_names(fsm_);
      
      // server events
      fsm_.event_name(EV_START_SERVER,        "START SERVER");
      fsm_.event_name(EV_STOP_SERVER,         "STOP SERVER");
      fsm_.event_name(EV_END_STREAM,          "END STREAM");
      fsm_.event_name(EV_STREAM_INIT_FAILED,  "STREAM INIT ERROR");
      fsm_.event_name(EV_BAD_MESSAGE,         "BAD MESSAGE");
      
      // trivial transitions
      {
        transition::sptr server_start {new transition{ST_INIT,  EV_START_SERVER,       ST_READY,    "Start server"}};
        transition::sptr server_stop  {new transition{ST_READY, EV_STOP_SERVER,        ST_STOPPED,  "Stop server"}};
        transition::sptr init_error   {new transition{ST_READY, EV_STREAM_INIT_FAILED, ST_READY,    "Cannot initialize stream"}};
        transition::sptr bad_message  {new transition{ST_READY, EV_BAD_MESSAGE,        ST_READY,    "Bad stream part arrived"}};
        
        fsm_.add_transition(server_start);
        fsm_.add_transition(server_stop);
        fsm_.add_transition(init_error);
        fsm_.add_transition(bad_message);
      }
      
      // client stream transitions, all go from READY -> READY
      // these don't impact the server state machine, except for the few actions we actually do
      {
        transition::sptr client_start {new transition  {ST_READY, EV_START,  ST_READY,  "Start client stream"}};
        transition::sptr client_one   {new transition  {ST_READY, EV_ONE,    ST_READY,  "Single client message"}};
        transition::sptr client_next  {new transition  {ST_READY, EV_NEXT,   ST_READY,  "Next in client stream"}};
        transition::sptr client_end   {new transition  {ST_READY, EV_END,    ST_READY,  "End client stream"}};
        transition::sptr client_stop  {new transition  {ST_READY, EV_STOP,   ST_READY,  "Client requests stop server stream"}};
        transition::sptr client_fix   {new transition  {ST_READY, EV_FIX,    ST_READY,  "Client requests missing piece of server stream"}};
        transition::sptr client_error {new transition  {ST_READY, EV_ERROR,  ST_READY,  "Client says ERROR"}};
        
        action::sptr init_one{new action{[this](uint16_t seqno,
                                                transition & tran,
                                                state_machine & sm)
          {
            auto handler = handlers_[act_message_.stream_type_];
            if( handler )
            {
              stream::sptr stream_data{new stream};
              stream_data->fsm_              = (handler->fsm_factory_)(act_message_, trace_);
              stream_data->terminal_states_  = handler->terminal_states_;
              stream_data->info_             = (handler->info_factory_)(act_message_.id_);
              stream_data->type_             = act_message_.stream_type_;
              
              stream_data->fsm_->enqueue(act_message_.event_);
              
              stream_data->last_state_       = stream_data->fsm_->run(ST_INIT);
              
              // check if we are done here
              if( stream_data->terminal_states_.count(stream_data->last_state_) == 0 )
              {
                // if the last state is non terminal state then we need to store the stream data
                // because the server may want to send additional messages in response to this single
                // request
                streams_[act_message_.id_] = stream_data;
              }
            }
            else
            {
              fsm_.enqueue(EV_STREAM_INIT_FAILED);
              THROW_(std::string{"No handler for stream type:"}+std::to_string((int)act_message_.stream_type_));
            }
          }, "INIT ONE"
        }};
        
        client_one->set_action(1, init_one);

        /*
        loop::sptr run_stream{new loop{[this](uint16_t seqno,
                                              transition & trans,
                                              state_machine & sm,
                                              uint64_t iteration)
          {
            return false;
          }, "RUN STREAM"
        }};

        action::sptr check_stream{new action{[this](uint16_t seqno,
                                                   transition & tran,
                                                   state_machine & sm)
          {
          }, "CHECK STREAM STATE"
        }};
         */
        
        fsm_.add_transition(client_start);
        fsm_.add_transition(client_one);
        fsm_.add_transition(client_next);
        fsm_.add_transition(client_end);
        fsm_.add_transition(client_stop);
        fsm_.add_transition(client_fix);
        fsm_.add_transition(client_error);
      }
    }
  }
  
  /* Packet descriptions:
   
   * EV_START / EV_ONE
   *  - 1B: msg. type    / = EV_START / EV_ONE
   *  - 1B: stream type  / to match stream handler at the server side
   *  - 1-10B: ID        / VarInt64, for client messages it is redundant
   *  - [data]           /
   
   * EV_NEXT:
   *  - 1B:    type      / = EV_NEXT
   *  - 1-10B: ID        / VarInt64, position of EV_START/EV_ONE message
   *  - 1-10B: Seq.No    / VarInt64, identifies the message number of the stream
   *  - [data]           /
   
   * EV_END:             / This message flags end of stream
   *  - 1B:    msg.type  / = EV_END
   *  - 1-10B: ID        / VarInt64, position of EV_START/EV_ONE message
   *  - 1-10B: Seq.No    / VarInt64, identifies the message number of the stream
   *  - [data]           /
   
   * EV_STOP:            / Either client or server may tell the other party to stop sending new parts
   *  - 1B:    msg.type  / = EV_STOP
   *  - 1-10B: ID        / VarInt64, position of EV_START/EV_ONE message
   *  - [reason]         /
   
   * EV_FIX:             / Tell the other party that we badly need a missing stream part
   *  - 1B:    msg.type  / = EV_FIX
   *  - 1-10B: ID        / VarInt64, position of EV_START/EV_ONE message
   *  - 1-10B: Seq.No    / VarInt64, identifies the message number of the stream
   
   * EV_ERROR:           / Tell the other party that we are unhappy with the stream or the process
   *  - 1B:    msg.type  / = EV_ERROR
   *  - 1-10B: ID        / VarInt64, position of EV_START/EV_ONE message
   *  - 1-10B: Seq.No    / VarInt64, identifies the message number of the stream
   *  - [reason]         /
   
   * The gateway lib builds on the assumption that both client and server can send a stream.
   * Client has the privilege to start a new stream and all message parts both client and server
   * are identified by the client stream position.
   
   * ID:      Varint64 encoded position of the client stream start position
   * Seq.No.: Sequence numbers identify the order of the stream part to make a continous stream of data.
   *          They may come out of order, even after EV_END message.
   
   */
  
  void
  simple_server::run(uint64_t from)
  {
    // telling our state machine that we have been started
    fsm_.enqueue(EV_START_SERVER);
    last_state_ = fsm_.run(last_state_);
    
    auto pull = [&](uint64_t msg_id,
                    const uint8_t * ptr,
                    uint64_t len)
    {
      try
      {
        if( len > 1 )
        {
          uint8_t  event   = *ptr;
          uint64_t pos     = 1;
          uint64_t remain  = len-1;
          uint64_t id      = 0;
          uint64_t seqno   = 0;
          
          act_message_.position_     = msg_id;
          act_message_.total_bytes_  = len;
          act_message_.event_        = ptr[0];
          act_message_.stream_type_  = 0; // unknown
          
          switch( *ptr )
          {
            case EV_START:
            case EV_ONE:
            {
              uint8_t stream_type = ptr[pos];
              ++pos;
              --remain;
              
              if( get_varint64(ptr, id, pos, remain) )
              {
                act_message_.id_           = id;
                act_message_.seqno_        = seqno;
                act_message_.buffer_       = ptr + pos;
                act_message_.size_         = remain;
                act_message_.stream_type_  = ptr[1];
                
                fsm_.enqueue(*ptr);
              }
              else
              {
                fsm_.enqueue(EV_STREAM_INIT_FAILED);
              }
              break;
            }
              
            case EV_NEXT:
            case EV_END:
            {
              
              if( get_varint64(ptr, id, pos, remain) &&
                  get_varint64(ptr, seqno, pos, remain) )
              {
                act_message_.id_           = id;
                act_message_.seqno_        = seqno;
                act_message_.buffer_       = ptr + pos;
                act_message_.size_         = remain;
                
                fsm_.enqueue(*ptr);
              }
              else
              {
                fsm_.enqueue(EV_BAD_MESSAGE);
              }
              break;
            }
              
            case EV_STOP:   // break; // TODO
            case EV_FIX:    // break; // TODO
            case EV_ERROR:  // break; // TODO
            default:
            {
              fsm_.enqueue(EV_BAD_MESSAGE);
              break;
            }
          };
        }
        last_state_ = fsm_.run(last_state_);
      }
      catch (const std::exception & e)
      {
        std::cerr << "TODO: exception during stream processing: " << e.what() << "\n";
      }
      
      return !is_stopped();
    };
    
    while( !is_stopped() )
    {
      from = pull_data(from, pull, 1000);
    }
    
    // telling our state machine that we have been stoppped
    fsm_.enqueue(EV_STOP_SERVER);
    last_state_ = fsm_.run(last_state_);
  }
  
  // the data stream's state machine may upcall to these
  void
  simple_client::stop(uint64_t id)
  {
  }
  
  bool
  simple_client::wait_data(uint64_t id,
                           uint64_t start_seqno)
  {
    return false;
  }
  
  bool
  simple_client::get_data(uint64_t id,
                          uint64_t seqno,
                          stream_part & part)
  {
    return false;
  }
  
  void
  simple_client::request_resend(uint64_t id,
                                uint64_t seqno)
  {
  }


  bool
  simple_gateway::get_varint64(const uint8_t * ptr,
                               uint64_t & result,
                               uint64_t & position,
                               uint64_t & remaining)
  {
    bool ret = false;
    if( ptr && remaining && remaining > position )
    {
      uint8_t sz = (remaining>10?10:remaining);
      queue::varint v{ptr+position, sz};
      position   += v.len();
      remaining  -= v.len();
      result     = v.get64();
      ret        = true;
    }
    return ret;
  }
  
  void
  simple_server::add_handler(uint8_t stream_type,
                             new_stream_fun new_handler,
                             const state_set & terminal_states,
                             new_info_fun new_info)
  {
    handler::sptr h{new handler};
    h->fsm_factory_      = new_handler;
    h->terminal_states_  = terminal_states;
    h->info_factory_     = new_info;
    handlers_[stream_type].swap(h);
  }
  
  void
  simple_server::push_event(uint64_t id,
                            uint16_t event,
                            bool if_empty)
  {
    auto it = streams_.find(id);
    if( it != streams_.end() )
    {
      if( if_empty )
        it->second->fsm_->enqueue_if_empty(event);
      else
        it->second->fsm_->enqueue(event);
    }
    else
    {
      THROW_(std::string{"invalid stream id:"}+std::to_string(id));
    }
  }
  
  void
  simple_server::stop()
  {
    stopped_ = true;
  }
  
  bool
  simple_server::is_stopped() const
  {
    return stopped_.load();
  }
  
  uint64_t
  simple_gateway::sender_position() const
  {
    uint64_t ret = sender_.position();
    return ret;
  }
  
  uint64_t
  simple_gateway::receiver_position() const
  {
    uint64_t ret = receiver_.position();
    return ret;
  }
  
  void
  simple_gateway::send_data(const queue::simple_publisher::buffer_vector & data)
  {
    sender_.push(data);
  }
  
  uint64_t
  simple_gateway::pull_data(uint64_t from,
                            queue::simple_subscriber::pull_fun f,
                            uint64_t timeout_ms)
  {
    return receiver_.pull(from, f, timeout_ms);
  }
  
  void
  simple_gateway::seek_to_end()
  {
    receiver_.seek_to_end();
  }
  
  void
  simple_gateway::set_event_names(fsm::state_machine & fsm)
  {
    fsm.event_name(EV_START,  "START STREAM");
    fsm.event_name(EV_ONE,    "SINGLE MESSAGE");
    fsm.event_name(EV_NEXT,   "NEXT STREAM");
    fsm.event_name(EV_END,    "END STREAM");
    fsm.event_name(EV_STOP,   "STOP STREAM");
    fsm.event_name(EV_FIX,    "FIX STREAM");
    fsm.event_name(EV_ERROR,  "ERROR");
  }
  
  simple_gateway::simple_gateway(const std::string & base_path,
                                 const std::string & sender_path,
                                 const std::string & receiver_path,
                                 const queue::params & prms)
  : base_path_{base_path},
    sender_{sender_path, prms},
    receiver_{receiver_path, prms}
  {
  }

  simple_client::simple_client(const std::string & path,
                               const queue::params & prms)
  : simple_gateway{path, path+"/0", path+"/1", prms}
  {
  }
  
  simple_gateway::stream_part::stream_part()
  : id_{0},
    seqno_{0},
    buffer_{nullptr},
    size_{0},
    position_{0},
    total_bytes_{0},
    event_{0},
    stream_type_{0}
  {
  }
  
  simple_gateway::stream_info::stream_info()
  : id_{-1},
    sent_seqno_{-1},
    received_seqno_{-1},
    sent_pos_{0},
    received_pos_{0}
  {
  }

  simple_gateway::~simple_gateway() { }
  simple_client::~simple_client() { }
  simple_server::~simple_server() { }
  
  simple_client::sptr
  simple_client::create(const std::string & path,
                        const queue::params & prms)
  {
    try
    {
      // quick try to initialize the other side of the connection
      // which may very well fail because of the missing semaphore of the
      // other channel
      std::unique_ptr<simple_server> tmp{new simple_server{path, prms, [](uint16_t seqno,
                                                                          const std::string & desc,
                                                                          const fsm::transition & trans,
                                                                          const fsm::state_machine & sm){}}};
    }
    catch(...) { }
    
    // this part may throw
    sptr ret{new simple_client{path, prms}};
    return ret;
  }
  
  simple_server::sptr
  simple_server::create(const std::string & path,
                        const queue::params & prms,
                        fsm::state_machine::trace_fun trace_cb)
  {
    try
    {
      // quick try to initialize the other side of the connection
      // which may very well fail because of the missing semaphore of the
      // other channel
      std::unique_ptr<simple_client> tmp{new simple_client{path, prms}};
    }
    catch(...) { }

    // this part may throw
    sptr ret{new simple_server{path, prms, trace_cb}};
    return ret;
  }
  
}}
