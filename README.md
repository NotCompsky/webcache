# Description

Web server that serves files from a SQLite database.

Features:

* fast (very low latency)
* supports gzip compression
* supports up to 3000 requests per second
  * Although SQLite lookup speeds probably dramatically limit this in realistic scenarios
* ability to write to the database with a `POST` request
  * You are able to use a script to, for any web page you visit, cache the page by clicking a button

Bugs:

* large files (above around 500KB) should be saved as files and inserted via specifying the filepath, they should not be uploaded
  * This is because the server cannot handle large incoming requests - there is some as-yet-unidentified bug

Missing features:

* No HTTPS
  * If you require this, run it behind a reverse proxy like nginx
* Does not respect CORS headers
  * Currently, CORS headers are written for maximum security - thus many assets do not load except in `local-only mode`

# Compiling

Prerequisites:

* libz.h
  * Installed with `libz-dev` or `zlib1g-dev`
* [sqlite3.c](https://www.sqlite.org/download.html)
  * Or you can modify the compile instructions to use `libsqlite0-dev`
* [libycompsky](https://github.com/NotCompsky/libcompsky)
* Any modern C++ compiler

# Usage

## Server

    [/path/to/sqlite.db] [PORT] [MAX_CONTENT_SIZE_IN_MB] [MODE]

MODE can be:

* ro (read-only)
* rw (read-write)

## Command line

Example:

![usageB1](https://github.com/user-attachments/assets/dc4f67c2-c6cf-4582-a361-a0399d8aabd9)

## Browser

Greasemonkey/Tampermonkey scripts:

* `baseline`
  * Adds a 'copy to clipboard' button to all websites, so you can cache the HTML contents (only HTML) with just a couple of clicks
  * The web pages would look exactly like their normal versions if CORS headers were respected; currently however this is not the case<!-- TODO -->
* `local-only mode`
  * Rewrites cached pages (via client-side JS) to only load assets from the WebCache server
  * To make a website look like its original, you must usually cache all of its assets too (CSS, JavaScript, images, JSON...)

Example:

![usage addonB1](https://github.com/user-attachments/assets/5592b850-45d6-48de-a431-f757f9d65538)
![usage addonB2](https://github.com/user-attachments/assets/8bc57193-5d98-412a-9564-e2b11e75ec65)
![usage addonB3](https://github.com/user-attachments/assets/2341c489-30f2-4fa4-b8c0-5bdd7633c7b4)
![usage addonB4](https://github.com/user-attachments/assets/0147b040-d0fa-410f-a56a-0df68e0309c4)
![usage addonB5](https://github.com/user-attachments/assets/4232409b-4d6c-43e9-afd6-4a9fa49b7b0c)