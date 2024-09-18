#define CACHE_CONTROL_HEADER "Cache-Control: max-age=" MAX_CACHE_AGE "\n"

#define COMPSKY_SERVER_NOFILEWRITES
#define COMPSKY_SERVER_NOSENDMSG
#define COMPSKY_SERVER_NOSENDFILE
#include <compsky/server/server.hpp>
#include <compsky/server/static_response.hpp>
#include <compsky/utils/ptrdiff.hpp>
#include <compsky/os/read.hpp>
#include <compsky/mimetype/mimetype.hpp>
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
#include <chrono>
#include "sqlite-amalgamation-3450100/sqlite3.h"
#include "typedefs.hpp"

#define SECURITY_HEADERS \
	GENERAL_SECURITY_HEADERS \
	"Content-Security-Policy: default-src 'none'; frame-src 'none'; connect-src 'self'; script-src 'self'; img-src 'self' data:; media-src 'self'; style-src 'self'\r\n"


constexpr std::string_view known_headers(
	HEADER__RETURN_CODE__OK
	HEADER__CONTENT_TYPE__HTML
	HEADER__CONNECTION_KEEP_ALIVE
	SECURITY_HEADERS
	"Content-Length: "
);
char* init_knownheader_response(char* const buf){
	return buf + known_headers.size() + 19 + 4; // size of content-length value, and size of \r\n\r\n
}
std::string_view stringview_from_knownheader_response(char* const buf,  char* const end_of_content,  char* const beginning_of_content){
	const unsigned content_length = compsky::utils::ptrdiff(end_of_content,beginning_of_content);
	unsigned content_length_ndigits = 0;
	{
		unsigned n = content_length;
		do {
			++content_length_ndigits;
			n /= 10;
		} while(n != 0);
	}
	char* const beginning_of_response = buf + 19 - content_length_ndigits;
	{
		char* itr2 = beginning_of_response;
		compsky::asciify::asciify(itr2, known_headers, content_length, "\r\n\r\n");
	}
	
	return std::string_view(beginning_of_response, compsky::utils::ptrdiff(end_of_content,beginning_of_response));
}

unsigned index_of_char(const char c,  const char* const str,  const unsigned N){
	unsigned i = 0;
	for (;  i < N;  ++i){
		if (str[i] == c){
			break;
		}
	}
	return i;
}
unsigned last_index_of_char__excluding_first(const char c,  const char* const str,  const unsigned N){
	unsigned i = N;
	while (i != 0){
		--i;
		if (str[i] == c){
			break;
		}
	}
	return i;
}

void write_human_bytes(char*& itr,  int64_t n){
	unsigned unit_indx = 0;
	while (n > 4098){
		++unit_indx;
		n /= 1024;
	}
	compsky::asciify::asciify(itr, n);
	if (unit_indx != 0){
		compsky::asciify::asciify(itr, (unit_indx==1) ? 'K' : ((unit_indx==2) ? 'M' : ((unit_indx==3) ? 'G' : ((unit_indx==4) ? 'T' : ((unit_indx==5) ? 'P' : '?')))), 'i');
	}
	compsky::asciify::asciify(itr, 'B');
}

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
unsigned server_buf_sz;

unsigned compress(char* const compressed_data_buf,  const void* const uncompressed_data,  const int uncompressed_size){
	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	stream.avail_in = uncompressed_size;
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(uncompressed_data));
	stream.avail_out = max_decompressed_size;
	stream.next_out = reinterpret_cast<unsigned char*>(compressed_data_buf);
	
	if (deflateInit2(&stream,  9,  Z_DEFLATED,  15 + 16,  8,  Z_DEFAULT_STRATEGY) != Z_OK){
		write(2, "Failed to initialize zlib\n", 26);
		return 0;
	}
	
	const int result = deflate(&stream, Z_FINISH);
	deflateEnd(&stream);
	
	return stream.total_out;
}

unsigned decompress(char* const decompressed_data_buf,  const void* const compressed_data,  const int compressed_size,  const int _max_decompressed_size){
	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	stream.avail_in = compressed_size;
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(compressed_data));
	stream.avail_out = _max_decompressed_size;
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
char* second_buf;

