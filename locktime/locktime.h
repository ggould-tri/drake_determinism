#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

/// Implementation of the locked-time algorithm for synchronizing components
/// in a simulation to obtain deterministic logical dependencies.   In this
/// way, if every clients is individually determinisitic, the system as a
/// whole will be deterministic.
///
/// The system as a whole consists of a number of independent computation
// processes (processes, threads, event loops, whatever) abstractly called
/// "Clients" that share a common monotonic understanding of time, and which
/// may send and receive potentially information-bearing synchronization
/// points ("Messages").
///
/// When a client sends a message, it is assumed to depend on every message
/// previously received by that client.  In this way, the flow of messages
/// in the system is a directed acyclic graph, and correspondingly a partial
/// order on the messages.  "LockTime" is a scalar whose total order obeys the
/// partial order of messages.  Clients are allowed to depend on LockTime
/// with their own semantics; the server simply maintains relationships
/// between times.
///
/// This is accomplished by controlling when clients are permitted to receive
/// messages (and therefore to carry out computation that depends on those
/// messages).  This works if and only if clients obey some laws:
///
/// * Clients operate in a periodic polling mode of message receipt.
/// * No client may obtain a time signal from any source other than the locked
///   time server.
/// * No client may communicate with external components except via the
///   locked time mechanism.
///
/// To put it another way, a client's sent messages at a LockTime-announced
/// time t must be a function of its initial state, LockTime-received messages
/// prior to t, and t itself.
///
/// A typical client will periodically poll for new messages and
/// perform computation based on those messages.  
///
/// In the default case of all clients depending on all messages, this
/// algorithm would simply be a gated event loop and no multiprocessing
/// permitted.  To enable multiprocessing, latency is added to message
/// delivery and clients are permitted to pre-announce future read and write
/// times.
template<typename Message>
class LockTimeServer {
  public:
    /// An unique ID assigned to clients, counting up from zero; real clients
    /// will have ids >=0; negative values are reserved for implementation use.
    ///
    /// NOTE:  In some edge cases the client ID is used to resolve otherwise
    /// simultaneous events.  As such the order of client registration is part
    /// of the determinism boundary.  This may be a bug.
    using ClientId = int;

    /// The notion of time visible to and used by clients.  Note that messages
    /// with the same LockTime still maintain an arbitrary-but-stable order.
    using LockTime = double;

    /// All of the information about a Message at the time that it is
    /// delivered into a client's receive queue; the structure that the
    /// client will ultimately receive at the appropriate time.
    struct MessageWithEnvelope {
        /// The ownership point of the Message.
        std::unique_ptr<Message> message;

        /// What client sent this message.
        ///
        /// Clients are numbered from 0; negative IDs are reserved for
        /// implementation use (e.g. logging and announcements), will be
        /// ignored by the server, and may be discarded by clients.
        ClientId sender_id;

        // What client will receive this message.
        ClientId recipient_id;

        /// A time the sender could have sent this message within the server's
        /// understanding of data dependencies.
        LockTime send_time;

        /// A time the receiver could have received this message within
        /// the server's understanding of data dependencies.
        LockTime delivery_time;

        /// Ordering for `MessageQueue`.
        bool operator<(const MessageWithEnvelope& other) const {
          if (send_time < other.send_time) return true;
          if (send_time > other.send_time) return false;
          return (sender_id < other.sender_id);
        }
    };

    /// NOTE:  Since C++03 a C++ "multiset" is conceptually just a stable
    /// sorted vector.
    using MessageQueue = std::multiset<MessageWithEnvelope>;

    /// A function callback provided by clients so that the server can
    /// deliver their pending messages.  The message vector will be sorted
    /// by `delivery_time` and should be processed in order.
    using AllowAdvanceFn = std::function<void(LockTime, const MessageQueue&)>;

    /// Create a LockTimeServer.  Time will be initialized to zero.
    LockTimeServer(
      LockTime delivery_latency = 0.005) 
        : delivery_latency_(delivery_latency) {}

