#pragma once

#include <memory>
#include <optional>
#include <string>

#include <drake/common/drake_copyable.h>
#include <drake/lcm/drake_lcm_interface.h>

namespace drake_determinism {
namespace comms {

/**
 * An implementation of DrakeLcmInterface that uses LCM message serialization
 * over a unix domain socket.
 *
 * A unix domain socket is a reliable, in-order, two-way connection between
 * *exactly two endpoints*.  This class uses linux "abstract" sockets which
 * are named with an arbitrary string of up to 106 characters.  The two
 * endpoints of this LCM implementation are called "server" and "client"; the
 * server must start first but they are otherwise identical.
 *
 * URLs for this LCM interface take the form:
 *
 *  unix:/<arbitrary_name>[?end=(server|client)]
 *
 * If the end is omitted, the interface will attempt to detect which end it is
 * and start in the appropriate mode.
 *
 * The underlying serialization format is the LCM event log format.
 */
class UnixSocketLcm final : public drake::lcm::DrakeLcmInterface {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(UnixSocketLcm);

  /**
   * Maximum packet size; chosen arbitrarily.  There is little advantage to
   * fragmentation since everything ends up buffered in memory regardless.
   */
  constexpr static int kMtu = 1 << 15;

  /**
   * Constructs using the given URL.  A receive thread will be started.
   * @warning THIS BLOCKS.
   */
  explicit UnixSocketLcm(std::string lcm_url);
  ~UnixSocketLcm() final;

  void Publish(const std::string&, const void*, int,
               std::optional<double>) override;
  std::string get_lcm_url() const override;
  std::shared_ptr<drake::lcm::DrakeSubscriptionInterface> Subscribe(
      const std::string&,
      drake::lcm::DrakeLcmInterface::HandlerFunction) override;
  std::shared_ptr<drake::lcm::DrakeSubscriptionInterface> SubscribeAllChannels(
      drake::lcm::DrakeLcmInterface::MultichannelHandlerFunction) override;
  int HandleSubscriptions(int) override;
  void OnHandleSubscriptionsError(const std::string&) override;

 private:
  // Use a pimpl pattern for consistency with DrakeLcm.
  class Impl;
  const std::unique_ptr<Impl> impl_;
};

}  // namespace comms
}  // namespace drake_determinism
