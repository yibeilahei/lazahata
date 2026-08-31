#pragma once

// Minimal file manager page served by FileTransferServer. Plain HTML/CSS/JS,
// no build step and no external JS libraries (unlike CrossPoint's jszip-based
// page) — this only needs list/upload/download/mkdir/rename/move/delete.
static const char FILE_MANAGER_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>lazahata file transfer</title>
<style>
body { font-family: sans-serif; margin: 0; padding: 16px; background: #f4f4f4; color: #222; }
h1 { font-size: 18px; }
#path { font-family: monospace; margin-bottom: 8px; word-break: break-all; }
table { width: 100%; border-collapse: collapse; background: #fff; }
td, th { padding: 8px; border-bottom: 1px solid #ddd; text-align: left; font-size: 14px; }
tr.dir td:first-child { font-weight: bold; cursor: pointer; }
button { font-size: 13px; padding: 4px 8px; margin-left: 4px; }
#toolbar { margin: 12px 0; }
#status { color: #666; font-size: 13px; margin-top: 8px; }
#uploadProgress { display: none; width: 100%; height: 18px; margin-top: 8px; }
#cancelUpload { display: none; }
</style>
</head>
<body>
<h1>lazahata file transfer</h1>
<div id="path">/</div>
<div id="toolbar">
  <input type="file" id="fileInput">
  <button onclick="upload()">Upload</button>
  <button id="cancelUpload" onclick="cancelUpload()">Cancel upload</button>
  <button onclick="mkdir()">New folder</button>
  <button onclick="load()">Refresh</button>
</div>
<progress id="uploadProgress" value="0" max="100"></progress>
<table id="list"><thead><tr><th>Name</th><th>Size</th><th></th></tr></thead><tbody></tbody></table>
<div id="status"></div>
<script>
let path = "/";
let activeUpload = null;

function status(msg) { document.getElementById("status").textContent = msg || ""; }

function formatSize(bytes) {
  if (bytes < 1024) return bytes + " B";
  const units = ["KB", "MB", "GB"];
  let value = bytes / 1024;
  let i = 0;
  while (value >= 1024 && i < units.length - 1) { value /= 1024; i++; }
  return value.toFixed(value < 10 ? 1 : 0) + " " + units[i];
}

function load() {
  document.getElementById("path").textContent = path;
  fetch("/api/files?path=" + encodeURIComponent(path))
    .then(r => r.json())
    .then(items => {
      const body = document.querySelector("#list tbody");
      body.innerHTML = "";
      if (path !== "/") {
        const up = document.createElement("tr");
        up.className = "dir";
        up.innerHTML = "<td>..</td><td></td><td></td>";
        up.querySelector("td").onclick = () => { path = path.replace(/\/[^/]+\/?$/, "") || "/"; load(); };
        body.appendChild(up);
      }
      items.forEach(item => {
        const row = document.createElement("tr");
        if (item.isDirectory) row.className = "dir";
        const name = document.createElement("td");
        name.textContent = item.name;
        const size = document.createElement("td");
        size.textContent = item.isDirectory ? "" : formatSize(item.size);
        const actions = document.createElement("td");
        const full = (path === "/" ? "" : path) + "/" + item.name;
        if (item.isDirectory) {
          name.onclick = () => { path = full; load(); };
        } else {
          const dl = document.createElement("a");
          dl.href = "/download?path=" + encodeURIComponent(full);
          dl.textContent = "Download";
          actions.appendChild(dl);
        }
        const ren = document.createElement("button");
        ren.textContent = "Rename";
        ren.onclick = () => rename(full, item.name);
        actions.appendChild(ren);
        const mv = document.createElement("button");
        mv.textContent = "Move";
        mv.onclick = () => move_(full);
        actions.appendChild(mv);
        const del = document.createElement("button");
        del.textContent = "Delete";
        del.onclick = () => del_(full);
        actions.appendChild(del);
        row.appendChild(name);
        row.appendChild(size);
        row.appendChild(actions);
        body.appendChild(row);
      });
    })
    .catch(e => status("Error: " + e));
}

function upload() {
  const input = document.getElementById("fileInput");
  if (!input.files.length) { status("Choose a file first"); return; }
  const file = input.files[0];

  const bar = document.getElementById("uploadProgress");
  const cancelBtn = document.getElementById("cancelUpload");
  bar.style.display = "block";
  bar.value = 0;
  cancelBtn.style.display = "inline-block";

  let lastTime = performance.now();
  let lastLoaded = 0;
  let speed = 0;  // smoothed bytes/sec

  const xhr = new XMLHttpRequest();
  activeUpload = xhr;
  xhr.upload.onprogress = (e) => {
    const now = performance.now();
    const dt = (now - lastTime) / 1000;
    if (dt > 0.2) {  // update speed a few times a second, not on every chunk
      const instantSpeed = (e.loaded - lastLoaded) / dt;
      // Exponential moving average smooths out bursty per-chunk timing.
      speed = speed ? speed * 0.7 + instantSpeed * 0.3 : instantSpeed;
      lastTime = now;
      lastLoaded = e.loaded;
    }
    if (e.lengthComputable) {
      bar.value = Math.round((e.loaded / e.total) * 100);
      status("Uploading... " + bar.value + "% (" + formatSize(e.loaded) + " / " + formatSize(e.total) +
             ") - " + formatSize(speed) + "/s");
    } else {
      status("Uploading... " + formatSize(e.loaded) + " - " + formatSize(speed) + "/s");
    }
  };
  xhr.onload = () => {
    activeUpload = null;
    bar.style.display = "none";
    cancelBtn.style.display = "none";
    status(xhr.responseText);
    input.value = "";
    load();
  };
  xhr.onerror = () => {
    activeUpload = null;
    bar.style.display = "none";
    cancelBtn.style.display = "none";
    status("Upload failed");
  };
  xhr.onabort = () => {
    activeUpload = null;
    bar.style.display = "none";
    cancelBtn.style.display = "none";
    status("Upload canceled");
    load();
  };
  // Sent as a raw body (no multipart wrapper) so the device can bulk-read it
  // straight into its write buffer instead of parsing it byte-by-byte.
  xhr.open("POST", "/upload");
  xhr.setRequestHeader("Content-Type", "application/octet-stream");
  xhr.setRequestHeader("X-File-Path", encodeURIComponent(path));
  xhr.setRequestHeader("X-File-Name", encodeURIComponent(file.name));
  xhr.send(file);
}

function cancelUpload() {
  if (activeUpload) activeUpload.abort();
}

function mkdir() {
  const name = prompt("Folder name:");
  if (!name) return;
  fetch("/mkdir", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "name=" + encodeURIComponent(name) + "&path=" + encodeURIComponent(path) })
    .then(() => load())
    .catch(e => status("Error: " + e));
}

function rename(fullPath, oldName) {
  const name = prompt("New name:", oldName);
  if (!name || name === oldName) return;
  fetch("/rename", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "path=" + encodeURIComponent(fullPath) + "&name=" + encodeURIComponent(name) })
    .then(() => load())
    .catch(e => status("Error: " + e));
}

function move_(fullPath) {
  const dest = prompt("Destination folder (e.g. /Books):", path);
  if (!dest) return;
  fetch("/move", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "path=" + encodeURIComponent(fullPath) + "&dest=" + encodeURIComponent(dest) })
    .then(() => load())
    .catch(e => status("Error: " + e));
}

function del_(fullPath) {
  if (!confirm("Delete " + fullPath + "?")) return;
  fetch("/delete", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "path=" + encodeURIComponent(fullPath) })
    .then(() => load())
    .catch(e => status("Error: " + e));
}

load();
</script>
</body>
</html>
)HTML";
