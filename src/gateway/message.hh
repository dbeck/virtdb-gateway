#pragma once

#include <cstdint>
#include <string>
#include <queue/simple_queue.hh>

namespace virtdb { namespace gateway {
  
  class message
  {
  public:
    typedef queue::simple_publisher::buffer_vector buffer_vector;
    
  protected:
    buffer_vector bufvec_;
    
  public:
    message(uint8_t   channel_id,
            uint64_t  msg_id);
    virtual ~message();
    virtual void prepare() = 0;
    const buffer_vector & bufvec() const;
  };
  
  class single_message : public message
  {
  public:
    single_message(uint8_t        channel_id,
                   uint64_t       msg_id,
                   uint8_t        stream_type,
                   const void *   ptr,
                   uint64_t       size);
    virtual ~single_message();
    void prepare();
  };

  class start_message : public message
  {
  public:
    start_message(uint8_t        channel_id,
                  uint64_t       msg_id,
                  uint8_t        stream_type,
                  const void *   ptr,
                  uint64_t       size);
    virtual ~start_message();
    void prepare();
  };

  class next_message : public message
  {
  public:
    next_message(uint8_t        channel_id,
                 uint64_t       msg_id,
                 uint64_t       seq_no,
                 const void *   ptr,
                 uint64_t       size);
    virtual ~next_message();
    void prepare();
  };

  class last_message : public message
  {
  public:
    last_message(uint8_t        channel_id,
                 uint64_t       msg_id,
                 uint64_t       seq_no,
                 const void *   ptr,
                 uint64_t       size);
    virtual ~last_message();
    void prepare();
  };

  class stop_message : public message
  {
  public:
    stop_message(uint8_t              channel_id,
                 uint64_t             msg_id,
                 const std::string &  reason);
    virtual ~stop_message();
    void prepare();
  };

  class fix_message : public message
  {
  public:
    fix_message(uint8_t        channel_id,
                uint64_t       msg_id,
                uint64_t       seq_no);
    virtual ~fix_message();
    void prepare();
  };

  class error_message : public message
  {
  public:
    error_message(uint8_t              channel_id,
                  uint64_t             msg_id,
                  uint64_t             seq_no,
                  const std::string &  reason);
    virtual ~error_message();
    void prepare();
  };
}}
