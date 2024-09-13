#define CACHE_CONTROL_HEADER "Cache-Control: max-age=" MAX_CACHE_AGE "\n"

#define COMPSKY_SERVER_NOFILEWRITES
#define COMPSKY_SERVER_NOSENDMSG
#define COMPSKY_SERVER_NOSENDFILE
#include <compsky/server/server.hpp>
#include <compsky/server/static_response.hpp>
#include <compsky/utils/ptrdiff.hpp>
#define MYSQL_ROW std::nullptr_t
#include <compsky/deasciify/a2n.hpp>
#include <signal.h>
#include <bit> // for std::bit_cast
#include <string_view>
#include <zlib.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "sqlite-amalgamation-3450100/sqlite3.h"
#include "typedefs.hpp"

#define SECURITY_HEADERS \
	GENERAL_SECURITY_HEADERS \
	"Content-Security-Policy: default-src 'none'; frame-src 'none'; connect-src 'self'; script-src 'self'; img-src 'self' data:; media-src 'self'; style-src 'self'\r\n"


const char* find_substr(const char* container,  const int container_sz,  const char* const substr,  const unsigned substr_len){
	const char* const container_end = container + container_sz;
	const char* substr_stateful = substr;
	
	while(container != container_end){
		if (*container == *substr_stateful){
			++substr_stateful;
			if (substr_stateful == substr + substr_len){
				++container;
				break;
			}
		} else {
			substr_stateful = substr;
		}
		++container;
	}
	
	if (container == container_end)
		container = nullptr;
	
	return container;
}

const char* ffd894237sfdfsd(const char* str){
	while(*str != 0){
		++str;
		if (*str == '"'){
			return str;
		}
	}
	return nullptr;
}

int max_decompressed_size;

unsigned decompress(char* const decompressed_data_buf,  const void* const compressed_data,  const int compressed_size){
	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	stream.avail_in = compressed_size;
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(compressed_data));
	stream.avail_out = max_decompressed_size;
	stream.next_out = reinterpret_cast<unsigned char*>(decompressed_data_buf);
	
	if (inflateInit2(&stream, 15 + 16) != Z_OK){
		write(2, "Failed to initialize zlib\n", 26);
		return 0;
	}
	
	const int result = inflate(&stream, Z_FINISH);
	inflateEnd(&stream);
	
	if (result != Z_STREAM_END) {
		write(2, "Failed to decompress data with zlib\n", 36);
		return 0;
	}
	
	return stream.total_out;
}

std::vector<Server::ClientContext> all_client_contexts;
std::vector<Server::EWOULDBLOCK_queue_item> EWOULDBLOCK_queue;

char* server_buf;
sqlite3_stmt* stmt;

constexpr
uint32_t uint32_value_of(const char(&s)[4]){
	return std::bit_cast<std::uint32_t>(s);
}

constexpr
uint64_t uint64_value_of(const char(&s)[8]){
	return std::bit_cast<std::uint64_t>(s);
}

class HTTPResponseHandler {
 public:
	const bool keep_alive;
	
	HTTPResponseHandler(const std::int64_t _thread_id,  char* const _buf)
	: keep_alive(true)
	{}
	
	char remote_addr_str[INET6_ADDRSTRLEN];
	unsigned remote_addr_str_len;
	
