#include "network/FileTransferServer.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WebServer.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

#include "network/html/FileManagerPage.h"

#ifndef LAZAHATA_VERSION
#define LAZAHATA_VERSION "dev"
#endif

namespace {
// Appends `s` to `out`, escaping '"' and '\' so it stays valid inside a JSON string.
void appendJsonEscaped(std::string& out, const char* s) {
  for (const char* p = s; *p; ++p) {
    if (*p == '"' || *p == '\\') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
}

void joinPath(char* out, size_t outSize, const char* dir, const char* name) {
  if (strcmp(dir, "/") == 0) {
    snprintf(out, outSize, "/%s", name);
  } else {
    snprintf(out, outSize, "%s/%s", dir, name);
  }
}

// Directory portion of `path` (parent folder), written into `out`. "/" if path has no parent.
void dirnameOf(char* out, size_t outSize, const char* path) {
  const char* slash = strrchr(path, '/');
  if (!slash || slash == path) {
    snprintf(out, outSize, "/");
    return;
  }
  const size_t n = static_cast<size_t>(slash - path);
  snprintf(out, outSize, "%.*s", static_cast<int>(n), path);
}

// Final path segment (file/folder name) of `path`.
const char* basenameOf(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

bool isProtectedPath(const char* path) {
  const char* name = basenameOf(path);
  return name[0] == '.' || strcmp(name, "System Volume Information") == 0;
}
}  // namespace

class FileTransferServer::RawUploadHandler : public RequestHandler {
  FileTransferServer& owner;

 public:
  explicit RawUploadHandler(FileTransferServer& owner) : owner(owner) {}

  bool canHandle(WebServer& /*server*/, HTTPMethod method, const String& uri) override {
    return method == HTTP_POST && uri == "/upload";
  }
  bool canRaw(WebServer& /*server*/, const String& uri) override { return uri == "/upload"; }

  void raw(WebServer& /*server*/, const String& /*uri*/, HTTPRaw& raw) override {
    switch (raw.status) {
      case RAW_START:
        owner.handleUploadStart();
        break;
      case RAW_WRITE:
        owner.handleUploadChunk(raw.buf, raw.currentSize);
        break;
      case RAW_END:
        owner.handleUploadEnd(raw.totalSize);
        break;
      case RAW_ABORTED:
        owner.handleUploadAbort();
        break;
    }
  }

  bool handle(WebServer& /*server*/, HTTPMethod /*method*/, const String& /*uri*/) override {
    owner.sendUploadResponse();
    return true;
  }
};

FileTransferServer::FileTransferServer() = default;
FileTransferServer::~FileTransferServer() { stop(); }

bool FileTransferServer::begin() {
  server = makeUniqueNoThrow<WebServer>(80);
  if (!server) {
    LOG_ERR("XFER", "OOM: WebServer");
    return false;
  }

  // The raw upload handler reads these directly (query-string args aren't
  // parsed for raw-body requests, so path/name travel as headers instead;
  // Content-Length doubles as the pre-allocation size hint).
  static const char* kCollectedHeaders[] = {"Content-Length", "X-File-Path", "X-File-Name"};
  server->collectHeaders(kCollectedHeaders, 3);

  uploadHandler = new (std::nothrow) RawUploadHandler(*this);
  if (!uploadHandler) {
    LOG_ERR("XFER", "OOM: upload handler");
    return false;
  }
  server->addHandler(uploadHandler);

  server->on("/", HTTP_GET, [this]() { handleRoot(); });
  server->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server->on("/api/files", HTTP_GET, [this]() { handleFileList(); });
  server->on("/download", HTTP_GET, [this]() { handleDownload(); });
  server->on("/mkdir", HTTP_POST, [this]() { handleMkdir(); });
  server->on("/rename", HTTP_POST, [this]() { handleRename(); });
  server->on("/move", HTTP_POST, [this]() { handleMove(); });
  server->on("/delete", HTTP_POST, [this]() { handleDelete(); });
  server->onNotFound([this]() { handleNotFound(); });

  server->begin();
  running = true;

  mdnsHostname = gpio.deviceIsX3() ? "x3" : "x4";
  mdnsStarted = MDNS.begin(mdnsHostname.c_str());
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", 80);
    LOG_INF("XFER", "Server started on port 80, mdns=%s.local", mdnsHostname.c_str());
  } else {
    LOG_ERR("XFER", "mDNS failed to start");
    mdnsHostname.clear();
    LOG_INF("XFER", "Server started on port 80");
  }
  return true;
}

void FileTransferServer::stop() {
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
    mdnsHostname.clear();
  }
  if (server) {
    // ~WebServer() deletes every handler registered via addHandler(),
    // including uploadHandler, so just drop our (non-owning) pointer.
    server->stop();
    server.reset();
  }
  uploadHandler = nullptr;
  running = false;
}

