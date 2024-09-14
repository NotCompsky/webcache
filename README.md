# Description

Web server that serves files from a SQLite database.

Features:

* supports gzip compression
* supports up to 3000 requests per second
  * Although SQLite lookup speeds probably dramatically limit this in realistic scenarios
* ability to write to the database with a `POST` request
  * You are able to use a script to, for any web page you visit, cache the page by clicking a button

# Compiling

Prerequisites:

* libz.h
  * Installed with `libz-dev` or `zlib1g-dev`
* [sqlite3.c](https://www.sqlite.org/download.html)
  * Or you can modify the compile instructions to use `libsqlite0-dev`
* Any modern C++ compiler
* [libycompsky](https://github.com/NotCompsky/libcompsky)

# Usage

    [/path/to/sqlite.db] [PORT] [MAX_CONTENT_SIZE_IN_MB] [MODE]

MODE can be:

* ro (read-only)
* rw (read-write)