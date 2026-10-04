#include "http_server/http.h"

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

namespace http_server {
namespace {

std::string_view ReasonPhrase(const int status) {
  switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    default:
      // Clients go by the number; the phrase is only for people.
      return "Status";
  }
}

}  // namespace

absl::StatusOr<Request> ParseRequest(const std::string_view head) {
  const std::string_view request_line = head.substr(0, head.find("\r\n"));
  const std::vector<std::string_view> parts = absl::StrSplit(request_line, ' ');
  if (parts.size() != 3 || parts[0].empty() || !parts[1].starts_with('/') ||
      !parts[2].starts_with("HTTP/1.")) {
    return absl::InvalidArgumentError(
        absl::StrCat("malformed request line: ", request_line));
  }
  return Request{
      .method = std::string(parts[0]),
      .target = std::string(parts[1]),
  };
}

std::string SerializeResponse(const Response& response) {
  return absl::StrCat("HTTP/1.1 ", response.status, " ",
                      ReasonPhrase(response.status), "\r\n",
                      "Content-Type: ", response.content_type, "\r\n",
                      "Content-Length: ", response.body.size(), "\r\n",
                      "Connection: close", kEndOfHead, response.body);
}

}  // namespace http_server
