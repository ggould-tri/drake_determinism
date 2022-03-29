#include "comms/unix_socket_lcm.h"

#include <errno.h>
#include <filesystem>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <drake/common/text_logging.h>
#include <lcm/lcm-cpp.hpp>

#include "comms/unix_seqpacket.h"

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

 private:
  std::vector<lcm::ReceiveBuffer> queue_;
};

enum UnixSocketEnd {
  kFallback,
  kServer,
  kClient
};

struct SocketConfig {
  std::string name = "";
  UnixSocketEnd end = kFallback;
};

SocketConfig ParseUrl(std::string url) {
  DRAKE_DEMAND(url.substr(0, 5) == "unix:");
  std::string url_body = url.substr(5);
  size_t query_pos = url_body.rfind("?");
  size_t end_of_name = (query_pos != std::string::npos
                        ? query_pos : url_body.size());
  std::string name = url_body.substr(0, end_of_name);
  UnixSocketEnd end = kFallback;
  if (query_pos != std::string::npos) {
    std::string query_string = url_body.substr(query_pos + 1);
    DRAKE_DEMAND(query_string.substr(0, 4) == "end=");
    std::string endpoint_string = query_string.substr(4);
    DRAKE_DEMAND(endpoint_string == "server" || endpoint_string == "client");
    end = (endpoint_string == "client") ? kClient : kServer;
  }
  return SocketConfig{name, end};
}

std::string BuildUrl(SocketConfig config) {
  std::string url = "unix:" + config.name;
  if (config.end != kFallback) {
    url += "?end=" + (config.end == kServer ? "server" : "client");
  }
  return url;
}

} // namespace

class UnixSocketLcm::Impl final {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(Impl)

  explicit Impl(std::string lcm_url)
  : config_(ParseUrl(lcm_url)),
    connection_(config_.name) {
    DRAKE_DEMAND(lcm_url == BuildUrl(config_));
    switch (config_.end) {
      case kClient: connection_.StartAsClient(); break;
      case kServer: connection_.StartAsServer(); break;
      case kFallback: connection_.StartWithFallback(); break;
    };
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
  UnixSeqpacket connection_;
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