	std::string_view handle_request(
		Server::ClientContext* client_context,
		std::vector<Server::ClientContext>& client_contexts,
		std::vector<Server::LocalListenerContext>& local_listener_contexts,
		char* str,
		const char* const body_content_start,
		const std::size_t body_len,
		std::vector<char*>& headers
	){
		// NOTE: str guaranteed to be at least default_req_buffer_sz_minus1
		constexpr const char prefix0[8] = {'G','E','T',' ','/','c','a','c'};
		if (reinterpret_cast<uint64_t*>(str)[0] == uint64_value_of(prefix0)){
			[[likely]];
			constexpr const char prefix1[8] = {'h','e','d','/','h','t','t','p'};
			if (reinterpret_cast<uint64_t*>(str)[1] == uint64_value_of(prefix1)){
				char* domain = str + 16;
				if ((domain[0] == ':') and (domain[1] == '/') and (domain[2] == '/')){
					domain += 3;
				} else if ((domain[0] == 's') and (domain[1] == ':') and (domain[2] == '/') and (domain[3] == '/')){
					domain += 4;
				} else {
					[[unlikely]];
					printf("Bad request, http(s): %.30s\n", str);
					return not_found;
				}
				
				unsigned domain_length;
				{
					char* domain_end = domain;
					while(*domain_end != 0){
						if (*domain_end == '/'){
							break;
						}
						++domain_end;
					}
					if (*domain_end == 0){
						[[unlikely]];
						printf("Bad request, domain doesn't end: %.30s\n", str);
						return not_found;
					}
					domain_length = compsky::utils::ptrdiff(domain_end,domain);
				}
				
				char* const path = domain + domain_length;
				unsigned path_length;
				{
					char* path_end = path;
					while(*path_end != 0){
						if (*path_end == ' '){
							break;
						}
						++path_end;
					}
					if (*path_end == 0){
						[[unlikely]];
						printf("Bad request, path doesn't end: %.30s\n", str);
						return not_found;
					}
					path_length = compsky::utils::ptrdiff(path_end,path);
				}
				sqlite3_bind_text(stmt, 1, domain, domain_length, SQLITE_STATIC);
				sqlite3_bind_text(stmt, 2, path,   path_length,   SQLITE_STATIC);
				if (sqlite3_step(stmt) == SQLITE_ROW){
					[[likely]];
					const void* const compressed_data = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0));
					const int compressed_size = sqlite3_column_bytes(stmt, 0);
					
					// const unsigned decompressed_sz = decompress(decompressed_data_buf, compressed_data, compressed_size);
					
					const char* const header_data = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1));
					const int header_size = sqlite3_column_bytes(stmt, 1);
					
					const char* const header__content_encoding = find_substr(header_data, header_size, "\"content-encoding\": \"", 21);
					const char* content_encoding_prefix = "";
					std::string_view content_encoding(nullptr,0);
					if (header__content_encoding != nullptr){
						content_encoding_prefix = "\r\nContent-Encoding: ";
						
						const char* const header__content_encoding__end = ffd894237sfdfsd(header__content_encoding);
						
						content_encoding = std::string_view(header__content_encoding, compsky::utils::ptrdiff(header__content_encoding__end,header__content_encoding));
					}
					
					const char* const header__content_typ = find_substr(header_data, header_size, "\"content-type\": \"", 25);
					std::string_view content_typ("text/html; charset=utf-8", 24);
					if (header__content_typ != nullptr){
						[[likely]];
						
						const char* const header__content_typ__end = ffd894237sfdfsd(header__content_typ);
						
						content_typ = std::string_view(header__content_typ, compsky::utils::ptrdiff(header__content_typ__end,header__content_typ));
					}
					
					const char* const header__content_length = find_substr(header_data, header_size, "\"content-length\": \"", 27);
					std::string_view content_length(nullptr,0);
					char* server_itr = server_buf;
					{
						char content_length_int_str[10+1];
						if (header__content_length != nullptr){
							[[likely]];
							
							const char* const header__content_length__end = ffd894237sfdfsd(header__content_length);
							
							content_length = std::string_view(header__content_length, compsky::utils::ptrdiff(header__content_length__end,header__content_length));
						} else {
							printf("WARNING: content-length header not present for %.*s %.*s\n", (int)domain_length, domain, (int)path_length, path);
							
							uint32_t content_length_int = compressed_size;
							if (content_encoding.size() != 0){
								uint32_t content_length_int = decompress(server_buf, compressed_data, compressed_size);
							}
							
							char* itr = content_length_int_str;
							unsigned n_digits = 0;
							do {
								*(itr++) = '0' + (content_length_int % 10);
								content_length_int /= 10;
								++n_digits;
							} while(content_length_int != 0);
							
							content_length = std::string_view(content_length_int_str, n_digits);
						}
						
						compsky::asciify::asciify(server_itr,
							"HTTP/1.1 200 OK",
							content_encoding_prefix, content_encoding, "\r\n"
							"Content-Type: ", content_typ, "\r\n"
							"Cache-Control: max-age=64800\r\n"
							"Connection: keep-alive\r\n"
							"Content-Length: ", content_length, "\r\n"
							"\r\n",
							
							std::string_view(reinterpret_cast<const char*>(compressed_data), compressed_size)
							// std::string_view(reinterpret_cast<const char*>(decompressed_data_buf), decompressed_sz)
						);
					}
					sqlite3_reset(stmt);
					
