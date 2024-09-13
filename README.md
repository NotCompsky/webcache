# Description

Web server that serves files from a SQLite database.

Features:

* supports gzip compression
* supports up to 3000 requests per second
  * Although SQLite lookup speeds probably dramatically limit this in realistic scenarios

Future features:

* ability to write to the database with a `POST` request
  * Then you will be able to use a script to, for any web page you visit, cache the page by clicking a button

# Compiling

Prerequisites:

* libz.h
  * Installed with `libz-dev` or `zlib1g-dev`
* Any modern C++ compiler