R"===(const actionbtn = document.getElementById("actionbtn");
const input_bothparts = document.getElementById("input_bothparts");
const input_domain = document.getElementById("input_domain");
const input_urlpath = document.getElementById("input_urlpath");
const link_to_cached_page = document.getElementById("link_to_cached_page");
function both_parts_of_url(url){
	return url.match(/^https?:\/\/([^\/]+)(\/.*)$/);
}
function set_both_parts_of_url(url){
	const m = both_parts_of_url(url);
	if (m !== null){
		input_domain.value = m[1];
		input_urlpath.value = m[2];
	}
	return (m !== null);
}
function guess_url_from_content(value){
	const searchforstr = '<a href="https://yandexwebcache.net/yandbtm?';
	const yandex_cache_url_begin_index = value.indexOf(searchforstr);
	
	if (yandex_cache_url_begin_index !== -1){
		const end_of_link = value.indexOf('"', yandex_cache_url_begin_index+searchforstr.length);
		const url_within_this = value.substr(yandex_cache_url_begin_index, end_of_link-yandex_cache_url_begin_index);
		
		let url_start = url_within_this.indexOf("&amp;url=");
		if (url_start !== -1){
			url_start += 9;
			let url_enddd = url_within_this.indexOf("&amp;",url_start);
			if (url_enddd === -1)
				url_enddd = url_within_this.length;
			return decodeURIComponent(url_within_this.substr(url_start, url_enddd-url_start));
		}
	} else {
		const node = document.createElement("div");
		node.innerHTML = value;
		return Array.from(node.getElementsByTagName("link")).filter(x => x.rel==="canonical")[0].href;
	}
	return null;
}
function guess_bothparts_from_content(value){
	const url = guess_url_from_content(value);
	if (url)
		return both_parts_of_url(url);
	else
		return null;
}
input_bothparts.addEventListener("change", ()=>{
	if (set_both_parts_of_url(input_bothparts.value))
		input_bothparts.value = "";
});
actionbtn.addEventListener("pointerup", ()=>{
	let domain = input_domain.value;
	const should_guess_url_from_content = document.getElementById("guess_url_from_content").checked;
	if (should_guess_url_from_content || domain){
		if ((!should_guess_url_from_content) && (domain.match(/^[a-z0-9-]+([.][a-z0-9-]+)+$/) === null)){
			alert("Bad domain");
			return;
		}
		let path = input_urlpath.value;
		if (should_guess_url_from_content || path){
			const do_from_clipboard = document.getElementById("addfromclipboard").checked;
			const do_from_filepath  = document.getElementById("enable_filepath").checked;
			const do_from_textarea  = document.getElementById("enable_textarea").checked;
			
			if (do_from_clipboard + do_from_filepath + do_from_textarea === 1){
				let filepath = "";
				let contents = "";
				let promise = Promise.resolve();
				
				if (do_from_clipboard){
					promise = navigator.clipboard.readText();
				} else if (do_from_filepath){
					filepath = document.getElementById("input_filepath").value;
					if (filepath){
						if (!filepath.startsWith("/")){
							alert("Filepath doesn't start with slash");
							return;
						}
					}
				} else if (do_from_textarea){
					contents = document.getElementById("textarea").value;
				}
				
				actionbtn.disabled = true;
				link_to_cached_page.classList.add("display_none");
				promise.then(_contents => {
					if (_contents !== undefined)
						contents = _contents;
					if (should_guess_url_from_content){
						const m = guess_bothparts_from_content(contents);
						if (m === null){
							alert("Couldn't guess domain or path");
							return;
						} else {
							domain = m[1];
							path = m[2];
						}
					}
					if (!path.startsWith("/")){
						alert("Path doesn't start with slash");
						return;
					}
					fetch(document.location, {credentials:"include", method:"POST", body:domain+"\n"+path+"\n"+filepath+"\n\n"+contents}).then(r => {
						actionbtn.disabled = false;
						if(!r.ok){
							const errstr = `Server returned ${r.status}: ${r.statusText}`;
							alert(errstr);
							throw Error(errstr);
						} else if (contents.length !== 0){
							link_to_cached_page.href = "/cached/https://"+domain+path;
							link_to_cached_page.classList.remove("display_none");
							fetch("/cached/https://"+domain+path, {credentials:"include", method:"GET"}).then(rrr => {
								if (rrr.ok){
									rrr.blob().then(blobby => {
										blobby.bytes().then(uint8arr => {
											const expected_values = new TextEncoder().encode(contents);
											let matched = (expected_values.length === uint8arr.length);
											if (matched){
												for (let i = 0;  i < expected_values.length;  ++i){
													if (expected_values[i] !== uint8arr[i]){
														matched = false;
														break;
													}
												}
											}
											if (!matched){
												alert("Server's cached version differs from what was requested to be cached. You should try again, using a file path instead of sending the file contents directly.");
											}
										});
									});
								} else {
									alert(`Couldn't verify server's cached version is uncorrupted - server returned ${r.status}: ${r.statusText}`);
								}
							});
						}
					});
				});
			} else {
				alert("Please select exactly ONE method");
			}
		}
	}
});
document.getElementById("enable_textarea").addEventListener("change", e=>{
	document.getElementById("textarea").disabled = !e.currentTarget.checked;
});)==="