					return std::string_view(server_buf, compsky::utils::ptrdiff(server_itr,server_buf));
				} else {
					sqlite3_reset(stmt);
					printf("No results for %.*s %.*s\n", (int)domain_length, domain, (int)path_length, path);
				}
			} else {
				printf("Bad request, not GET /cached/http: %.16s\n", str);
			}
		} else {
			printf("Bad request, not GET /cached/: %.8s\n%lu vs %lu\n", str, reinterpret_cast<uint64_t*>(str)[0], uint64_value_of(prefix0));
		}
		
		return not_found;
	}
	
	bool handle_timer_event(){
		unsigned n_nonempty = 0;
		uint64_t total_sz = 0;
		for (unsigned i = 0;  i < EWOULDBLOCK_queue.size();  ++i){
			if (EWOULDBLOCK_queue[i].client_socket != 0){
				++n_nonempty;
				total_sz += EWOULDBLOCK_queue[i].queued_msg_length;
			}
		}
		if (n_nonempty != 0)
			printf("EWOULDBLOCK_queue[%lu] has %u non-empty entries (%luKiB)\n", EWOULDBLOCK_queue.size(), n_nonempty, total_sz/1024);
		
		for (unsigned i = 0;  i < all_client_contexts.size();  ++i){
			if (all_client_contexts[i].client_id != 0){
				printf("client %i\n", all_client_contexts[i].client_socket);
			}
		}
		
		// TODO: Tidy vectors such as EWOULDBLOCK_queue by removing items where client_socket==0
		return false;
	}
	bool is_acceptable_remote_ip_addr(){
		return true;
	}
};


int main(const int argc,  const char* const* const argv){
	int32_t sqlite_mode = SQLITE_OPEN_READWRITE;
	const char* const db_path = argv[1];
	const unsigned port_n = a2n<unsigned>(argv[2]);
	const unsigned server_buf_sz = 1024*1024 * a2n<unsigned>(argv[3]);
	max_decompressed_size = server_buf_sz;
	
	if (argc != 5){
		error_and_exit:
		[[unlikely]]
		write(2, "USAGE: [/path/to/sqlite.db] [PORT] [MAX_CONTENT_SIZE_IN_MB] ['ro' for READONLY_MODE, 'rw' for default READ+WRITE]\n", 114);
		return 1;
	}
	
	if (argv[4][0] != 'r'){
		[[unlikely]]
		goto error_and_exit;
	}
	if (argv[4][1] == 'o'){
		sqlite_mode = SQLITE_OPEN_READONLY;
	} else if (argv[4][1] == 'w'){
	} else {
		[[unlikely]]
		goto error_and_exit;
	}
	
	// decompressed_data_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	
	server_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	if (server_buf == nullptr){
		[[unlikely]]
		return 1;
	}
	
	
	sqlite3* db;
	
	if (sqlite3_open_v2(db_path, &db, sqlite_mode, nullptr) != SQLITE_OK){
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		return 1;
	}
	if (sqlite3_prepare_v2(db, "SELECT content, headers FROM file WHERE domain=? AND path=? LIMIT 1", -1, &stmt, NULL) != SQLITE_OK){
		fprintf(stderr, "Failed to prepare SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	
	
	signal(SIGPIPE, SIG_IGN); // see https://stackoverflow.com/questions/5730975/difference-in-handling-of-signals-in-unix for why use this vs sigprocmask - seems like sigprocmask just causes a queue of signals to build up  (but send could have MSG_NOSIGNAL flag)
	Server::max_req_buffer_sz_minus_1 = 500*1024; // NOTE: Size is arbitrary
	Server::run(port_n, INADDR_LOOPBACK, server_buf, all_client_contexts, EWOULDBLOCK_queue);
	
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	
	return 0;
}