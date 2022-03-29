#include <optional>
#include <string>
#include <sys/un.h>

namespace drake_determinism {
namespace comms {

/**
 * Abstracts around the Unix domain socket SOCK_SEQPACKET semantics, in order
 * to firewall off legacy C interfaces from C++ code so that nobody has to
 * look at evil casts and so that all of the stateful baggage is RAII.
 *
 * Internally this uses Linux's "abstract" naming scheme, in which names are
 * not bound to the filesystem.  This forgoes any possibilty of proper access
 * control, but is necessary because clients that must use
 * drake::temp_directory() for filenames (i.e. unit tests) cannot comply with
 * the AF_UNIX limit of 108 character filenames.
 *
 * This class is not and cannot be MacOS compatible, as Apple provides neither
 * SOCK_SEQPACKET nor abstract naming.  Insert rude emoji here.
 */
class UnixSeqpacket final {
 public:
  /**
   * This method does nothing but record the provided name.
   * @p abstract_name must be less than 107 characters. */
  explicit UnixSeqpacket(const std::string& abstract_name);

  ~UnixSeqpacket();

  /**
   * Creates and binds a server socket, listens on it, and accepts exactly
   * one incoming connection.
   *
   * @returns the resulting error, if any, as some may be recoverable.
   *
   * @warning Blocking method. */
  int StartAsServer();

  /**
   * Creates and connects a client socket.
   *
   * @warning Blocking method. */
  void StartAsClient();

  /**
   * Attempts to start as server, but if the address is already in use
   * attempts to start as client.
   *
   * @warning Blocking method. */
  void StartWithFallback();

  /**
   * If `StartAsServer` or `StartAsClient` has been called, returns the file
   * descriptor of the connected endpoint.  Otherwise asserts.
   *
   * Per the class comment, the returned fd will have blocking writes,
   * nonblocking reads, and datagram semantics. */
  int fd() const;

 private:
  std::string abstract_name_;
  std::optional<int> server_fd_;
  struct sockaddr_un server_sockaddr_;
  std::optional<int> endpoint_fd_;
  struct sockaddr_un endpoint_sockaddr_;
};

}  // namespace comms
}  // namespace drake_determinism
