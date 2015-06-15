#pragma once

namespace virtdb { namespace gateway {
  
  class simple_gateway
  {
    // disable default construction
    // simple_gateway() = delete;
    
    // disable copying until properly implemented
    simple_gateway(const simple_gateway &) = delete;
    simple_gateway & operator=(const simple_gateway &) = delete;
    
  public:
    simple_gateway();
    virtual ~simple_gateway();
  };
  
}}
