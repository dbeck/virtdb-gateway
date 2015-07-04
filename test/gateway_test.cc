#include <gtest/gtest.h>
#include <gateway/exception.hh>
#include <gateway/simple_gateway.hh>
#include <gateway/streaming_gateway.hh>
#include <gateway/zmq_gateway.hh>
#include <gateway/virtdb_gateway.hh>
#include <future>
#include <iostream>
#include <string.h>
#include <map>
#include <thread>

using namespace virtdb::gateway;
using namespace virtdb::fsm;
using namespace virtdb::queue;

namespace virtdb { namespace test {
  
  class SimpleGatewayTest : public ::testing::Test { };
  class StreamingGatewayTest : public ::testing::Test { };
  
  class FsmTest : public ::testing::Test { };
  
  auto trace = [](uint16_t seqno,
                  const std::string & desc,
                  const transition & trans,
                  const state_machine & sm)
  {
    std::cout << "fsm=(" << sm.description() << ") state=(" << sm.state_name(trans.state()) << ")"
              << "+event=(" << sm.event_name(trans.event()) << ") "
              << " transition=(" << trans.description() << ")  : "
              << "(" << seqno << ") "
              << desc << "\n";
  };
  
}}

using namespace virtdb::test;

TEST_F(SimpleGatewayTest, CreateOnly)
{
  auto server = simple_server::create("/tmp/SimpleGatewayTest.CreateOnly");
  auto client = simple_client::create("/tmp/SimpleGatewayTest.CreateOnly");
}

TEST_F(SimpleGatewayTest, PushSingle)
{
  const char * path = "/tmp/SimpleGatewayTest.PushSingle";

  // sync object
  std::promise<void> notify_on_msg;
  std::future<void> on_msg{notify_on_msg.get_future()};
  
  // server
  auto server = simple_server::create(path, params(), trace);
  server->seek_to_end();
  
  {
    auto new_stream = [&](const simple_gateway::stream_part & start,
                          state_machine::trace_fun trace_cb) {
      std::string name{"PushSingle STREAM:"};
      name += std::to_string(start.id_);
      state_machine::sptr fsm { new state_machine{name, trace_cb} };
      fsm->state_name(0, "INIT");
      fsm->state_name(1, "DONE");
      transition::sptr fake {new transition{0, simple_gateway::EV_ONE, 1, "Single message"}};
      simple_gateway::set_event_names(*fsm);
      fsm->add_transition(fake);
      return fsm;
    };
    
    simple_gateway::state_set terminal_states = { 1 };
    
    auto new_info = [&](uint64_t id) {
      simple_gateway::stream_info::sptr info { new simple_gateway::stream_info };
      info->id_ = id;
      // tell the test that we are ready
      if( on_msg.wait_for(std::chrono::microseconds(1)) != std::future_status::ready )
        notify_on_msg.set_value();
      return info;
    };
    
    server->add_handler(1,
                        new_stream,
                        terminal_states,
                        new_info);
  }
  
  // run server in the background
  std::thread thr{[server](){
    server->run(server->receiver_position());
  }};
  
  // client
  auto client = simple_client::create(path);
  client->seek_to_end();
  
  {
    auto push_data = [&](const std::string & msg) {
      
      auto feeder = [&](simple_gateway::stream_part & p) {
        p.buffer_ = (const uint8_t *)msg.c_str();
        p.size_ = msg.size();
        return false;
      };
      
      state_machine::sptr fsm { new state_machine{"PushSingleClient", trace} };
      simple_gateway::state_set terminal_states = { 0 };
      simple_gateway::stream_info::sptr info { new simple_gateway::stream_info };
      
      client->start(1,
                    feeder,
                    fsm,
                    terminal_states,
                    info);
    };
    push_data("Hello world");
    push_data("Foo");
  }
  
  on_msg.wait();
  
  // cleanup server
  server->stop();
  thr.join();
}

TEST_F(SimpleGatewayTest, PushSingleNoHandler)
{
  const char * path = "/tmp/SimpleGatewayTest.PushSingle.NoReceiver";
  
  // server
  auto server = simple_server::create(path, params(), trace);
  server->seek_to_end();
  
  // run server in the background
  std::thread thr{[server](){
    server->run(server->receiver_position());
  }};
  
  // client
  auto client = simple_client::create(path);
  client->seek_to_end();
  
  {
    auto push_data = [&](const std::string & msg) {
      
      auto feeder = [&](simple_gateway::stream_part & p) {
        p.buffer_ = (const uint8_t *)msg.c_str();
        p.size_ = msg.size();
        return false;
      };
      
      state_machine::sptr fsm { new state_machine{"PushSingleNoReceiver", trace} };
      simple_gateway::state_set terminal_states = { 0 };
      simple_gateway::stream_info::sptr info { new simple_gateway::stream_info };
      
      client->start(1,
                    feeder,
                    fsm,
                    terminal_states,
                    info);
    };
    push_data("Bar");
    push_data("Baz");
  }

  // wait a bit for message arrival
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  // cleanup server
  server->stop();
  thr.join();
}

TEST_F(StreamingGatewayTest, PushSingle)
{
  // TODO
}

TEST_F(StreamingGatewayTest, PushSinglePullSingle)
{
  // TODO
}

TEST_F(StreamingGatewayTest, PushSinglePullStream)
{
  // TODO
}

TEST_F(StreamingGatewayTest, PushStreamPullStream)
{
  // TODO
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
}
