#include "comms/unix_socket_lcm.h"

#include <arpa/inet.h>
#include <errno.h>
#include <filesystem>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <fmt/format.h>
#include <drake/common/text_logging.h>
#include <lcm/lcm-cpp.hpp>
#include <lcm/eventlog.h>

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

// Annoyingly Posix doesn't provide 64-bit ntohl.  This preprocessor mumbling
// is a fairly common idiom to provide it.
#if __BIG_ENDIAN__
# define htonll(x) (x)
# define ntohll(x) (x)
#else
# define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) \
                    | htonl((x) >> 32))
# define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) \
                    | ntohl((x) >> 32))
#endif

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
  std::string query_string = (config.end == kServer ? "?end=server"
                              : config.end == kClient ? "?end=client"
                              : "");
  return fmt::format("unix:{}{}", config.name, query_string);
}

} // namespace

class UnixSocketLcm::Impl final {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(Impl)

  explicit Impl(std::string lcm_url)
  : config_(ParseUrl(lcm_url)),
    connection_(config_.name),
    rx_buffer_(kMtu),
    tx_buffer_(kMtu) {
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

  void Publish(const std::string& channel,
               const void* data, int data_size,
               std::optional<double> timestamp) {
    // NOTE: Since we're emulating LCM's event log serialization we carefully
    // put everything in network byte order, even though it cannot leave this
    // host.
    uint8_t* start = tx_buffer_.data();
    uint8_t* cursor = start;
    *reinterpret_cast<uint64_t*>(cursor) = htonll(event_count_);
    event_count_++;
    cursor += sizeof(uint64_t);
    uint64_t lcm_timestamp = static_cast<uint64_t>(timestamp.value_or(0) * 1e6);
    *reinterpret_cast<uint64_t*>(cursor) = htonll(lcm_timestamp);
    cursor += sizeof(uint64_t);
    *reinterpret_cast<uint32_t*>(cursor) = htonl(channel.size());
    cursor += sizeof(uint32_t);
    *reinterpret_cast<uint32_t*>(cursor) = htonl(data_size);
    cursor += sizeof(uint32_t);
    memcpy(cursor, channel.c_str(), channel.size());
    cursor += channel.size();
    DRAKE_DEMAND(cursor + data_size <= start + kMtu);
    memcpy(cursor, data, data_size);
    cursor += data_size;
    int num_bytes_written = write(connection_.fd(),
                                  tx_buffer_.data(), cursor - start);
    // Disallow fragmentation.
    DRAKE_DEMAND(num_bytes_written == cursor - start);
  }

  // Nonblockingly receives a packet and deserializes its envelope into an lcm
  // event structure (or return nullopt if no packet was waiting).  The return
  // value contains aliases into rx_buffer_ and should be fully processed and
  // discarded before any other receive operations.
  std::optional<lcm_eventlog_event_t> Receive() {
    int num_bytes_received = recv(connection_.fd(), rx_buffer_.data(), kMtu,
                        MSG_DONTWAIT);
    if (num_bytes_received == 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return std::nullopt;
    }
    uint8_t* start = rx_buffer_.data();
    uint8_t* cursor = start;
    lcm_eventlog_event_t result{};
    result.eventnum = ntohll(*reinterpret_cast<uint64_t*>(cursor));
    cursor += sizeof(uint64_t);
    result.timestamp = ntohll(*reinterpret_cast<uint64_t*>(cursor));
    cursor += sizeof(uint64_t);
    result.channellen = ntohl(*reinterpret_cast<uint32_t*>(cursor));
    cursor += sizeof(uint32_t);
    result.datalen = ntohl(*reinterpret_cast<uint32_t*>(cursor));
    cursor += sizeof(uint32_t);
    result.channel = reinterpret_cast<char*>(cursor);
    cursor += result.channellen;
    DRAKE_DEMAND(cursor + result.datalen <= start + kMtu);
    result.data = reinterpret_cast<void*>(cursor);
    cursor += result.datalen;

    // Disallow fragmentation.
    DRAKE_DEMAND(num_bytes_received == cursor - start);
    return result;
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
  uint64_t event_count_ = 0;
  std::vector<uint8_t> rx_buffer_;
  std::vector<uint8_t> tx_buffer_;
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
