// ==UserScript==
// @name         WebCache pages utilities
// @namespace    http://tampermonkey.net/
// @version      2024-09-16
// @description  .
// @author       You
// @match        http://localhost:8080/cached/*
// @grant        none
// ==/UserScript==

const dont_cache_this_classname = "notcompsky-webcache-dont-cache-this";
const dont_cache_this__item2parent = {};
const btn = document.createElement("button");
btn.innerText = "Overwrite WebCache with this edited HTML";
btn.style.position = "fixed";
btn.style.left = "0";
btn.style.bottom = "0";
btn.style.zIndex = "9999";
function detach_things(){
    for (let node of document.getElementsByClassName(dont_cache_this_classname)){
        let topmost_uncacheable_node = node;
        let tmp = node;
        while (tmp !== document.body){
            tmp = tmp.parentNode;
            if (tmp.classList.contains(dont_cache_this_classname))
                topmost_uncacheable_node = tmp;
        }
        
        if (dont_cache_this__item2parent[topmost_uncacheable_node] === undefined)
            dont_cache_this__item2parent[topmost_uncacheable_node] = [topmost_uncacheable_node.parentNode, topmost_uncacheable_node.nextElementSibling];
    }
    for (let node of Object.keys(dont_cache_this__item2parent)){
        try {
            node.remove();
        } catch(e){
            console.log(e, "while trying to remove", node.outerHTML);
        }
    }
}
function reattach_things(){
    for (let [node,relations] of Object.entries(dont_cache_this__item2parent)){
        if (relations[1] === null){
            relations[0].appendChild(node);
        } else {
            relations[0].insertBefore(node, relations[1]);
        }
    }
}
btn.addEventListener("pointerup", ()=>{
    const url_prefix = document.location.protocol+"//"+document.location.host;
    const full_path = document.location.toString().substr(url_prefix.length);

    const m = full_path.match(/\/cached\/https?:\/\/([^\/]+)(\/.*)$/);
    const domain = m[1];
    const path = m[2];

    // Temporarily remove items from the DOM before printing the HTML
    btn.disabled = true;
    btn.remove();
    detach_things();
    
    const contents = ((document.doctype===null)?"":"<!DOCTYPE html>")+document.documentElement.outerHTML;
    document.body.appendChild(btn);

	fetch(document.location.protocol+"//"+document.location.host+"/cacheinsert", {credentials:"include", method:"POST", body:domain+"\n"+path+"\n\n\n"+contents}).then(r => {
        if(!r.ok){
            const errstr = `Server returned ${r.status}: ${r.statusText}`;
            alert(errstr);
        } else {
            setTimeout(()=>{
                fetch(document.location.protocol+"//"+document.location.host+"/cached/https://"+domain+path, {credentials:"include", method:"GET"}).then(rrr => {
                    btn.disabled = false;
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
                                    alert("Server's cached version differs from what was requested to be cached. This might be because it is taking longer to update the database (and there is no error) or it might be due to an error. You should try again, using a file path instead of sending the file contents directly.");
                                } else {
                                    document.body.appendChild(btn);
                                    reattach_things();
                                }
                            });
                        });
                    } else {
                        alert(`Couldn't verify server's cached version is uncorrupted - server returned ${r.status}: ${r.statusText}`);
                    }
                });
            }, 9000);
        }
	});
});
document.body.appendChild(btn);