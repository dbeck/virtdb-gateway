#pragma once

namespace virtdb { namespace gateway {
  
  class virtdb_gateway
  {
    // disable default construction
    // virtdb_gateway() = delete;
    
    // disable copying until properly implemented
    virtdb_gateway(const virtdb_gateway &) = delete;
    virtdb_gateway & operator=(const virtdb_gateway &) = delete;
    
  public:
    virtdb_gateway();
    virtual ~virtdb_gateway();
  };
  
}}
