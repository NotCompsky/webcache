default: sqlite-amalgamation-3450100/sqlite3.o webcache-server

sqlite-amalgamation-3450100/sqlite3.o:
	clang-15 sqlite-amalgamation-3450100/sqlite3.c -O3 -march=native -o sqlite-amalgamation-3450100/sqlite3.o

webcache-server:
	clang++-15 -std=c++2a -Ilibcompsky/include/ server.cpp sqlite-amalgamation-3450100/sqlite3.o -O3 -march=native -o webcache-server -lz