R"===(const actionbtn = document.getElementById("actionbtn");
const input_bothparts = document.getElementById("input_bothparts");
const input_domain = document.getElementById("input_domain");
const input_urlpath = document.getElementById("input_urlpath");
function set_both_parts_of_url(url){
	const m = url.match(/^https?:\/\/([^\/]+)(\/.*)$/);
	if (m !== null){
		input_domain.value = m[1];
		input_urlpath.value = m[2];
	}
	return (m !== null);
}
function guess_bothparts_from_content(value){
	const searchforstr = '<a href="https://yandexwebcache.net/yandbtm?';
	const yandex_cache_url_begin_index = value.indexOf(searchforstr);
	const end_of_link = value.indexOf('"', yandex_cache_url_begin_index+searchforstr.length);
	const url_within_this = value.substr(yandex_cache_url_begin_index, end_of_link-yandex_cache_url_begin_index);
	
	let url_start = url_within_this.indexOf("&amp;url=");
	if (url_start !== -1){
		url_start += 9;
		let url_enddd = url_within_this.indexOf("&amp;",url_start);
		if (url_enddd === -1)
			url_enddd = url_within_this.length;
		set_both_parts_of_url(decodeURIComponent(url_within_this.substr(url_start, url_enddd-url_start)));
	}
}
input_bothparts.addEventListener("change", ()=>{
	if (set_both_parts_of_url(input_bothparts.value))
		input_bothparts.value = "";
});
document.getElementById("guess_from_textarea").addEventListener("pointerup", ()=>{
	guess_bothparts_from_content(document.getElementById("textarea").value);
});
document.getElementById("guess_from_clipboard").addEventListener("pointerup", ()=>{
	navigator.clipboard.readText().then(content => guess_bothparts_from_content(content));
});
actionbtn.addEventListener("pointerup", ()=>{
	const domain = input_domain.value;
	if (domain){
		if (domain.match(/^[a-z0-9-]+([.][a-z0-9-]+)+$/) === null){
			alert("Bad domain");
			return;
		}
		const path = input_urlpath.value;
		if (path){
			if (!path.startsWith("/")){
				alert("Path doesn't start with slash");
				return;
			}
			
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
				promise.then(_contents => {
					if (_contents !== undefined)
						contents = _contents;
					fetch(document.location, {credentials:"include", method:"POST", body:domain+"\n"+path+"\n\n"+filepath+"\n"+contents}).then(r => {
						actionbtn.disabled = false;
						if(!r.ok){
							const errstr = `Server returned ${r.status}: ${r.statusText}`;
							alert(errstr);
							throw Error(errstr);
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