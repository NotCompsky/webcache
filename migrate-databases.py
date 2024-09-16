#!/usr/bin/env python3

import sqlite3

def migrate_one_domain(from_db:str, to_db:str, domains:list):
	db = sqlite3.connect("file:"+from_db+"?mode=ro")
	cursor = db.cursor()
	
	db2 = sqlite3.connect(to_db)
	cursor2 = db2.cursor()
	
	for domain in domains:
		cursor2.execute("CREATE TABLE file (domain STRING NOT NULL, path STRING NOT NULL, content BLOB NOT NULL, headers STRING NOT NULL, t_added INTEGER UNSIGNED NOT NULL, hits INTEGER UNSIGNED NOT NULL, UNIQUE (domain,path))")
		
		rowid_offset:int = 0
		while True:
			cursor.execute("SELECT rowid,path,content,headers,t_added,hits FROM file WHERE domain=? AND rowid>? ORDER BY rowid ASC LIMIT 100", (domain,rowid_offset))
			ls:list = cursor.fetchall()
			if len(ls) == 0:
				break
			for rowid,path,content,headers,t_added,hits in ls:
				cursor2.execute("INSERT INTO file (domain,path,content,headers,t_added,hits) VALUES (?,?,?,?,?,?)", (domain,path,content,headers,t_added,hits))
				rowid_offset = rowid
			db2.commit()
			print("Done up to", rowid_offset)
	
	db2.close()
	db.close()

if __name__ == "__main__":
	import argparse
	
	parser = argparse.ArgumentParser()
	parser.add_argument("--from-db", help="/path/to/sqlite.db")
	parser.add_argument("--to-db", help="/path/to/sqlite.db")
	parser.add_argument("domain", nargs="+")
	args = parser.parse_args()
	
	migrate_one_domain(args.from_db, args.to_db, args.domain)