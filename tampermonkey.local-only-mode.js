// ==UserScript==
// @name         WebCache pages utilities
// @namespace    http://tampermonkey.net/
// @version      2024-09-16
// @description  local-only mode
// @author       You
// @match        http://localhost:8080/cached/*
// @grant        none
// ==/UserScript==

function standardise_url(url){
    return document.location.protocol + document.location.host + "/cached/" + url;
}

for (let node of document.getElementsByTagName("a")){
    node.href = standardise_url(node.href);
}
for (let node of document.getElementsByTagName("img")){
    node.src = standardise_url(node.src);
}