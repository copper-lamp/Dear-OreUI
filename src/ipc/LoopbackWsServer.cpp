#include "ipc/LoopbackWsServer.h"

#include "diagnostic/DiagnosticLogger.h"
#include "ipc/JsPayloadHandler.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

namespace dearoreui::ipc {

namespace {

constexpr std::size_t kMaxFrameBytes = 64 * 1024;

// ---------------------------------------------------------------------------
// Winsock lifecycle (ref-counted: the mod DLL and the unit tests may each
// start/stop their own server).
// ---------------------------------------------------------------------------

std::mutex gWsaMutex;
int        gWsaRefs{0};

bool ensureWsaStarted() {
    std::lock_guard lock(gWsaMutex);
    if (gWsaRefs == 0) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            return false;
        }
    }
    ++gWsaRefs;
    return true;
}

void releaseWsa() {
    std::lock_guard lock(gWsaMutex);
    if (--gWsaRefs <= 0) {
        gWsaRefs = 0;
        WSACleanup();
    }
}

// ---------------------------------------------------------------------------
// SHA-1 (compact, public-domain style) used for the RFC 6455 handshake.
// ---------------------------------------------------------------------------

struct Sha1Ctx {
    std::uint32_t h[5]{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::uint64_t byteCount{0};
    std::uint8_t  block[64]{};
    std::size_t   blockLen{0};
};

void sha1Transform(Sha1Ctx& ctx, std::uint8_t const* block) {
    std::uint32_t w[80]{};
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t{block[i * 4]} << 24) | (std::uint32_t{block[i * 4 + 1]} << 16)
             | (std::uint32_t{block[i * 4 + 2]} << 8) | std::uint32_t{block[i * 4 + 3]};
    }
    for (int i = 16; i < 80; ++i) {
        std::uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
        w[i] = (v << 1) | (v >> 31);
    }
    std::uint32_t a = ctx.h[0], b = ctx.h[1], c = ctx.h[2], d = ctx.h[3], e = ctx.h[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f{};
        std::uint32_t k{};
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        std::uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d;
        d = c;
        c = (b << 30) | (b >> 2);
        b = a;
        a = tmp;
    }
    ctx.h[0] += a;
    ctx.h[1] += b;
    ctx.h[2] += c;
    ctx.h[3] += d;
    ctx.h[4] += e;
}

void sha1Update(Sha1Ctx& ctx, std::uint8_t const* data, std::size_t len) {
    ctx.byteCount += len;
    while (len > 0) {
        std::size_t take = 64 - ctx.blockLen;
        if (take > len) take = len;
        std::memcpy(ctx.block + ctx.blockLen, data, take);
        ctx.blockLen += take;
        data += take;
        len -= take;
        if (ctx.blockLen == 64) {
            sha1Transform(ctx, ctx.block);
            ctx.blockLen = 0;
        }
    }
}