void FileTransferServer::handleClient() {
  if (server) {
    server->handleClient();
  }
}

void FileTransferServer::handleRoot() const { server->send_P(200, "text/html", FILE_MANAGER_PAGE); }

void FileTransferServer::handleStatus() const {
  char json[192];
  snprintf(json, sizeof(json),
           "{\"version\":\"" LAZAHATA_VERSION "\",\"ip\":\"%s\",\"ssid\":\"%s\",\"freeHeap\":%lu,\"uptime\":%lu}",
           WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), static_cast<unsigned long>(ESP.getFreeHeap()),
           static_cast<unsigned long>(millis() / 1000));
  server->send(200, "application/json", json);
}

void FileTransferServer::handleFileList() const {
  std::string path = server->hasArg("path") ? server->arg("path").c_str() : "/";
  if (path.empty()) {
    path = "/";
  }

  HalFile dir = Storage.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    server->send(404, "application/json", "[]");
    return;
  }

  std::string json;
  json.reserve(1024);
  json.push_back('[');
  bool first = true;
  char name[128];
  for (HalFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    if (isProtectedPath(name)) {
      continue;
    }
    if (!first) {
      json.push_back(',');
    }
    first = false;
    json += "{\"name\":\"";
    appendJsonEscaped(json, name);
    json += "\",\"size\":";
    json += std::to_string(file.isDirectory() ? 0 : file.fileSize());
    json += ",\"isDirectory\":";
    json += file.isDirectory() ? "true" : "false";
    json += "}";
  }
  json.push_back(']');
  server->send(200, "application/json", json.c_str());
}

void FileTransferServer::handleDownload() const {
  if (!server->hasArg("path")) {
    server->send(400, "text/plain", "Missing path");
    return;
  }
  const std::string path = server->arg("path").c_str();
  if (isProtectedPath(path.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }

  HalFile file;
  if (!Storage.openFileForRead("XFER", path.c_str(), file)) {
    server->send(404, "text/plain", "File not found");
    return;
  }

  char header[192];
  snprintf(header, sizeof(header), "attachment; filename=\"%s\"", basenameOf(path.c_str()));
  server->sendHeader("Content-Disposition", header);
  server->setContentLength(file.fileSize());
  server->send(200, "application/octet-stream", "");
  server->client().setNoDelay(true);

  constexpr size_t kChunkSize = 16384;
  auto buffer = makeUniqueNoThrow<uint8_t[]>(kChunkSize);
  if (!buffer) {
    LOG_ERR("XFER", "OOM: download buffer");
    return;
  }
  int n;
  while ((n = file.read(buffer.get(), kChunkSize)) > 0) {
    server->client().write(buffer.get(), static_cast<size_t>(n));
  }
}

void FileTransferServer::flushUploadBuffer() {
  if (upload.bufferPos == 0 || !upload.file) {
    return;
  }
  const size_t written = upload.file.write(upload.buffer.data(), upload.bufferPos);
  if (written != upload.bufferPos) {
    LOG_ERR("XFER", "Short upload write (%u of %u)", static_cast<unsigned>(written),
            static_cast<unsigned>(upload.bufferPos));
    upload.success = false;
  }
  upload.bufferPos = 0;
}

void FileTransferServer::handleUploadStart() {
  upload.success = false;
  upload.bufferPos = 0;
  upload.preallocatedSize = 0;
  // Uploads are large sequential writes; Nagle's algorithm just adds
  // latency here, and delayed-ACK has no small-write coalescing to help.
  server->client().setNoDelay(true);

  // Query-string args aren't parsed for raw-body requests, so the browser
  // sends the destination folder/filename as headers instead.
  const std::string dir = WebServer::urlDecode(server->header("X-File-Path")).c_str();
  const std::string name = WebServer::urlDecode(server->header("X-File-Name")).c_str();
  if (name.empty()) {
    LOG_ERR("XFER", "Upload missing X-File-Name header");
    return;
  }
  char full[256];
  joinPath(full, sizeof(full), dir.empty() ? "/" : dir.c_str(), name.c_str());
  upload.destPath = full;
  if (!Storage.openFileForWrite("XFER", upload.destPath.c_str(), upload.file)) {
    LOG_ERR("XFER", "Failed to create %s", upload.destPath.c_str());
    return;
  }
  upload.success = true;

  // Reserve one contiguous extent up front using the client's declared
  // body size as an (over-)estimate: this skips per-cluster FAT chain
  // updates during the writes below. It's shrunk back to the real size
  // with truncate() once we know how many bytes actually landed.
  const std::string contentLength = server->header("Content-Length").c_str();
  if (!contentLength.empty()) {
    const uint64_t hint = strtoull(contentLength.c_str(), nullptr, 10);
    if (hint > 0 && upload.file.preAllocate(hint)) {
      upload.preallocatedSize = hint;
    }
  }
}

void FileTransferServer::handleUploadChunk(const uint8_t* data, size_t len) {
  if (!upload.file) {
    return;
  }
  size_t offset = 0;
  while (offset < len) {
    const size_t space = UploadState::kBufferSize - upload.bufferPos;
    const size_t chunk = std::min(space, len - offset);
    memcpy(upload.buffer.data() + upload.bufferPos, data + offset, chunk);
    upload.bufferPos += chunk;
    offset += chunk;
    if (upload.bufferPos == UploadState::kBufferSize) {
      flushUploadBuffer();
    }
  }
}

void FileTransferServer::handleUploadEnd(size_t totalBytes) {
  flushUploadBuffer();
  if (upload.preallocatedSize > totalBytes) {
    upload.file.truncate(totalBytes);
  }
  LOG_INF("XFER", "Uploaded %s (%lu bytes)", upload.destPath.c_str(), static_cast<unsigned long>(totalBytes));
}

void FileTransferServer::handleUploadAbort() {
  upload.success = false;
  upload.file = HalFile();  // close before removing the partial file
  if (!upload.destPath.empty()) {
    Storage.remove(upload.destPath.c_str());
  }
  LOG_ERR("XFER", "Upload aborted: %s", upload.destPath.c_str());
}

void FileTransferServer::sendUploadResponse() const {
  if (upload.success) {
    char msg[300];
    snprintf(msg, sizeof(msg), "File uploaded successfully: %s", basenameOf(upload.destPath.c_str()));
    server->send(200, "text/plain", msg);
  } else {
    server->send(500, "text/plain", "Upload failed");
  }
}

void FileTransferServer::handleMkdir() const {
  if (!server->hasArg("name")) {
    server->send(400, "text/plain", "Missing name");
    return;
  }
  const std::string parent = server->hasArg("path") ? server->arg("path").c_str() : "/";
  const std::string name = server->arg("name").c_str();
  char full[256];
  joinPath(full, sizeof(full), parent.c_str(), name.c_str());
  if (Storage.mkdir(full)) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", "Could not create folder");
  }
}

