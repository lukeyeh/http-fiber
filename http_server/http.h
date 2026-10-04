// The HTTP/1.1 messages the server understands: what a client asked for, and
// what to send back. Knowledge of the wire format lives here and nowhere else.

#ifndef HTTP_SERVER_HTTP_H_
#define HTTP_SERVER_HTTP_H_

#include <string>
#include <string_view>

#include "absl/status/statusor.h"

namespace http_server {

struct Request {
  std::string method;  // "GET", "POST", ...
  std::string target;  // What was asked for, e.g. "/index.html?x=1".
};

struct Response {
  int status = 200;
  std::string content_type = "text/plain";
  std::string body;
};

// The bytes that end the head of a request: its request line and headers.
inline constexpr std::string_view kEndOfHead = "\r\n\r\n";

// Reads the request line out of the head of a request. The headers are not
// interpreted. Fails with kInvalidArgument if the request line is not
// "<method> /<target> HTTP/1.x".
absl::StatusOr<Request> ParseRequest(std::string_view head);

// Returns the complete message to send for `response`. It tells the client
// that the connection will be closed afterwards.
std::string SerializeResponse(const Response& response);

}  // namespace http_server

#endif  // HTTP_SERVER_HTTP_H_
