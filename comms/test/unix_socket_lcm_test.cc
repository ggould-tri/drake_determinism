#include "comms/unix_socket_lcm.h"

#include <chrono>
#include <thread>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <drake/lcmt_scope.hpp>
#include <drake/lcm/drake_lcm_interface.h>

namespace drake_determinism {
namespace comms {

GTEST_TEST(UnixSocketLcmTest, LifecycleTest) {
  const std::string base_url = "unix:/UnixSocketLcmTest_LifecycleTest.socket";
  {
    // Test with client/server URLs.
    auto server = std::thread(
        [&]() { UnixSocketLcm server(base_url + "?end=server"); });
    auto client = std::thread(
        [&]() { UnixSocketLcm client(base_url + "?end=client"); });
    server.join();
    client.join();
  }
  {
    // Test with autodetect URLs.
    auto first = std::thread(
        [&]() { UnixSocketLcm first(base_url); });
    auto second = std::thread(
        [&]() { UnixSocketLcm second(base_url); });
    first.join();
    second.join();
  }
}

GTEST_TEST(UnixSocketLcmTest, SingleMessageTest) {
  const std::string base_url =
      "unix:/UnixSocketLcmTest_SingleMessageTest.socket";

  // Test one message in each direction.
  const drake::lcmt_scope first_sent{.utime = 1,
                                     .size = 2,
                                     .value = {0.2, 0.3}};
  const drake::lcmt_scope second_sent{.utime = 2,
                                      .size = 3,
                                      .value = {0.4, 0.6, 0.8}};
  std::atomic<bool> first_done = false;
  std::atomic<bool> second_done = false;

  auto first = std::thread(
      [&]() {
        UnixSocketLcm lcm(base_url);
        drake::lcm::Subscribe<drake::lcmt_scope>(
            &lcm, "test_channel",
            [&](const drake::lcmt_scope& msg) {
              DRAKE_DEMAND(msg.utime == second_sent.utime);
              DRAKE_DEMAND(msg.size == second_sent.size);
              DRAKE_DEMAND(msg.value == second_sent.value);
              first_done = true;
            });
        drake::lcm::Publish(&lcm, "test_channel", first_sent);
        lcm.HandleSubscriptions(1000);
      });
  auto second = std::thread(
      [&]() {
        UnixSocketLcm lcm(base_url);
        drake::lcm::Subscribe<drake::lcmt_scope>(
            &lcm, "test_channel",
            [&](const drake::lcmt_scope& msg) {
              DRAKE_DEMAND(msg.utime == first_sent.utime);
              DRAKE_DEMAND(msg.size == first_sent.size);
              DRAKE_DEMAND(msg.value == first_sent.value);
              second_done = true;
            });
        drake::lcm::Publish(&lcm, "test_channel", second_sent);
        lcm.HandleSubscriptions(1000);
      });
  first.join();
  second.join();
  DRAKE_DEMAND(first_done);
  DRAKE_DEMAND(second_done);
}

}  // namespace comms
}  // namespace drake_determinism
