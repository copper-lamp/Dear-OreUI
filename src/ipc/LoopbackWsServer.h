#pragma once

#include "api/types/Result.h"
#include "ipc/HostDispatcher.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace dearoreui::ipc {

// RFC 6455 WebSocket frame model (cohtml-free, unit-testable).

struct WsFrame {
    bool          fin{true};
    bool          masked{false};
    std::uint8_t  opcode{0}; // 0x1 text, 0x0 continuation, 0x8 close, 0x9 ping, 0xA pong
    std::string   payload;
};

struct WsFrameParseResult {
    std::size_t consumed{0}; // bytes consumed from the buffer; 0 => need more bytes
    std::string error;       // non-empty => fatal protocol error for this connection
    WsFrame     frame;
};

// Encodes a server->client text frame (FIN + text, unmasked per RFC 6455 §5.1).
[[nodiscard]] std::string encodeWsTextFrame(std::string_view payload);

// Decodes one frame from a byte buffer (masked or unmasked; the mask flag is
// carried on WsFrame so the caller can enforce RFC 6455 §5.1 policies).
// Returns consumed == 0 when the buffer holds a partial frame. Rejects RSV
// bits, unsupported opcodes, fragmented control frames and frames above the
// 64KB limit.
[[nodiscard]] WsFrameParseResult parseWsFrame(std::string_view buffer);

// RFC 6455 §4.2.2: Sec-WebSocket-Accept = base64(sha1(key + GUID)).
[[nodiscard]] std::string computeWebSocketAccept(std::string_view secWebSocketKey);

// Extracts the "token" query parameter from a request target such as
// "/dearoreui?token=abc" (returns "" when absent).
[[nodiscard]] std::string extractTokenParam(std::string_view requestTarget);

// Random lowercase hex token (8 bytes => 16 chars by default).
[[nodiscard]] std::string generateToken(std::size_t byteCount = 8);

struct WsServerInfo {
    std::uint16_t port{0};
    std::string   token;
};

// Local-only WebSocket loopback server (Stage 8). Listens on 127.0.0.1 with a
// random port and requires a session token on every request, so it is
// unreachable from the network and unguessable by local processes.
//
// Two request flavors share one payload router (handleJsPayload -> dispatcher):
//   * WebSocket upgrade (RFC 6455)  : JS callHost request/response frames.
//   * Plain HTTP POST               : XMLHttpRequest fallback when the cohtml
//                                     build lacks WebSocket (same endpoint,
//                                     response returned in the body).
//
// Threading: one accept thread + one thread per connection (connections <= 2).
// The server thread talks to HostDispatcher only, which carries its own mutex,
// so no game-main-thread state is shared (see DearOreUI-阶段8-WebSocket回环通道).
class LoopbackWsServer {
public:
    LoopbackWsServer() = default;
    ~LoopbackWsServer();

    LoopbackWsServer(LoopbackWsServer const&)            = delete;
    LoopbackWsServer& operator=(LoopbackWsServer const&) = delete;

    // Binds 127.0.0.1:0, generates a token and starts serving. On success the
    // returned WsServerInfo carries the actual port and the session token the
    // injected JS must embed in its ws:// URL.
    [[nodiscard]] api::Result<WsServerInfo> start(
        HostDispatcher&           dispatcher,
        std::chrono::milliseconds hostCallTimeout = std::chrono::milliseconds{5000}
    );

    // Stops the listener, closes active client sockets and joins all threads.
    // Must be called before the dispatcher it routes to is destroyed.
    void stop();

    [[nodiscard]] bool isRunning() const;

private:
    struct Client {
        std::uintptr_t socket{0};
        std::thread    thread;
    };

    void acceptLoop();
    void handleClient(std::uintptr_t clientSocket);
    void wsFrameLoop(std::uintptr_t clientSocket, std::string initialBuffer);

    HostDispatcher*           mDispatcher{nullptr};
    std::chrono::milliseconds mHostCallTimeout{5000};
    std::uintptr_t            mListenSocket{0};
    std::string               mToken;
    std::thread               mAcceptThread;
    std::atomic<bool>         mRunning{false};
    std::atomic<bool>         mStopping{false};
    std::mutex                mClientsMutex;
    std::vector<Client>       mClients;
};

} // namespace dearoreui::ipc