sqlite3* db;
sqlite3_stmt* stmt;
sqlite3_stmt* stmt2 = nullptr;
#ifndef COMPSKY_NOSTATS
sqlite3_stmt* stmt_stats_CountPerDomain;
sqlite3_stmt* stmt_stats_LargestFiles;
#endif

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
		constexpr const char prefix5[8] = {'G','E','T',' ','/','s','t','y'};
		constexpr const char prefix6[8] = {'G','E','T',' ','/','s','t','a'};
		constexpr const char prefix2[8] = {'P','O','S','T',' ','/','c','a'};
		constexpr const char prefix3[8] = {'h','e','i','n','s','e','r','t'};
		constexpr const char prefix4[8] = {'h','e','i','n','s','.','j','s'};
		if (reinterpret_cast<uint64_t*>(str)[0] == uint64_value_of(prefix0)){
			[[likely]];
			constexpr const char prefix1[8] = {'h','e','d','/','h','t','t','p'};
			if (reinterpret_cast<uint64_t*>(str)[1] == uint64_value_of(prefix1)){
				const char* domain = str + 16;
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
					const char* domain_end = domain;
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
				
				const char* const path = domain + domain_length;
				unsigned path_length;
				{
					const char* path_end = path;
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
					compsky::mimetyp::MimeTyp mimetype_id = compsky::mimetyp::Unknown;
					if ((header__content_typ == nullptr) and (compressed_size >= compsky::mimetyp::n_bytes)){
						mimetype_id = compsky::mimetyp::guess_mimetype(reinterpret_cast<const char*>(compressed_data));
						if (mimetype_id == compsky::mimetyp::GZIP){
							[[likely]];
							char buf[compsky::mimetyp::n_bytes];
							decompress(buf, compressed_data, compressed_size, compsky::mimetyp::n_bytes);
							mimetype_id = compsky::mimetyp::guess_mimetype(buf);
						}
						const char* const mimetype = compsky::mimetyp::mimetype2str(mimetype_id);
						if (mimetype != nullptr){
							[[likely]];
							content_typ = std::string_view(mimetype,strlen(mimetype));
						}
					}
					
					char* server_itr = server_buf;
					{
						compsky::asciify::asciify(server_itr,
							"HTTP/1.1 200 OK",
							content_encoding_prefix, content_encoding, "\r\n"
							"Content-Type: ", content_typ, "\r\n"
							"Cache-Control: max-age=64800\r\n"
							"Connection: keep-alive\r\n"
							SECURITY_HEADERS
							"Content-Length: ", compressed_size, "\r\n"
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
					
					char* const beginning_of_content = init_knownheader_response(server_buf);
					char* itr = beginning_of_content;
					
					constexpr std::string_view htmlbegin("<!DOCTYPE html><html><body><h1>Not Found</h1><h2>Possible alternatives</h2><b>These may or may not be cached</b><ul>");
					compsky::asciify::asciify(itr, htmlbegin);
					
					{
						const unsigned paramindx = index_of_char('?', path, path_length);
						if (paramindx != path_length){
							compsky::asciify::asciify(itr, "<li><a href=\"/cached/",std::string_view(domain,domain_length),std::string_view(path,paramindx),"\">Without URL parameterse</a></li>");
						}
					}
					
					if (domain_length == 14){
						if (
							(domain[3] == '.') and
							(domain[4] == 'r') and
							(domain[5] == 'e') and
							(domain[6] == 'd') and
							(domain[7] == 'd') and
							(domain[8] == 'i') and
							(domain[9] == 't') and
							(domain[10]== '.') and
							(domain[11]== 'c') and
							(domain[12]== 'o') and
							(domain[13]== 'm')
						){
							// *.reddit.com
							
							if (!(
								(domain[0] == 'w') and
								(domain[1] == 'w') and
								(domain[2] == 'w')
							)){
								compsky::asciify::asciify(itr, "<li><a href=\"/cached/https://www.reddit.com",std::string_view(path,path_length),"\">On www.reddit.com</a></li>");
							} else if (!(
								(domain[0] == 'o') and
								(domain[1] == 'l') and
								(domain[2] == 'd')
							)){
								compsky::asciify::asciify(itr, "<li><a href=\"/cached/https://old.reddit.com",std::string_view(path,path_length),"\">On old.reddit.com</a></li>");
							} else if (!(
								(domain[0] == 'n') and
								(domain[1] == 'e') and
								(domain[2] == 'w')
							)){
								compsky::asciify::asciify(itr, "<li><a href=\"/cached/https://new.reddit.com",std::string_view(path,path_length),"\">On new.reddit.com</a></li>");
							}
							
							{
								const unsigned second_last_slash = last_index_of_char__excluding_first('/',path,path_length-1);
								if (second_last_slash != 0){
									compsky::asciify::asciify(itr, "<li><a href=\"/cached/",std::string_view(domain,domain_length),std::string_view(path,second_last_slash+1),"\">Without last subpath</a></li>");
								}
							}
						}
					}
					if (itr != beginning_of_content + htmlbegin.size()){
						compsky::asciify::asciify(itr, "</ul></body></html>");
						return stringview_from_knownheader_response(server_buf, itr, beginning_of_content);
					}
					
				}
			} else if (reinterpret_cast<uint64_t*>(str)[1] == uint64_value_of(prefix4)){
				return
					HEADER__RETURN_CODE__OK
					HEADER__CONTENT_TYPE__JS
					HEADER__CONNECTION_KEEP_ALIVE
					HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
					"Content-Length: 5226\r\n" // NOTE: If calculating length in Python, must add 4 bytes due to newlines being escaped
					"\r\n"
					#include "cacheinsert.js"
				;
			} else if (reinterpret_cast<uint64_t*>(str)[1] == uint64_value_of(prefix3)){
				return
					HEADER__RETURN_CODE__OK
					HEADER__CONTENT_TYPE__HTML
					HEADER__CONNECTION_KEEP_ALIVE
					SECURITY_HEADERS
					"Content-Length: 1760\r\n"
					"\r\n"
					#include "cacheinsert.html"
				;
			} else {
				[[unlikely]];
				printf("Bad request, not GET /cached/http: %.16s\n", str);
			}
		} else if ((reinterpret_cast<uint64_t*>(str)[0] == uint64_value_of(prefix2)) and (stmt2 != nullptr)){
			// Format: [URL_DOMAIN]\n[URL_PATH]\n[FILE_CONTENT_PATH]\n[MIMETYPE]\n[FILE_CONTENT]
			const char* const body_end = body_content_start + body_len;
			
			// Sanity check
			if (body_len == 0)
				return not_found;
			if (not ((body_content_start[0] >= 'a') && (body_content_start[0] <= 'z')))
				return not_found;
			
			const char* const domain = body_content_start;
			const char* domain_end;
			const char* path_end;
			const char* filepath_end;
			const char* mimetype_end = nullptr;
			{
				const char* itr = domain;
				while(itr != body_end){
					if (*itr == '\n'){
						domain_end = itr;
						++itr;
						break;
					}
					++itr;
				}
				
				while(itr != body_end){
					if (*itr == '\n'){
						path_end = itr;
						++itr;
						break;
					}
					++itr;
				}
				
				while(itr != body_end){
					if (*itr == '\n'){
						filepath_end = itr;
						++itr;
						break;
					}
					++itr;
				}
				
				while(itr != body_end){
					if (*itr == '\n'){
						mimetype_end = itr;
						++itr;
						break;
					}
					++itr;
				}
			}
			if (mimetype_end != nullptr){
				[[likely]];
				const char* const path = domain_end + 1;
				const char* const filepath = path_end + 1;
				const char* mimetype = filepath_end + 1;
				const char* contents = mimetype_end + 1;
				const char* contents_end = body_end;
				
				if (filepath_end != filepath){
					if (body_end != contents){
						[[unlikely]];
						printf("Bad POST request: You provided filepath AND contents, but only one of filepath OR contents should be provided.\n");
						return not_found;
					}
					if (filepath_end > filepath + 1023){
						[[unlikely]];
						printf("Bad POST request: Filepath too long.\n");
						return not_found;
					}
					char filepath_buf[1024];
					{
						char* itr = filepath_buf;
						compsky::asciify::asciify(itr, std::string_view(filepath,compsky::utils::ptrdiff(filepath_end,filepath)), '\0');
					}
					
					const compsky::os::ReadOnlyFile f(const_cast<const char*>(filepath_buf));
					if (f.size() == 0){
						[[unlikely]];
						printf("Bad POST request: File has size 0 or does not exist: %s\n", filepath_buf);
						return not_found;
					}
					/*const int fd = open(filepath_buf, O_RDONLY);
					if (fd == -1){
						[[unlikely]];
						return not_found; // TODO: "Error: Filepath does not exist."
					}
					read(fd, server_buf, f_sz);
					close(fd);*/
					if (f.size() > server_buf_sz){
						[[unlikely]];
						printf("Bad POST request: File too large.\n");
						return not_found;
					}
					if (f.read_entirety_into_buf(server_buf)){
						[[unlikely]];
						printf("Bad POST request: Couldn't read all content from file: %s\n", strerror(errno));
						return not_found;
					}
					contents = server_buf;
					contents_end = contents + f.size();
					printf("Read %luKiB\n", compsky::utils::ptrdiff(contents_end,contents)/1024u);
					fflush(stdout);
				}
				
				if (mimetype_end == mimetype){
					if (contents_end < contents + compsky::mimetyp::n_bytes){
						[[unlikely]];
						printf("Bad POST request: Mimetype not provided, and contents too small to guess it.\n");
						return not_found;
					}
					mimetype = compsky::mimetyp::mimetype2str(compsky::mimetyp::guess_mimetype(contents));
					if (mimetype == nullptr){
						[[unlikely]];
						printf("Bad POST request: Mimetype couldn't be guessed from: %.*s\n", (int)compsky::mimetyp::n_bytes, contents);
						return not_found;
					}
					mimetype_end = mimetype + strlen(mimetype);
					printf("Guessed mimetype %s\n", mimetype);
					fflush(stdout);
				}
				
				const unsigned uncompressed_content_length = compsky::utils::ptrdiff(contents_end,contents);
				unsigned content_length = uncompressed_content_length;
				
				if (content_length == 0){
					[[unlikely]];
					printf("Bad POST request: Content has length 0\n");
					return not_found;
				}
				
				const char* content_encoding_stuff_or_empty = "";
				{
					const unsigned compressed_content_sz = compress(second_buf, contents, content_length);
					if (compressed_content_sz + 1000u < content_length){
						printf("Storing compressed version, %u i.e. %uKiB vs %uKiB\n", compressed_content_sz, compressed_content_sz/1024, content_length/1024);
						contents = second_buf;
						content_encoding_stuff_or_empty = ", \"content-encoding\": \"gzip\"";
						content_length = compressed_content_sz;
					} else {
						printf("Storing uncompressed version, %u i.e. %uKiB vs %uKiB\n%.*s\n", content_length, content_length/1024, compressed_content_sz/1024, (int)content_length, contents);
					}
				}
				
				char headers[1024];
				unsigned headers_len;
				{
					char* itr = headers;
					compsky::asciify::asciify(itr, "{\"content-type\": \"", std::string_view(mimetype,compsky::utils::ptrdiff(mimetype_end,mimetype)), "\", \"content-length\": \"", uncompressed_content_length, content_encoding_stuff_or_empty, "\"}");
					headers_len = compsky::utils::ptrdiff(itr, headers);
				}
				
				sqlite3_bind_text(stmt2, 1, domain,   compsky::utils::ptrdiff(domain_end,domain),     SQLITE_STATIC);
				sqlite3_bind_text(stmt2, 2, path,     compsky::utils::ptrdiff(path_end,path),         SQLITE_STATIC);
				sqlite3_bind_text(stmt2, 3, contents, content_length,                                 SQLITE_STATIC);
				sqlite3_bind_text(stmt2, 4, headers,  headers_len,                                    SQLITE_STATIC);
				sqlite3_bind_int64(stmt2, 5, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
				
				if (sqlite3_step(stmt2) != SQLITE_DONE){
					[[unlikely]];
					sqlite3_reset(stmt2);
					fprintf(stderr, "Error: Couldn't insert into database: %s\n", sqlite3_errmsg(db));
					return not_found;
				}
				sqlite3_reset(stmt2);
				
				return
					HEADER__RETURN_CODE__OK
					HEADER__CONTENT_TYPE__TEXT
					HEADER__CONNECTION_KEEP_ALIVE
					HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
					"Content-Length: 2\r\n"
					"\r\n"
					"OK"
				;
			}
		} else if (reinterpret_cast<uint64_t*>(str)[0] == uint64_value_of(prefix5)){
			return
				HEADER__RETURN_CODE__OK
				HEADER__CONTENT_TYPE__CSS
				HEADER__CONNECTION_KEEP_ALIVE
				HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
				"Content-Length: 105\r\n"
				"\r\n"
				#include "style.css"
			;
#ifndef COMPSKY_NOSTATS
		} else if (reinterpret_cast<uint64_t*>(str)[0] == uint64_value_of(prefix6)){
			char* const beginning_of_content = init_knownheader_response(server_buf);
			char* itr = beginning_of_content;
			
			compsky::asciify::asciify(itr, "<!DOCTYPE html><html><body>");
			
			constexpr const char prefix7[8] = {'t','s','/','s','i','t','e','s'};
			constexpr const char prefix8[8] = {'t','s','/','b','i','g','f','i'};
			if (reinterpret_cast<uint64_t*>(str)[1] == uint64_value_of(prefix7)){
				compsky::asciify::asciify(itr, "<h2>All domains</h2><table><tr><th>Count</th><th>Total size</th><th>Domain</th></tr>");
				while (sqlite3_step(stmt_stats_CountPerDomain) != SQLITE_DONE){
					const int64_t count = sqlite3_column_int64(stmt_stats_CountPerDomain, 0);
					
					const int64_t total_sz = sqlite3_column_int64(stmt_stats_CountPerDomain, 1);
					
					const char* const domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt_stats_CountPerDomain, 2));
					const int domain_sz = sqlite3_column_bytes(stmt_stats_CountPerDomain, 2);
					
					compsky::asciify::asciify(itr, "<tr><td>", count, "</td><td>");
					write_human_bytes(itr, total_sz);
					compsky::asciify::asciify(itr, "</td><td>", std::string_view(domain,domain_sz), "</td></tr>");
				}
				compsky::asciify::asciify(itr, "</table>");
				sqlite3_reset(stmt_stats_CountPerDomain);
			} else if (reinterpret_cast<uint64_t*>(str)[1] == uint64_value_of(prefix8)){
				compsky::asciify::asciify(itr, "<h2>Largest files</h2><table><tr><th>Size</th><th>Domain</th></tr>");
				while (sqlite3_step(stmt_stats_LargestFiles) != SQLITE_DONE){
					const int64_t file_sz = sqlite3_column_int64(stmt_stats_LargestFiles, 0);
					
					const char* const domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt_stats_LargestFiles, 1));
					const int domain_sz = sqlite3_column_bytes(stmt_stats_LargestFiles, 1);
					
					const char* const path = reinterpret_cast<const char*>(sqlite3_column_text(stmt_stats_LargestFiles, 2));
					const int path_sz = sqlite3_column_bytes(stmt_stats_LargestFiles, 2);
					
					compsky::asciify::asciify(itr, "<tr><td>");
					write_human_bytes(itr, file_sz);
					compsky::asciify::asciify(itr, "</td><td><a href=\"/cached/https://", std::string_view(domain,domain_sz), std::string_view(path,path_sz), "\">",std::string_view(domain,domain_sz),"</a></td></tr>");
				}
				compsky::asciify::asciify(itr, "</table>");
				sqlite3_reset(stmt_stats_LargestFiles);
			} else {
				compsky::asciify::asciify(itr,
					"<h2>Stats</h2>"
					"<a href=\"/stats/bigfiles\">Biggest files</a><br>"
					"<a href=\"/stats/sites\">All cached sites</a>"
				);
			}
			compsky::asciify::asciify(itr, "</body></html>");
			
			return stringview_from_knownheader_response(server_buf, itr, beginning_of_content);
