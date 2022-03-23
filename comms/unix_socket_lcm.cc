#include "comms/unix_socket_lcm.h"

#include <errno.h>
#include <filesystem>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <drake/common/text_logging.h>

namespace drake_determinism {
namespace comms {

namespace {

using drake::lcm::DrakeLcmInterface;
using drake::lcm::DrakeSubscriptionInterface;
using HandlerFunction = DrakeLcmInterface::HandlerFunction;
using MultichannelHandlerFunction =
    DrakeLcmInterface::MultichannelHandlerFunction;

// By definition a unix socket can never have nontrivial connection backlog.
constexpr static int kBacklog = 1;

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
  std::string filename = "";
  UnixSocketEnd end = kClient;
};

SocketConfig ParseUrl(std::string url) {
  DRAKE_DEMAND(url.substr(0, 5) == "unix:");
  std::string url_body = url.substr(5);
  size_t query_pos = url_body.rfind("?");
  DRAKE_DEMAND(query_pos != std::string::npos);
  std::string filename = url_body.substr(0, query_pos);
  DRAKE_DEMAND(filename[0] == '/');
  std::string query_string = url_body.substr(query_pos + 1);
  DRAKE_DEMAND(query_string.substr(0, 4) == "end=");
  std::string endpoint_string = query_string.substr(4);
  DRAKE_DEMAND(endpoint_string == "server" || endpoint_string == "client");
  UnixSocketEnd end = (endpoint_string == "client") ? kClient : kServer;
  return SocketConfig{filename, end};
}

std::string BuildUrl(SocketConfig config) {
  return "unix:" + config.filename + "?end=" + (config.end == kServer
                                                ? "server" : "client");
}

// Convenience function for styleguide-disfavored pointer magic.
struct sockaddr* upcast_sockaddr_un(struct sockaddr_un* input) {
  // Pointer-reinterpretation is a common idiom for posix sockets, in
  // effect acting as a less safe version of superclass dynamic_cast<>.
  return reinterpret_cast<struct sockaddr*>(input);
}

int CreateServerSocketAndAwait(std::filesystem::path path) {
  int server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  DRAKE_DEMAND(server_sock != -1);

  struct sockaddr_un server_sockaddr;
  socklen_t server_sockaddr_len = sizeof(server_sockaddr);
  server_sockaddr.sun_family = AF_UNIX;
  strcpy(server_sockaddr.sun_path, path.c_str());
  std::filesystem::remove(path);

  int bind_result = bind(
      server_sock,
      upcast_sockaddr_un(&server_sockaddr),
      server_sockaddr_len);
  if (bind_result == -1) {
    drake::log()->error("Bind error: {}", strerror(errno));
    close(server_sock);
    DRAKE_DEMAND(bind_result != -1);
  }

  drake::log()->info("Created server socket and listening for connections: {}",
                     path);
  int listen_result = listen(server_sock, kBacklog);
  if (listen_result == -1) {
    drake::log()->error("Listen error: {}", strerror(errno));
    close(server_sock);
    DRAKE_DEMAND(listen_result != -1);
  }
  struct sockaddr_un client_sockaddr;
  socklen_t client_sockaddr_len = sizeof(client_sockaddr);
  int client_sock = accept(
      server_sock,
      upcast_sockaddr_un(&client_sockaddr),
      &client_sockaddr_len);
  if (client_sock == -1){
    drake::log()->error("Accept error: {}", strerror(errno));
    close(server_sock);
    close(client_sock);
    exit(1);
  }

  drake::log()->info("Got a connection on socket: {}", path);
  return client_sock;
}

int ConnectClientSocket(std::filesystem::path path) {
  int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  struct sockaddr_un client_sockaddr;
  socklen_t client_sockaddr_len = sizeof(client_sockaddr);

  if (sock < 0) {
    drake::log()->error("Error creating client socket: {}", strerror(errno));
    DRAKE_DEMAND(sock >= 0);
  }
  client_sockaddr.sun_family = AF_UNIX;
  strcpy(client_sockaddr.sun_path, path.c_str());
  connect(sock,
          upcast_sockaddr_un(&client_sockaddr),
          client_sockaddr_len);
  return sock;
}

} // namespace

class UnixSocketLcm::Impl final {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(Impl)

  explicit Impl(std::string lcm_url)
  : config_(ParseUrl(lcm_url)) {
    DRAKE_DEMAND(lcm_url == BuildUrl(config_));
    DRAKE_DEMAND(config_.filename.size() < 108);  // see `man 7 unix`.
    int result = -999;
    if (config_.end == kServer) {
      result = CreateServerSocketAndAwait(config_.filename);
    } else if (config_.end == kClient) {
      result = ConnectClientSocket(config_.filename);
    }
    if (result < 0) {
      drake::log()->error("Bind error: {}", strerror(errno));
      DRAKE_DEMAND(result >= 0);
    }
    fd_ = result;
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

  void OnHandleSubscriptionsError(const std::string& err) {
    ;  // ...
  }

 private:
  SocketConfig config_;
  int fd_{};
};

UnixSocketLcm::UnixSocketLcm(std::string lcm_url)
    : impl_(std::make_unique<Impl>(lcm_url)) {}

UnixSocketLcm::~UnixSocketLcm() {}

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

void UnixSocketLcm::OnHandleSubscriptionsError(const std::string& err) {
  impl_->OnHandleSubscriptionsError(err);
}


}  // namespace comms
}  // namespace drake_determinism
