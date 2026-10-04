// A small HTTP/1.1 server built on Gloop fibers.
//
// One fiber accepts connections and starts a child fiber for each. A
// connection's fiber reads one request, calls the handler, writes the
// response and closes the connection. Code in a fiber is written as plain
// blocking code; a fiber that is waiting for its client costs the others
// nothing.

#ifndef HTTP_SERVER_SERVER_H_
#define HTTP_SERVER_SERVER_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "absl/status/statusor.h"
#include "gloop/thread/fiber/fiber.h"
#include "http_server/http.h"
#include "os/socket.h"

namespace http_server {

// Turns a request into the response to send. May be called from several
// fibers at once, so it must be safe to call concurrently.
using Handler = std::function<Response(const Request&)>;

class Server {
 public:
  // Starts serving requests made from this machine to `port`, or to a free
  // port chosen by the kernel if `port` is 0. Requests are being served by
  // the time this returns. Fails with kFailedPrecondition if the port is taken.
  static absl::StatusOr<std::unique_ptr<Server>> Start(uint16_t port,
                                                       Handler handler);

  // Stops accepting connections and waits for the requests already being
  // served to finish. A client that has connected but sent no request holds
  // this up for at most a few seconds.
  ~Server();

  // The port being served; never 0.
  uint16_t port() const { return listener_.port(); }

 private:
  Server(os::Listener listener, Handler handler);

  // The body of the accepting fiber. Returns once the fiber is cancelled and
  // every connection's fiber has finished.
  void AcceptConnections() const;

  // The body of a connection's fiber.
  void ServeConnection(os::Socket socket) const;

  const os::Listener listener_;
  const Handler handler_;
  // Last, so that it starts running only once the members above exist.
  thread::Fiber accept_fiber_;
};

}  // namespace http_server

#endif  // HTTP_SERVER_SERVER_H_
