// ==UserScript==
// @name         WebCache pages utilities
// @namespace    http://tampermonkey.net/
// @version      2024-09-16
// @description  .
// @author       You
// @match        http://localhost:8080/cached/*
// @grant        none
// ==/UserScript==

const btn = document.createElement("button");
btn.innerText = "Overwrite WebCache with this edited HTML";
btn.style.position = "fixed";
btn.style.left = "0";
btn.style.bottom = "0";
btn.style.zIndex = "9999";
btn.addEventListener("pointerup", ()=>{
    const url_prefix = document.location.protocol+"//"+document.location.host;
    const full_path = document.location.toString().substr(url_prefix.length);

    const m = full_path.match(/\/cached\/https?:\/\/([^\/]+)(\/.*)$/);
    const domain = m[1];
    const path = m[2];

    btn.remove();

	const contents = ((document.doctype===null)?"":"<!DOCTYPE html>")+document.documentElement.outerHTML;
	fetch(document.location.protocol+"//"+document.location.host+"/cacheinsert", {credentials:"include", method:"POST", body:domain+"\n"+path+"\n\n\n"+contents}).then(r => {
        if(!r.ok){
            const errstr = `Server returned ${r.status}: ${r.statusText}`;
            alert(errstr);
            document.body.appendChild(btn);
        } else {
            fetch(document.location.protocol+"//"+document.location.host+"/cached/https://"+domain+path, {credentials:"include", method:"GET"}).then(rrr => {
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
                            } else {
                                document.body.appendChild(btn);
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
document.body.appendChild(btn);