    /// Attach a client to this server.  The name must be unique.
    /// The client provides a function whereby it can be authorized to
    /// advance its time and receive the corresponding messages.
    ///
    /// Returns the client's ID and the client's starting time, ie, the
    /// earliest time the client is permitted to mention or depend on.
    ///
    /// After this call the client should AnnounceClientWork to its new
    /// time along with any startup messages.
    ///
    /// NOTE:  For performance, this method should allow clients to enroll
    /// a message filtering function, to allow users to implement pub/sub
    /// systems and reduce copying.  However reasoning about the time-locked
    /// behaviour of such a filtering function is subtle (e.g. at what
    /// LockTime does a subscription take effect, and how can that be causally
    /// enforced?) and so this is omitted for now.
    std::pair<ClientId, LockTime> AttachClient(
        std::string client_name,
        AllowAdvanceFn advance_callback) {
      LockTime client_time = NoMoreReceivesTime();
      clients_.push_back(
        ClientInfo{
          .name = client_name,
          .callback = advance_callback,
          .earliest_possible_client_time = client_time,
          .latest_possible_client_time = client_time,
          .message_queue{}});
      ClientId id = clients_.size();
      return {id, client_time};
    }

    /// Advance the time on sender @p sender_id and send the accompanying
    /// messages.  The messages' delivery times will be ignored.
    void AnnounceClientWork(
        ClientId client_id,
        LockTime new_client_time,
        MessageQueue* to_be_sent) {
      while (to_be_sent->size()) {
        auto message_handle = to_be_sent->extract(to_be_sent->lower_bound());
        assert(message_handle.value().send_time <= new_client_time);
        clients_[message_handle.value().receiver_id].insert(
          std::move(message_handle));
      }
      clients_[client_id].last_send_time = new_client_time;
      AdvanceClientTimes();
    };

    /// Inform the server that a client will not send or process messages
    /// prior to `silent_until`, e.g. because the client sends periodically,
    /// or has completed a polling cycle, or it has a built-in processing
    /// latency, or some similar reason.
    ///
    /// A client is never required to send this message, but it is polite
    /// to do so as it allows greater multiprocessing.
    ///
    /// The client is not required to have been authorized to this time.
    void DeclareSilentUntil(
        ClientId client_id,
        LockTime silent_until) {
      ClientInfo* client = &clients_[client_id];
      assert(silent_until >= client->earliest_possible_client_time);
      client->earliest_possible_client_time = silent_until;
    }

  private:
    // All per-client state.
    struct ClientInfo {
        std::string name;
        AllowAdvanceFn callback;

        // The last time that this client has sent a message (or has
        // promised not to send a message).
        LockTime last_send_time;

        // The latest time the server has authorized the client to advance
        // to (and correspondingly, the time until which all incoming messages
        // have been delivered); the client's subjective time must this time
        // or earlier.
        LockTime authorized_time;

        // Message queue for each client.
        std::vector<MessageQueue> message_queue;
    };

    // The latest time at which we know no further messages can be sent
    // by any currently attached client.
    LockTime NoMoreSendsTime() {
      LockTime last_send_time = kStart;
      for (const ClientInfo& client : clients_) {
        last_send_time = std::max(last_send_time,
                                  client.last_send_time);
      }
      return last_send_time;
    }

    // The latest time at which we know the current receive queues to be
    // complete.
    LockTime NoMoreReceivesTime() {
      return NoMoreSendsTime() + delivery_latency_;
    }

    void AdvanceClientTimes() {
      // If any client is not authorized to the latest possible receive time,
      // do so.
      //
      // As the latest possible receive time is always at least
      // `delivery_latency_` later than the earliest latest send, it should
      // always be possible at least to authorize the recipient of that send.
      LockTime authorize_until = NoMoreReceivesTime();
      for (int i = 0; i < clients_.size(); i++) {
        ClientInfo* client = &clients_[i];
        MessageQueue to_deliver;
        while(true) {  // Drain off the deliverable prefix of message_queue.
          if (client->message_queue.size() == 0) break;
          auto first_msg = client->message_queue.begin();
          if (first_msg->receive_time > authorize_until) break;
          to_deliver.insert(first_msg);
          client->message_queue.erase(first_msg);
        }
        client->callback(authorize_until, to_deliver);
      }
    }

    // NOTE:  We may want to change this in the future; for instance,
    // TRI's driving simulator uses a post-2038 sim start time to clearly
    // distinguish sim and real logs and test for time bugs.
    static constexpr LockTime kStart = 0;

    // This can't be kConst because it is settable at ctor time.  It is
    // kept const because otherwise reasoning about when exactly it was
    // mutated would be quite subtle.
    const LockTime delivery_latency_;

    std::vector<ClientInfo> clients_;
};
