// ==UserScript==
// @name         Add WebCache buttons to Reddit
// @namespace    http://tampermonkey.net/
// @version      2024-09-17
// @description  Add WebCache buttons to Reddit
// @author       You
// @match        https://www.reddit.com/*
// @match        https://old.reddit.com/*
// @grant        none
// ==/UserScript==

for (let node of Array.from(document.getElementsByClassName("domain"))){
    const new_link = document.createElement("a");
    new_link.href = "http://localhost:8080/cached/" + node.previousElementSibling.href;
    new_link.innerText = "CACHED";
    node.appendChild(new_link);
}