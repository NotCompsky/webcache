#!/usr/bin/env bash

if [[ $# -eq 1 ]]; then
	OUTFILE_PATH="$1"
	xclip -selection clipboard -o > "$OUTFILE_PATH"
	ls -l "$OUTFILE_PATH"

	COMPSKYSUCCESSHERE=""
	path=""
	url=""
	while read -r line; do
		if [[ "$path" ]]; then
			url="$line"
		elif [[ "$domain" ]]; then
			path="$line"
		elif [[ "$COMPSKYSUCCESSHERE" ]]; then
			domain="$line"
		else
			COMPSKYSUCCESSHERE="$line"
		fi
	done < <(cat "$OUTFILE_PATH" | sed -E 's~<!DOCTYPE .*<base href="(https?://([^"/]+)(/[^"]*))"~COMPSKYSUCCESSHERE\n\2\n\3\n\1\n~' | grep -A 3 COMPSKYSUCCESSHERE | head -n 4)
	echo "$url"
	read -p "Set cache as this URL? [y=yes] " set_cache_as_url
	if [ "$set_cache_as_url" = y ]; then
		curl 'http://localhost:8080/cacheinsert' -X POST -H 'Accept: */*' -H 'Content-Type: text/plain;charset=UTF-8' -H 'Origin: http://localhost:8080' --data-raw "$domain
$path
$OUTFILE_PATH

	"
		echo ""
		printf "%s" "http://localhost:8080/cached/$url" | xclip -i -selection clipboard
		# NOTE: By default, primary (XA_PRIMARY) is pasted with middle-button, NOT left-button
	fi
else
	echo "USAGE: $0 [TEMPORARY_FILE_PATH]" >&2
fi