void FileTransferServer::handleRename() const {
  if (!server->hasArg("path") || !server->hasArg("name")) {
    server->send(400, "text/plain", "Missing path or name");
    return;
  }
  const std::string oldPath = server->arg("path").c_str();
  const std::string newName = server->arg("name").c_str();
  if (isProtectedPath(oldPath.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }
  char dir[256];
  dirnameOf(dir, sizeof(dir), oldPath.c_str());
  char newPath[256];
  joinPath(newPath, sizeof(newPath), dir, newName.c_str());
  if (Storage.rename(oldPath.c_str(), newPath)) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", "Rename failed");
  }
}

void FileTransferServer::handleMove() const {
  if (!server->hasArg("path") || !server->hasArg("dest")) {
    server->send(400, "text/plain", "Missing path or dest");
    return;
  }
  const std::string oldPath = server->arg("path").c_str();
  const std::string dest = server->arg("dest").c_str();
  if (isProtectedPath(oldPath.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }
  char newPath[256];
  joinPath(newPath, sizeof(newPath), dest.c_str(), basenameOf(oldPath.c_str()));
  if (Storage.rename(oldPath.c_str(), newPath)) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", "Move failed");
  }
}

void FileTransferServer::handleDelete() const {
  if (!server->hasArg("path")) {
    server->send(400, "text/plain", "Missing path");
    return;
  }
  const std::string path = server->arg("path").c_str();
  if (isProtectedPath(path.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }
  HalFile file = Storage.open(path.c_str());
  const bool isDir = file && file.isDirectory();
  file = HalFile();  // close before remove/rmdir
  const bool ok = isDir ? Storage.rmdir(path.c_str()) : Storage.remove(path.c_str());
  if (ok) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", isDir ? "Folder not empty or missing" : "Delete failed");
  }
}

void FileTransferServer::handleNotFound() const { server->send(404, "text/plain", "Not found"); }
