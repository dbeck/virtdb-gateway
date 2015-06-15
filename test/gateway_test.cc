#include <gtest/gtest.h>
#include <gateway/exception.hh>
#include <gateway/simple_gateway.hh>
#include <gateway/zmq_gateway.hh>
#include <gateway/virtdb_gateway.hh>
#include <future>
#include <iostream>
#include <string.h>
#include <map>

using namespace virtdb::gateway;

namespace virtdb { namespace test {
  
  class SimpleGatewayTest : public ::testing::Test { };
  
}}

using namespace virtdb::test;

TEST_F(SimpleGatewayTest, Dummy)
{
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
}
