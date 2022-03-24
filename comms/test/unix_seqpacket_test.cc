#include "comms/unix_seqpacket.h"

#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <gtest/gtest.h>
#include <fmt/format.h>


namespace drake_determinism {
namespace comms {

std::string get_safe_name(std::string suffix="") {
  return fmt::format("{}{}", getpid(), suffix);
}

GTEST_TEST(LifecycleTest, UnixSeqpacketTest) {
  UnixSeqpacket u(get_safe_name());
}

GTEST_TEST(ClientServerTest, UnixSeqpacketTest) {
  const std::string name = get_safe_name();
  const std::string server_to_client = "server-to-client message";
  const std::string client_to_server = "client-to-server message";
  auto run_server = [&]() {
                      UnixSeqpacket server(name);
                      server.StartAsServer();
                      int written = write(server.fd(), server_to_client.c_str(),
                                          server_to_client.length() + 1);
                      EXPECT_EQ(written, server_to_client.length() + 1);
                      char buf[256];
                      recv(server.fd(), &buf, 256, 0);
                      EXPECT_EQ(std::string(buf), client_to_server);
                    };
  auto run_client = [&]() {
                      UnixSeqpacket client(name);
                      client.StartAsClient();
                      int written = write(client.fd(), client_to_server.c_str(),
                                          client_to_server.length() + 1);
                      EXPECT_EQ(written, client_to_server.length() + 1);
                      char buf[256];
                      recv(client.fd(), &buf, 256, 0);
                      EXPECT_EQ(std::string(buf), server_to_client);
                    };
  std::thread server_thread(run_server);
  std::thread client_thread(run_client);
  client_thread.join();
  server_thread.join();
}

}  // namespace comms
}  // namespace drake_determinism