void sha1Final(Sha1Ctx& ctx, std::uint8_t digest[20]) {
    std::uint64_t const bits = ctx.byteCount * 8; // original message length in bits
    ctx.block[ctx.blockLen++] = 0x80;             // append the 0x80 padding bit
    if (ctx.blockLen > 56) {
        std::memset(ctx.block + ctx.blockLen, 0, 64 - ctx.blockLen);
        sha1Transform(ctx, ctx.block);
        ctx.blockLen = 0;
    }
    std::memset(ctx.block + ctx.blockLen, 0, 56 - ctx.blockLen);
    for (int i = 0; i < 8; ++i) {
        ctx.block[56 + i] = static_cast<std::uint8_t>((bits >> ((7 - i) * 8)) & 0xFF);
    }
    sha1Transform(ctx, ctx.block);
    for (int i = 0; i < 5; ++i) {
        digest[i * 4]     = static_cast<std::uint8_t>((ctx.h[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((ctx.h[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((ctx.h[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(ctx.h[i] & 0xFF);
    }
}

// ---------------------------------------------------------------------------
// Base64 (RFC 4648 §4, with padding).
// ---------------------------------------------------------------------------

[[nodiscard]] std::string base64Encode(std::uint8_t const* data, std::size_t len) {
    static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string           out;
    out.reserve(((len + 2) / 3) * 4);
    for (std::size_t i = 0; i < len; i += 3) {
        std::uint32_t v = std::uint32_t{data[i]} << 16;
        if (i + 1 < len) v |= std::uint32_t{data[i + 1]} << 8;
        if (i + 2 < len) v |= std::uint32_t{data[i + 2]};
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? kTable[(v >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? kTable[v & 0x3F] : '=');
    }
    return out;
}

// ---------------------------------------------------------------------------
// Small string helpers for HTTP request parsing.
// ---------------------------------------------------------------------------

[[nodiscard]] std::string toLowerAscii(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

[[nodiscard]] std::string trimCopy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end   = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r')) {
        ++begin;
    }
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r')) {
        --end;
    }
    return std::string{value.substr(begin, end - begin)};
}

[[nodiscard]] long parseContentLength(std::string_view head) {
    auto lineEnd = head.find("\r\n");
    if (lineEnd == std::string_view::npos) return -1;
    std::istringstream stream{std::string{head.substr(lineEnd + 2)}};
    std::string        line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (toLowerAscii(std::string_view{line}.substr(0, colon)) == "content-length") {
            auto value = trimCopy(std::string_view{line}.substr(colon + 1));
            try {
                return std::stol(value);
            } catch (...) {
                return -1;
            }
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Socket send helpers.
// ---------------------------------------------------------------------------

bool sendAll(SOCKET sock, std::string_view data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        int n = ::send(sock, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

void sendSimpleHttp(SOCKET sock, int status, std::string_view body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " " << (status == 200 ? "OK" : (status == 403 ? "Forbidden" : "Bad Request"))
             << "\r\nContent-Type: application/json\r\nContent-Length: " << body.size()
             << "\r\nConnection: close\r\n\r\n";
    static_cast<void>(sendAll(sock, response.str()));
    static_cast<void>(sendAll(sock, body));
}

[[nodiscard]] std::string encodeControlFrame(std::uint8_t opcode, std::string_view payload) {
    std::string out;
    out.push_back(static_cast<char>(0x80 | opcode));
    out.push_back(static_cast<char>(payload.size() & 0x7F));
    out.append(payload.data(), payload.size());
    return out;
}

[[nodiscard]] std::string encodeCloseFrame(std::uint16_t code) {
    std::string payload;
    payload.push_back(static_cast<char>((code >> 8) & 0xFF));
    payload.push_back(static_cast<char>(code & 0xFF));
    return encodeControlFrame(0x8, payload);
}

} // namespace

// ---------------------------------------------------------------------------
// Frame codec (public, unit-testable).
// ---------------------------------------------------------------------------

std::string encodeWsTextFrame(std::string_view payload) {
    std::string out;
    out.reserve(payload.size() + 10);
    out.push_back(static_cast<char>(0x81)); // FIN + text opcode
    std::uint64_t const len = payload.size();
    if (len <= 125) {
        out.push_back(static_cast<char>(len));
    } else if (len <= 0xFFFF) {
        out.push_back(126);
        out.push_back(static_cast<char>((len >> 8) & 0xFF));
        out.push_back(static_cast<char>(len & 0xFF));
    } else {
        out.push_back(127);
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }
    out.append(payload.data(), payload.size());
    return out;
}

WsFrameParseResult parseWsFrame(std::string_view buffer) {
    WsFrameParseResult result;
    if (buffer.size() < 2) {
        return result; // partial header
    }
    auto const* b = reinterpret_cast<std::uint8_t const*>(buffer.data());

    std::uint8_t const first  = b[0];
    std::uint8_t const second = b[1];
    if ((first & 0x70) != 0) {
        result.error = "rsv bits set";
        return result;
    }
    bool const        fin    = (first & 0x80) != 0;
    std::uint8_t const opcode = first & 0x0F;
    bool const        masked = (second & 0x80) != 0;
    std::uint64_t      len    = second & 0x7F;
    std::size_t        offset = 2;

    if (opcode >= 0x8) { // control frame: FIN required, payload <= 125
        if (!fin || len > 125) {
            result.error = "invalid control frame";
            return result;
        }
    }

    if (len == 126) {
        if (buffer.size() < 4) return result;
        len = (std::uint64_t{b[2]} << 8) | b[3];
        offset = 4;
    } else if (len == 127) {
        if (buffer.size() < 10) return result;
        len = 0;
        for (int i = 0; i < 8; ++i) {
            len = (len << 8) | b[2 + i];
        }
        offset = 10;
    }
    if (len > kMaxFrameBytes) {
        result.error = "frame too large";
        return result;
    }

    std::uint8_t maskKey[4]{};
    if (masked) {
        if (buffer.size() < offset + 4) return result;
        std::memcpy(maskKey, b + offset, 4);
        offset += 4;
    }

    if (buffer.size() < offset + len) {
        return result; // partial payload
    }

    std::string payload;
    payload.resize(static_cast<std::size_t>(len));
    if (len > 0) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(len); ++i) {
            payload[i] = static_cast<char>(b[offset + i] ^ (masked ? maskKey[i & 3] : 0));
        }
    }

    switch (opcode) {
    case 0x0: // continuation
    case 0x1: // text
    case 0x8: // close
    case 0x9: // ping
    case 0xA: // pong
        break;
    default:
        result.error = "unsupported opcode";
        return result;
    }

    result.consumed    = offset + static_cast<std::size_t>(len);
    result.frame.fin    = fin;
    result.frame.masked = masked;
    result.frame.opcode = opcode;
    result.frame.payload = std::move(payload);
    return result;
}

std::string computeWebSocketAccept(std::string_view secWebSocketKey) {
    std::string input{secWebSocketKey};
    input += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    Sha1Ctx sha;
    sha1Update(sha, reinterpret_cast<std::uint8_t const*>(input.data()), input.size());
    std::uint8_t digest[20]{};
    sha1Final(sha, digest);
    return base64Encode(digest, 20);
}

std::string extractTokenParam(std::string_view requestTarget) {
    auto query = requestTarget.find('?');
    if (query == std::string_view::npos) {
        return {};
    }
    std::string_view rest = requestTarget.substr(query + 1);
    std::size_t      start = 0;
    while (start <= rest.size()) {
        auto end = rest.find('&', start);
        if (end == std::string_view::npos) end = rest.size();
        auto part = rest.substr(start, end - start);
        if (part.rfind("token=", 0) == 0) {
            return std::string{part.substr(6)};
        }
        if (end == rest.size()) break;
        start = end + 1;
    }
    return {};
}

std::string generateToken(std::size_t byteCount) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::random_device    rd;
    std::string           token;
    token.reserve(byteCount * 2);
    for (std::size_t i = 0; i < byteCount; ++i) {
        auto byte = static_cast<std::uint8_t>(rd());
        token.push_back(kHex[(byte >> 4) & 0xF]);
        token.push_back(kHex[byte & 0xF]);
    }
    return token;
}

// ---------------------------------------------------------------------------
// LoopbackWsServer
// ---------------------------------------------------------------------------

LoopbackWsServer::~LoopbackWsServer() { stop(); }

bool LoopbackWsServer::isRunning() const { return mRunning; }

api::Result<WsServerInfo> LoopbackWsServer::start(
    HostDispatcher&           dispatcher,
    std::chrono::milliseconds hostCallTimeout
) {
    if (mRunning) {
        return api::Error{api::ErrorCode::InvalidState, "ws loopback server is already running"};
    }
    if (!ensureWsaStarted()) {
        return api::Error{api::ErrorCode::InternalError, "WSAStartup failed"};
    }

    SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        releaseWsa();
        return api::Error{api::ErrorCode::InternalError, "socket() failed"};
    }

    SOCKADDR_IN addr{};
    addr.sin_family        = AF_INET;
    addr.sin_addr.s_addr   = htonl(INADDR_LOOPBACK);
    addr.sin_port          = 0; // OS-assigned random port
    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(listenSock);
        releaseWsa();
        return api::Error{api::ErrorCode::InternalError, "bind() failed"};
    }
    if (::listen(listenSock, 4) == SOCKET_ERROR) {
        ::closesocket(listenSock);
        releaseWsa();
        return api::Error{api::ErrorCode::InternalError, "listen() failed"};
    }
    int nameLen = sizeof(addr);
    if (::getsockname(listenSock, reinterpret_cast<sockaddr*>(&addr), &nameLen) == SOCKET_ERROR) {
        ::closesocket(listenSock);
        releaseWsa();
        return api::Error{api::ErrorCode::InternalError, "getsockname() failed"};
    }
    std::uint16_t const port = ntohs(addr.sin_port);

    mDispatcher      = &dispatcher;
    mHostCallTimeout = hostCallTimeout;
    mToken           = generateToken();
    mListenSocket    = static_cast<std::uintptr_t>(listenSock);
    mStopping        = false;
    mRunning         = true;
    mAcceptThread    = std::thread([this]() { acceptLoop(); });

    diagnostic::globalLogger()
        .info("ws", "started")
        .withField("port", std::to_string(port))
        .withField("token", mToken)
        .emit();

    return WsServerInfo{port, mToken};
}

void LoopbackWsServer::stop() {
    if (!mRunning.exchange(false)) {
        return;
    }
    mStopping = true;

    if (mListenSocket != 0) {
        ::closesocket(static_cast<SOCKET>(mListenSocket));
        mListenSocket = 0;
    }
    {
        std::lock_guard lock(mClientsMutex);
        for (auto& client : mClients) {
            ::closesocket(static_cast<SOCKET>(client.socket));
        }
    }

    if (mAcceptThread.joinable()) {
        mAcceptThread.join();
    }

    std::vector<Client> clients;
    {
        std::lock_guard lock(mClientsMutex);
        clients.swap(mClients);
    }
    for (auto& client : clients) {
        if (client.thread.joinable()) {
            client.thread.join();
        }
    }

    mDispatcher = nullptr;
    releaseWsa();

    diagnostic::globalLogger().info("ws", "stopped").emit();
}

void LoopbackWsServer::acceptLoop() {
    while (!mStopping) {
        SOCKET client = ::accept(static_cast<SOCKET>(mListenSocket), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (mStopping) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (mStopping) {
            ::closesocket(client);
            break;
        }
        std::thread worker([this, client]() { handleClient(static_cast<std::uintptr_t>(client)); });
        {
            std::lock_guard lock(mClientsMutex);
            mClients.push_back(Client{static_cast<std::uintptr_t>(client), std::move(worker)});
        }
    }
    diagnostic::globalLogger().info("ws", "accept_loop_exited").emit();
}

void LoopbackWsServer::handleClient(std::uintptr_t clientSocket) {
    SOCKET const   sock   = static_cast<SOCKET>(clientSocket);
    auto&          logger = diagnostic::globalLogger();
    logger.info("ws", "client_connected").withField("socket", std::to_string(clientSocket)).emit();

    auto closeClient = [&]() {
        ::closesocket(sock);
        logger.info("ws", "client_disconnected").withField("socket", std::to_string(clientSocket)).emit();
    };

    // 1. Read the HTTP request head (terminated by CRLFCRLF).
    std::string head;
    std::string extra;
    char        buf[4096];
    while (!mStopping) {
        auto headerEnd = head.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            extra = head.substr(headerEnd + 4);
            head  = head.substr(0, headerEnd + 4);
            break;
        }
        int n = ::recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) {
            closeClient();
            return;
        }
        head.append(buf, static_cast<std::size_t>(n));
    }
    if (mStopping) {
        closeClient();
        return;
    }

    // 2. Parse the request line.
    auto lineEnd = head.find("\r\n");
    if (lineEnd == std::string::npos) {
        sendSimpleHttp(sock, 400, "bad request");
        closeClient();
        return;
    }
    std::istringstream lineStream{head.substr(0, lineEnd)};
    std::string        method, target, version;
    lineStream >> method >> target >> version;

    // 3. Token gate on every request.
    auto const token = extractTokenParam(target);
    if (token.empty() || token != mToken) {
        logger.warning("ws", "error").withMessage("handshake rejected: bad token").emit();
        sendSimpleHttp(sock, 403, "forbidden");
        closeClient();
        return;
    }

    // 4. Decide: WebSocket upgrade vs plain HTTP POST (XHR fallback).
    bool        isUpgrade = false;
    std::string secKey;
    {
        std::istringstream headerStream{head.substr(lineEnd + 2)};
        std::string        line;
        while (std::getline(headerStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto colon = line.find(':');
            if (colon == std::string::npos) continue;
            auto name  = toLowerAscii(std::string_view{line}.substr(0, colon));
            auto value = trimCopy(std::string_view{line}.substr(colon + 1));
            if (name == "upgrade" && value == "websocket") isUpgrade = true;
            if (name == "sec-websocket-key") secKey = value;
        }
    }

    if (isUpgrade) {
        if (secKey.empty()) {
            sendSimpleHttp(sock, 400, "missing sec-websocket-key");
            closeClient();
            return;
        }
        auto accept = computeWebSocketAccept(secKey);
        std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: "
            + accept + "\r\n\r\n";
        if (!sendAll(sock, response)) {
            closeClient();
            return;
        }
        wsFrameLoop(clientSocket, std::move(extra));
    } else {
        long contentLength = parseContentLength(head);
        if (contentLength < 0) {
            sendSimpleHttp(sock, 400, "missing content-length");
            closeClient();
            return;
        }
        std::string body = std::move(extra);
        while (body.size() < static_cast<std::size_t>(contentLength) && !mStopping) {
            int n = ::recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
        }
        body = body.substr(0, static_cast<std::size_t>(contentLength));

        auto const start = std::chrono::steady_clock::now();
        auto       result = handleJsPayload(*mDispatcher, body, mHostCallTimeout);
        auto const elapsed = std::chrono::steady_clock::now() - start;
        logger.info("ws", "request")
            .withField("mode", "http")
            .withField("payload_size", std::to_string(body.size()))
            .withField(
                "elapsed_ms",
                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count())
            )
            .emit();

        sendSimpleHttp(sock, 200, result);
        closeClient();
    }
}

void LoopbackWsServer::wsFrameLoop(std::uintptr_t clientSocket, std::string initialBuffer) {
    SOCKET const sock   = static_cast<SOCKET>(clientSocket);
    auto&        logger = diagnostic::globalLogger();
    std::string  readBuf = std::move(initialBuffer);
    bool         open    = true;
    std::string  fragment;
    bool         hasFragment = false;
    char         buf[4096];

    while (open && !mStopping) {
        while (true) {
            auto parsed = parseWsFrame(readBuf);
            if (!parsed.error.empty()) {
                logger.warning("ws", "error").withMessage(parsed.error).emit();
                static_cast<void>(sendAll(sock, encodeCloseFrame(1002)));
                open = false;
                break;
            }
            if (parsed.consumed == 0) break; // need more bytes
            readBuf.erase(0, parsed.consumed);
            auto const& frame = parsed.frame;

            if (!frame.masked) { // RFC 6455 §5.1: client frames MUST be masked
                logger.warning("ws", "error").withMessage("client frame not masked").emit();
                static_cast<void>(sendAll(sock, encodeCloseFrame(1002)));
                open = false;
                break;
            }

            if (frame.opcode == 0x8) { // close
                static_cast<void>(sendAll(sock, encodeCloseFrame(1000)));
                open = false;
                break;
            }
            if (frame.opcode == 0x9) { // ping
                static_cast<void>(sendAll(sock, encodeControlFrame(0xA, frame.payload)));
                continue;
            }
            if (frame.opcode == 0xA) { // pong
                continue;
            }

            if (frame.opcode == 0x1 || frame.opcode == 0x0) {
                if (frame.opcode == 0x1 && hasFragment) {
                    logger.warning("ws", "error").withMessage("new text frame while fragment open").emit();
                    static_cast<void>(sendAll(sock, encodeCloseFrame(1002)));
                    open = false;
                    break;
                }
                if (frame.opcode == 0x0 && !hasFragment) {
                    logger.warning("ws", "error").withMessage("continuation frame without a start").emit();
                    static_cast<void>(sendAll(sock, encodeCloseFrame(1002)));
                    open = false;
                    break;
                }
                fragment += frame.payload;
                if (fragment.size() > kMaxFrameBytes) {
                    logger.warning("ws", "error").withMessage("message exceeds max frame size").emit();
                    static_cast<void>(sendAll(sock, encodeCloseFrame(1009)));
                    open = false;
                    break;
                }
                if (frame.fin) {
                    auto const start = std::chrono::steady_clock::now();
                    auto       result = handleJsPayload(*mDispatcher, fragment, mHostCallTimeout);
                    auto const elapsed = std::chrono::steady_clock::now() - start;
                    logger.info("ws", "request")
                        .withField("mode", "ws")
                        .withField("payload_size", std::to_string(fragment.size()))
                        .withField(
                            "elapsed_ms",
                            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count())
                        )
                        .emit();
                    if (!result.empty()) {
                        if (!sendAll(sock, encodeWsTextFrame(result))) {
                            open = false;
                        }
                    }
                    fragment.clear();
                    hasFragment = false;
                } else {
                    hasFragment = true;
                }
                continue;
            }
        }
        if (!open) break;
        int n = ::recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        readBuf.append(buf, static_cast<std::size_t>(n));
    }

    logger.info("ws", "client_disconnected").withField("socket", std::to_string(clientSocket)).emit();
    ::closesocket(sock);
}

} // namespace dearoreui::ipc
