#pragma once

#include <exception>
#include <sstream>
#include <string>

namespace virtdb { namespace gateway {

  class exception : public std::exception
  {
    // must pass minimal location information as below
    exception() = delete;

  protected:
    std::string data_;
  
  public:
    exception(const char * file,
              unsigned int line,
              const char * func,
              const char * msg)
    {
      std::ostringstream os;
      os << '[' << file << ':' << line << "] " << func << "() ";
      if( msg ) os << "msg=" << msg << " ";
      data_ = os.str();
    }

    exception(const char * file,
              unsigned int line,
              const char * func,
              const std::string & msg)
    {
      std::ostringstream os;
      os << '[' << file << ':' << line << "] " << func << "() ";
      if( !msg.empty() ) os << "msg=" << msg << " ";
      data_ = os.str();
    }

    virtual ~exception() throw() {}

    const char* what() const throw()
    {
      return data_.c_str();
    }
  };

}}

#ifndef EXCEPTION_GATEWAY_
#define EXCEPTION_GATEWAY_(MSG) virtdb::gateway::exception(__FILE__,__LINE__,__func__,MSG)
#endif // EXCEPTION_GATEWAY_

#ifndef THROW_
#define THROW_(MSG) throw EXCEPTION_GATEWAY_(MSG);
#endif // THROW_

