#pragma once

constexpr size_t MAX_HEADER_LEN = 4096;
constexpr size_t default_req_buffer_sz_minus1 = 4*4096-1;
class HTTPResponseHandler;
typedef std::nullptr_t NonHTTPRequestHandler;
typedef compsky::server::Server<MAX_HEADER_LEN, default_req_buffer_sz_minus1, HTTPResponseHandler, NonHTTPRequestHandler, 0, 1, 10> Server;

constexpr static const std::string_view not_found =
	HEADER__RETURN_CODE__NOT_FOUND
	HEADER__CONTENT_TYPE__TEXT
	HEADER__CONNECTION_KEEP_ALIVE
	HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
	"Content-Length: 9\r\n"
	"\r\n"
	"Not Found"
;
