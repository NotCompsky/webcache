// ==UserScript==
// @name         webcache buttons
// @namespace    http://tampermonkey.net/
// @version      2024-09-14
// @description  Adds 'copy to clipboard' button for every website
// @author       Me
// @match        https://*/*
// @grant        none
// ==/UserScript==

const btn = document.createElement("button");
btn.innerText = "Copy to clipboard";
btn.style.position = "fixed";
btn.style.left = "0";
btn.style.bottom = "0";
btn.style.zIndex = "9999";
btn.addEventListener("pointerup", ()=>{
    btn.remove();
    navigator.clipboard.writeText(((document.doctype===null)?"":"<!DOCTYPE html>")+document.documentElement.outerHTML);
    document.body.appendChild(btn);
});
document.body.appendChild(btn);