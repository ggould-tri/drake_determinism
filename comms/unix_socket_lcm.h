#pragma once

#include <memory>
#include <optional>
#include <string>

#include <drake/common/drake_copyable.h>
#include <drake/lcm/drake_lcm_interface.h>

namespace drake_determinism {
namespace comms {

/**
 * An implementation of DrakeLcmInterface that uses unix domain sockets.
 *
 * A unix domain seqpacket socket is a reliable, in-order, two-way connection
 * between *exactly two endpoints*.  They are typically created in pairs, and
 * are named and tracked via filesystem nodes.
 *
 * The two endpoints of this LCM implementation are called "server" and
 * "client"; they are otherwise identical.
 *
 * URLs for this LCM interface take the form:
 *
 *  unix:/absolute/filename_base?end=[server|client]
 *
 * For instance, a common use case might look like this:
 *
 *  unix:/tmp/my_app-12345/lcm.socket?end=server
 *
 * It is the responsibility of the caller to ensure that the directory of this
 * URL exists and is readable and writeable by this process.  Common practice
 * is to use a per-instance directory under `/tmp`.
 */
class UnixSocketLcm : public drake::lcm::DrakeLcmInterface {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(UnixSocketLcm);

  /**
   * Constructs using the given URL.  A receive thread will be started.
   */
  explicit UnixSocketLcm(std::string lcm_url);

  void Publish(const std::string&, const void*, int,
               std::optional<double>) override;
  std::string get_lcm_url() const override;
  std::shared_ptr<drake::lcm::DrakeSubscriptionInterface> Subscribe(
      const std::string&,
      drake::lcm::DrakeLcmInterface::HandlerFunction) override;
  std::shared_ptr<drake::lcm::DrakeSubscriptionInterface> SubscribeAllChannels(
      drake::lcm::DrakeLcmInterface::MultichannelHandlerFunction) override;
  int HandleSubscriptions(int) override;

 private:
  // Use a pimpl pattern for consistency with DrakeLcm.
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace comms
}  // namespace drake_determinism
