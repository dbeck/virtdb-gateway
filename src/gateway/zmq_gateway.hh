#pragma once

namespace virtdb { namespace gateway {
  
  class zmq_gateway
  {
    // disable default construction
    // zmq_gateway() = delete;
    
    // disable copying until properly implemented
    zmq_gateway(const zmq_gateway &) = delete;
    zmq_gateway & operator=(const zmq_gateway &) = delete;
    
  public:
    zmq_gateway();
    virtual ~zmq_gateway();
  };
  
}}