#endif
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
	server_buf_sz = 1024*1024 * a2n<unsigned>(argv[4]);
	max_decompressed_size = server_buf_sz;
	
	if (argc != 6){
		error_and_exit:
		[[unlikely]]
		write(2, "USAGE: [/path/to/sqlite.db] [PORT] [MAX_REQUEST_SIZE_IN_KB] [MAX_CONTENT_SIZE_IN_MB] ['ro' for READONLY_MODE, 'rw' for default READ+WRITE]\n", 135);
		return 1;
	}
	
	if (argv[5][0] != 'r'){
		[[unlikely]]
		goto error_and_exit;
	}
	if (argv[5][1] == 'o'){
		sqlite_mode = SQLITE_OPEN_READONLY;
	} else if (argv[5][1] == 'w'){
	} else {
		[[unlikely]]
		goto error_and_exit;
	}
	
	// decompressed_data_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	
	server_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	second_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	if (server_buf == nullptr){
		[[unlikely]]
		return 1;
	}
	
	
	
	if (sqlite3_open_v2(db_path, &db, sqlite_mode, nullptr) != SQLITE_OK){
		fprintf(stderr, "Database does not exist at: %s\n", sqlite3_errmsg(db));
		
		if (sqlite_mode == SQLITE_OPEN_READWRITE){
			{
				const int fd = open(db_path, O_CREAT, S_IRUSR|S_IWUSR);
				if (fd == -1){
					[[unlikely]];
					fprintf(stderr, "Could not create database at: %s\n", db_path);
					return 1;
				}
				close(fd);
			}
			if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK){
				fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
				return 1;
			}
			
			fprintf(stderr, "Creating database at: %s\n", db_path);
			sqlite3_stmt* creation_stmt;
			if (sqlite3_prepare_v2(db, "CREATE TABLE file (domain STRING NOT NULL, path STRING NOT NULL, content BLOB NOT NULL, headers STRING NOT NULL, t_added INTEGER UNSIGNED NOT NULL DEFAULT 0, hits INTEGER UNSIGNED NOT NULL DEFAULT 0, UNIQUE (domain,path))", -1, &creation_stmt, NULL) != SQLITE_OK){
				[[unlikely]];
				fprintf(stderr, "Failed to prepare SQL query: %s\n", sqlite3_errmsg(db));
				sqlite3_close(db);
				return 1;
			}
			if (sqlite3_step(creation_stmt) != SQLITE_DONE){
				[[unlikely]];
				write(2, "Failed to create table\n", 23);
				return 1;
			}
			sqlite3_finalize(creation_stmt);
		} else {
			return 1;
		}
	}
	if (sqlite3_prepare_v2(db, "SELECT content, headers FROM file WHERE domain=? AND path=? LIMIT 1", -1, &stmt, NULL) != SQLITE_OK){
		[[unlikely]];
		fprintf(stderr, "Failed to prepare SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*), SUM(octet_length(content)) AS sz, domain FROM file GROUP BY domain ORDER BY sz DESC", -1, &stmt_stats_CountPerDomain, NULL) != SQLITE_OK){
		[[unlikely]];
		fprintf(stderr, "Failed to prepare SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	if (sqlite3_prepare_v2(db, "SELECT octet_length(content) AS sz, domain, path FROM file ORDER BY sz DESC LIMIT 100", -1, &stmt_stats_LargestFiles, NULL) != SQLITE_OK){
		[[unlikely]];
		fprintf(stderr, "Failed to prepare SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	if (sqlite_mode == SQLITE_OPEN_READWRITE){
		if (sqlite3_prepare_v2(db, "INSERT INTO file (domain, path, content, headers, t_added, hits) VALUES (?,?,?,?,?,0) ON CONFLICT(domain,path) DO UPDATE SET content=excluded.content, headers=excluded.headers", -1, &stmt2, NULL) != SQLITE_OK){
			[[unlikely]];
			fprintf(stderr, "Failed to prepare SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return 1;
		}
	}
	
	
	signal(SIGPIPE, SIG_IGN); // see https://stackoverflow.com/questions/5730975/difference-in-handling-of-signals-in-unix for why use this vs sigprocmask - seems like sigprocmask just causes a queue of signals to build up  (but send could have MSG_NOSIGNAL flag)
	Server::max_req_buffer_sz_minus_1 = a2n<unsigned>(argv[3]) * 1024; // NOTE: Size is arbitrary
	Server::run(port_n, INADDR_LOOPBACK, server_buf, all_client_contexts, EWOULDBLOCK_queue);
	
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmt2);
	sqlite3_close(db);
	
	return 0;
}