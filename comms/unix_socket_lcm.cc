#include "comms/unix_socket_lcm.h"

namespace drake_determinism {
namespace comms {

namespace {

using drake::lcm::DrakeLcmInterface;
using drake::lcm::DrakeSubscriptionInterface;
using HandlerFunction = DrakeLcmInterface::HandlerFunction;
using MultichannelHandlerFunction =
    DrakeLcmInterface::MultichannelHandlerFunction;

/*
class UnixSocketLcmSubscription final : public DrakeSubscriptionInterface {
 public:
  // DrakeLcm keeps this pinned; we will do likewise to minimize differences.
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(DrakeSubscription)

};
*/

} // namespace

class UnixSocketLcm::Impl {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(Impl)

  explicit Impl(std::string lcm_url)
  : url_(lcm_url) {
    ;  // ...
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
    return url_;
  }

  std::shared_ptr<DrakeSubscriptionInterface> Subscribe(
      const std::string& /* channel */, HandlerFunction /* callback */) {
    return nullptr;  // ...
  }

  std::shared_ptr<DrakeSubscriptionInterface> SubscribeAllChannels(
      MultichannelHandlerFunction /* callback */) {
    return nullptr;  // ...
  }

  int HandleSubscriptions(int timeout_millis) {
    return 0;  // ...
  }

 private:
  std::string url_;
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
