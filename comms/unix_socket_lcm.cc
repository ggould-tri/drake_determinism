#include "comms/unix_socket_lcm.h"

#include <filesystem>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <drake/common/text_logging.h>

namespace drake_determinism {
namespace comms {

namespace {

using drake::lcm::DrakeLcmInterface;
using drake::lcm::DrakeSubscriptionInterface;
using HandlerFunction = DrakeLcmInterface::HandlerFunction;
using MultichannelHandlerFunction =
    DrakeLcmInterface::MultichannelHandlerFunction;

class UnixSocketLcmSubscription final : public DrakeSubscriptionInterface {
 public:
  // DrakeLcm keeps this pinned; we will do likewise to minimize differences.
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(UnixSocketLcmSubscription)

  UnixSocketLcmSubscription(const std::string& channel,
                            HandlerFunction single_channel_handler) {
    ;  // ...
  }

  UnixSocketLcmSubscription(
      MultichannelHandlerFunction single_channel_handler) {
    ;  // ...
  }

  void set_unsubscribe_on_delete(bool /* enabled */) final {
    ;  // ...
  }

  void set_queue_capacity(int) final {
    // We let the socket be our queue for now.
  }

  ~UnixSocketLcmSubscription() {
    ;  // ... ???
  }
};

enum UnixSocketEnd {
  kServer,
  kClient
};

struct SocketConfig {
  std::string filename;
  UnixSocketEnd end;
};

SocketConfig ParseUrl(std::string url) {
  DRAKE_DEMAND(url.substr(0, 5) == "unix:");
  size_t query_pos = url.rfind("?");
  DRAKE_DEMAND(query_pos != std::string::npos);
  std::string filename_base = url.substr(6, query_pos - 6);
  DRAKE_DEMAND(filename_base[0] == '/');
  std::string query_string = url.substr(query_pos);
  DRAKE_DEMAND(query_string.substr(0, 4) == "end=");
  std::string endpoint_string = query_string.substr(4);
  DRAKE_DEMAND(endpoint_string == "server" || endpoint_string == "client");
  UnixSocketEnd end = (endpoint_string == "client") ? kClient : kServer;
  std::string output_socket = filename_base + "-" + endpoint_string;
  std::string input_socket = filename_base + "-" + (end == kServer
                                                    ? "server" : "client");
  return SocketConfig{filename_base, input_socket, output_socket, end};
}

std::string BuildUrl(SocketConfig config) {
  return "unix:" + config.filename_base + "?end=" + (config.end == kServer
                                                     ? "server" : "client");
}

int CreateServerSocketAndAwait(std::filesystem::path path) {
  int server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  DRAKE_DEMAND(server_sock != -1);

  struct sockaddr_un server_sockaddr;
  server_sockaddr.sun_family = AF_UNIX;
  strcpy(server_sockaddr.sun_path, path);
  unlink(path);

  int bind_result = bind(server_sock,
                         static_cast<struct sockaddr*>(&server_sockaddr),
                         sizeof(server_sockaddr));
  if (bind_result == -1) {
    close(server_sock);
    DRAKE_DEMAND(bind_result != -1);
  }

  drake::log->info("Created server socket and listening for connections: {}",
                   path);

  int listen_result = listen(server_sock, backlog);
  if (listen_result == -1) {
    close(server_sock);
    DRAKE_DEMAND(listen_result != -1);
  }

  drake::log->info("Got a connection on socket: {}", path);
}

int ConnectClientSocket(std::filesystem::path path) {

}

} // namespace

class UnixSocketLcm::Impl {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(Impl)

  explicit Impl(std::string lcm_url)
  : config_(ParseUrl(lcm_url)) {
    DRAKE_DEMAND(lcm_url == BuildUrl(config_));
  }

  ~Impl() {
    ;  // ...  (stop threads)
  }

  void Publish(const std::string& /* channel */,
               const void* /* data */, int /* data_size */,
               std::optional<double> /* timestamp */) {
    ;  // ...
  }

  std::string get_lcm_url() const {
    return BuildUrl(config_);
  }

  std::shared_ptr<DrakeSubscriptionInterface> Subscribe(
      const std::string& channel, HandlerFunction callback) {
    return std::make_shared<UnixSocketLcmSubscription>(channel, callback);
  }

  std::shared_ptr<DrakeSubscriptionInterface> SubscribeAllChannels(
      MultichannelHandlerFunction callback) {
    return std::make_shared<UnixSocketLcmSubscription>(callback);
  }

  int HandleSubscriptions(int timeout_millis) {
    return 0;  // ...
  }

 private:
  SocketConfig config_;
};

UnixSocketLcm::UnixSocketLcm(std::string lcm_url)
    : impl_(std::make_unique<Impl>(lcm_url)) {}

void UnixSocketLcm::Publish(const std::string& channel,
                            const void* data, int data_size,
                            std::optional<double> timestamp) {
  impl_->Publish(channel, data, data_size, timestamp);
}

std::string UnixSocketLcm::get_lcm_url() const {
  return impl_->get_lcm_url();
}

std::shared_ptr<DrakeSubscriptionInterface> UnixSocketLcm::Subscribe(
    const std::string& channel, HandlerFunction callback) {
  return impl_->Subscribe(channel, callback);
}

std::shared_ptr<DrakeSubscriptionInterface> UnixSocketLcm::SubscribeAllChannels(
    MultichannelHandlerFunction callback) {
  return impl_->SubscribeAllChannels(callback);
}

int UnixSocketLcm::HandleSubscriptions(int timeout_millis) {
  return impl_->HandleSubscriptions(timeout_millis);
}


}  // namespace comms
}  // namespace drake_determinism
