#pragma once

#include <fsm/state_machine.hh>
#include <set>
#include <memory>

namespace virtdb { namespace gateway {
  
  struct stream_fsm
  {
    fsm::state_machine::sptr  state_machine_;
    std::set<uint16_t>        final_states_;
    uint16_t                  act_state_;
    
    typedef std::shared_ptr<stream_fsm> sptr;
  };
}}

