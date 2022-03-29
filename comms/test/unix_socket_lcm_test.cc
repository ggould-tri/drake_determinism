#include "comms/unix_socket_lcm.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "drake/common/temp_directory.h"


namespace drake_determinism {
namespace comms {

GTEST_TEST(UnixSocketLcmTest, LifecycleTest) {
  const std::string temp_directory = drake::temp_directory();
  const std::string base_url =
      fmt::format("unix:/{}/lcm.socket",
                  temp_directory);
  UnixSocketLcm server(base_url + "?end=server");
  UnixSocketLcm client(base_url + "?end=client");
  // Let them go out of scope to run their dtors.
}

}  // namespace comms
}  // namespace drake_determinism
