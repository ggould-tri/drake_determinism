#include "comms/unix_seqpacket.h"

#include <errno.h>
#include <filesystem>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <drake/common/drake_assert.h>
#include <drake/common/text_logging.h>

namespace drake_determinism {
namespace comms {

// NOTE: Throughout this file I will use the `struct` keyword when talking
// about sockaddrs; this is conventional in Unix due to the underlying C APIs.

namespace {

// Convenience function for styleguide-disfavored pointer magic.
struct sockaddr* upcast_sockaddr_un(struct sockaddr_un* input) {
  // Pointer-reinterpretation is a common idiom for posix sockets, in
  // effect acting as a less safe version of superclass dynamic_cast<>.
  return reinterpret_cast<struct sockaddr*>(input);
}

// Do not permit any nontrivial connect/accept backlogging.
constexpr static int kBacklog = 1;

void DemandWithErrno(const std::string& description, bool demand) {
  if (!demand) {
    drake::log()->error("{} failed ({}): {})",
                        description, errno, strerror(errno));
  }
  DRAKE_DEMAND(demand);
}

}  // namespace

UnixSeqpacket::UnixSeqpacket(const std::string& abstract_name)
    : abstract_name_(abstract_name) {
  DRAKE_DEMAND(abstract_name.size() < 106);  // See `man 7 unix`.
}

UnixSeqpacket::~UnixSeqpacket() {
  if (server_fd_) {
    close(*server_fd_);
    server_fd_ = std::nullopt;
  }
  if (endpoint_fd_) {
    close(*endpoint_fd_);
    endpoint_fd_ = std::nullopt;
  }
}

int UnixSeqpacket::StartAsServer() {
  memset(&server_sockaddr_, 0, sizeof(struct sockaddr_un));
  server_sockaddr_.sun_family = AF_UNIX;
  strncpy(&server_sockaddr_.sun_path[1],
          abstract_name_.c_str(), abstract_name_.length());
  server_fd_ = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  DemandWithErrno("socket()", *server_fd_ > 0);

  int err = ::bind(*server_fd_, upcast_sockaddr_un(&server_sockaddr_),
                   sizeof(sa_family_t) + abstract_name_.length() + 1);
  if (err) {
    return errno;
  }

  err = listen(*server_fd_, 1);
  DemandWithErrno("listen()", err == 0);

  socklen_t endpoint_sockaddr_len = sizeof(endpoint_sockaddr_);
  endpoint_fd_ = accept(
      *server_fd_,
      upcast_sockaddr_un(&endpoint_sockaddr_),
      &endpoint_sockaddr_len);
  DemandWithErrno("accept()", *endpoint_fd_ > 0);
  return 0;
}

void UnixSeqpacket::StartAsClient() {
  memset(&endpoint_sockaddr_, 0, sizeof(struct sockaddr_un));
  endpoint_sockaddr_.sun_family = AF_UNIX;
  strncpy(&endpoint_sockaddr_.sun_path[1],
          abstract_name_.c_str(), abstract_name_.length());
  endpoint_fd_ = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  DemandWithErrno("socket()", *endpoint_fd_ > 0);

  int err = connect(*endpoint_fd_, upcast_sockaddr_un(&endpoint_sockaddr_),
                    sizeof(sa_family_t) + abstract_name_.length() + 1);
  DemandWithErrno("connect()", err == 0);
}

void UnixSeqpacket::StartWithFallback() {
  if (StartAsServer() == EADDRINUSE) { StartAsClient(); }
}

int UnixSeqpacket::fd() const {
  DRAKE_DEMAND(endpoint_fd_.has_value());
  return *endpoint_fd_;
}

}  // namespace comms
}  // namespace drake_determinism
