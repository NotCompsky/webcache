#!/usr/bin/env python3


from compression_utils import compress_w_best_compressor
from bs4 import BeautifulSoup as bs
from urllib.parse import unquote as decodeURIComponent


RED:str = '\033[31;1;1m'
GREEN:str = '\033[32;1;2m'
BLANK:str = '\033[0m'


def guess_url_from_content(value:bytes):
	# Lazily converted from JS to Python
	searchforstr:bytes = b'<a href="https://yandexwebcache.net/yandbtm?'
	
	if (searchforstr in value):
		yandex_cache_url_begin_index:int = value.index(searchforstr)
		end_of_link:int = value.index(b'"', yandex_cache_url_begin_index+len(searchforstr))
		url_within_this:bytes = value[yandex_cache_url_begin_index:end_of_link]
		
		url_start:int = url_within_this.index(b"&amp;url=")
		if (url_start != -1):
			url_start += 9
			url_enddd:int = url_within_this.index(b"&amp;",url_start)
			if (url_enddd == -1):
				url_enddd = len(url_within_this)
			return decodeURIComponent(url_within_this[url_start:url_enddd].decode())
	else:
		soup = bs(value, "lxml")
		canonical_node = soup.find("link",{"rel":"canonical"})
		if canonical_node is not None:
			return canonical_node["href"]
		else:
			canonical_node = soup.find("shreddit-canonical-url-updater")
			if canonical_node is not None:
				return canonical_node["value"]
		return None


if __name__ == "__main__":
	import argparse
	import os
	import re
	import sqlite3
	import json
	from datetime import datetime as dt
	
	parser = argparse.ArgumentParser()
	parser.add_argument("--db", help="Path to the WebCache SQLite database file. If not set, will only simulate a run.")
	parser.add_argument("dirpath")
	parser.add_argument("--readonly", default=False, action="store_true")
	parser.add_argument("--guess-url-from-contents", default=False, action="store_true")
	parser.add_argument("--domain", required=False)
	parser.add_argument("--format", default="/%(fname)s", help="Variables: fname, fname_but_underscores_replaced_by_slashes")
	parser.add_argument("--regexp", help="Only insert files which match this regexp")
	parser.add_argument("--dont-look-for-headers-json", default=False, action="store_true", help="Ignore the <path>.headers.json files")
	parser.add_argument("--replace", default=False, action="store_true")
	parser.add_argument("--dont-compress", default=False, action="store_true")
	parser.add_argument("--delete-when-cached", default=False, action="store_true")
	args = parser.parse_args()
	
	if (args.domain is not None) ^ (not args.guess_url_from_contents):
		raise ValueError("Exactly one of --domain and --guess-url-from-contents must be provided")
	
	if args.regexp is None:
		input("WARNING: You have not used a regexp. Press ENTER if you still want to continue.")
	
	if not args.format.startswith("/"):
		input("WARNING: Path --format does not begin with a slash. Press ENTER if you still want to continue.")
	
	db = None
	cursor = None
	sqlmode:str = "INSERT OR REPLACE" if args.replace else "INSERT"
	
	if args.db is not None:
		db = sqlite3.connect("file:" + args.db + ("?mode=ro" if args.readonly else ""))
		cursor = db.cursor()
	
	bytes_added:int = 0
	files_added_to_db:list = []
	resume_from:bool = False
	for fname in os.listdir(args.dirpath):
		if not resume_from:
			if fname == "80 MPs who supported assisted dying bill could turn against it with High Court judge safeguard removed _ ukpolitics.html":
				resume_from = True
			else:
				continue
		
		if fname.endswith(".headers.json"):
			continue
		if args.regexp is not None:
			if re.search(args.regexp, fname) is None:
				print("Does not match regexp: " + fname, end="\r")
				continue
		fp:str = args.dirpath + "/" + fname
		stat = os.stat(fp)
		headers:dict = {}
		if args.dont_look_for_headers_json:
			pass
		else:
			headers_fp:str = fp + ".headers.json"
			if os.path.exists(headers_fp):
				with open(headers_fp, "r") as f:
					headers = json.load(f)
			headers = {x.lower():y for x,y in headers.items()}
		
		if "content-encoding" in headers:
			del headers["content-encoding"]
		if "transfer-encoding" in headers:
			del headers["transfer-encoding"]
		
		if not os.path.isfile(fp):
			print(RED + "WARNING: Skipping subdirectory (or non-file): " + fp + BLANK)
			continue
		
		content:bytes = None
		with open(fp,"rb") as f:
			content = f.read()
		
		format_args:dict = {
			"fname": fname,
			"fname_but_underscores_replaced_by_slashes": fname.replace("_","/"),
		}
		
		domain:str = args.domain
		path:str = None
		if args.guess_url_from_contents:
			url:str = guess_url_from_content(content)
			if url is not None:
				m = re.search("^https?://([^/]+)(/.*)$", url)
				if m is None:
					print(RED + "Guessed bad URL: " + url + BLANK)
					continue
				else:
					domain, path = m.groups()
					print("Guessed " + domain + " -- " + path)
			else:
				print(RED+"Failed to guess URL of " + fname+BLANK)
				continue
		else:
			path = args.format % format_args
		t_added:int = int(stat.st_ctime)
		
		content_length:int = len(content)
		if not args.dont_compress:
			content_encoding_header, content = compress_w_best_compressor(content)
			if len(content) != content_length:
				content_encoding_header_value:str = content_encoding_header.replace("Content-Encoding: ","").replace("\r\n","")
				print(f'{content_encoding_header_value} compressed {content_length//1024}KiB -> {len(content)//1024}KiB', end=" \n")
				content_length = len(content)
				headers["content-encoding"] = content_encoding_header_value
		headers["content-length"] = str(content_length)
		
		if db is not None:
			try:
				if not args.readonly:
					cursor.execute(sqlmode+" INTO file (domain, path, content, headers, t_added, hits) VALUES (?, ?, ?, ?, ?, ?)", (domain, path, content, json.dumps(headers,separators=(", ",": ")), t_added, 0))
			except sqlite3.IntegrityError:
				print("Already exists: "+fname)
			else:
				files_added_to_db.append(fp)
				bytes_added += content_length
				print("Inserted: " + GREEN + fname + BLANK)
				if bytes_added > 50*1024*1024:
					print(GREEN + "Committing..." + BLANK)
					if not args.readonly:
						db.commit()
					bytes_added = 0
		else:
			print(f"{fname} -> {domain} {path}: t={t_added} ({dt.fromtimestamp(t_added).strftime('%Y/%m/%d %H:%M:%S')})", end=" \n")
			print(headers, end=" \n")
	print("Committing...")
	if not args.readonly:
		db.commit()
	
	cursor.close()
	db.close()
	
	print("Added the following files to the database:")
	for fp in files_added_to_db:
		print(fp)
		if args.delete_when_cached and (not args.readonly):
			os.remove(fp)