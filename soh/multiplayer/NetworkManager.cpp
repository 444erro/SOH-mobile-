#include "NetworkManager.h"

#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

NetworkManager& NetworkManager::Instance() {
    static NetworkManager instance;
    return instance;
}

NetworkManager::~NetworkManager() {
    Disconnect();
    if (mConnectThread.joinable()) {
        mConnectThread.join();
    }
}

bool NetworkManager::Connect(const std::string& host, uint16_t port) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mConnected) {
            return true;
        }
        if (mConnecting) {
            return false;
        }
    }

    if (mConnectThread.joinable()) {
        mConnectThread.join();
    }

    uint64_t generation;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mConnected || mConnecting) {
            return mConnected;
        }
        mConnecting = true;
        generation = ++mConnectionGeneration;
    }

    mConnectThread = std::thread([this, host, port, generation]() {
        struct addrinfo hints {};
        struct addrinfo* result = nullptr;
        int connectedSocket = -1;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        const std::string portStr = std::to_string(port);
        const int status = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);

        if (status == 0 && result != nullptr) {
            for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
                connectedSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (connectedSocket < 0) {
                    continue;
                }

                if (connect(connectedSocket, rp->ai_addr, rp->ai_addrlen) == 0) {
#if defined(_WIN32)
                    u_long nonBlocking = 1;
                    ioctlsocket(connectedSocket, FIONBIO, &nonBlocking);
#else
                    const int flags = fcntl(connectedSocket, F_GETFL, 0);
                    if (flags >= 0) {
                        fcntl(connectedSocket, F_SETFL, flags | O_NONBLOCK);
                    }
#endif
                    break;
                }

#if defined(_WIN32)
                closesocket(connectedSocket);
#else
                close(connectedSocket);
#endif
                connectedSocket = -1;
            }
        }

        if (result != nullptr) {
            freeaddrinfo(result);
        }

        std::lock_guard<std::mutex> lock(mMutex);
        if (generation != mConnectionGeneration) {
            if (connectedSocket >= 0) {
#if defined(_WIN32)
                closesocket(connectedSocket);
#else
                close(connectedSocket);
#endif
            }
            mConnecting = false;
            return;
        }

        mSocket = connectedSocket;
        mConnected = connectedSocket >= 0;
        mConnecting = false;

        if (mConnected) {
            SPDLOG_INFO("[Multiplayer] Connected to configured server");
        } else {
            SPDLOG_WARN("[Multiplayer] Could not connect to configured server");
        }
    });

    return false;
}

void NetworkManager::Disconnect() {
    std::lock_guard<std::mutex> lock(mMutex);
    ++mConnectionGeneration;

    if (mSocket >= 0) {
#if defined(_WIN32)
        closesocket(mSocket);
#else
        close(mSocket);
#endif
        mSocket = -1;
    }

    mConnected = false;
    mSendBuffer.clear();
    mReceiveBuffer.clear();
    mReceivedJson.clear();
    SPDLOG_INFO("[Multiplayer] Disconnected");
}

bool NetworkManager::IsConnected() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConnected;
}

bool NetworkManager::Send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mConnected || mSocket < 0 || data.empty()) {
        return false;
    }

    constexpr size_t maximumQueuedBytes = 64 * 1024;
    if (mSendBuffer.size() + data.size() > maximumQueuedBytes) {
        SPDLOG_ERROR("[Multiplayer] Send queue overflow");
        return false;
    }

    mSendBuffer.insert(mSendBuffer.end(), data.begin(), data.end());
    return FlushSendBufferLocked();
}

bool NetworkManager::SendJson(const std::string& json) {
    constexpr size_t maximumJsonSize = 64 * 1024;
    if (json.empty() || json.size() > maximumJsonSize) {
        if (json.size() > maximumJsonSize) {
            SPDLOG_ERROR("[Multiplayer] Refused oversized JSON packet");
        }
        return false;
    }

    std::vector<uint8_t> data(json.begin(), json.end());
    data.push_back(0);
    return Send(data);
}

bool NetworkManager::FlushSendBufferLocked() {
    while (!mSendBuffer.empty()) {
#if defined(_WIN32)
        int sent = send(mSocket, reinterpret_cast<const char*>(mSendBuffer.data()),
                        static_cast<int>(mSendBuffer.size()), 0);
#else
        ssize_t sent = send(mSocket, mSendBuffer.data(), mSendBuffer.size(), MSG_NOSIGNAL);
#endif

        if (sent > 0) {
            mSendBuffer.erase(mSendBuffer.begin(), mSendBuffer.begin() + sent);
            continue;
        }

        if (sent == 0) {
            SPDLOG_ERROR("[Multiplayer] Send failed");
#if defined(_WIN32)
            closesocket(mSocket);
#else
            close(mSocket);
#endif
            mSocket = -1;
            mConnected = false;
            return false;
        }

#if defined(_WIN32)
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return true;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
#endif

        SPDLOG_ERROR("[Multiplayer] Send failed");
#if defined(_WIN32)
        closesocket(mSocket);
#else
        close(mSocket);
#endif
        mSocket = -1;
        mConnected = false;
        mSendBuffer.clear();
        return false;
    }

    return true;
}

void NetworkManager::Update() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mConnected || mSocket < 0) {
        return;
    }

    if (!FlushSendBufferLocked()) {
        return;
    }

    uint8_t temporaryBuffer[4096];
    // ALL_CLIENT_STATE can legitimately exceed 64 KiB in populated Anchor
    // rooms because it contains every client's complete state. Keep a firm
    // bound, but large enough for the PC Anchor protocol.
    constexpr size_t maximumJsonSize = 1024 * 1024;
    constexpr size_t maximumReceiveBufferSize = 2 * maximumJsonSize;

    const auto closeConnection = [this]() {
        if (mSocket >= 0) {
#if defined(_WIN32)
            closesocket(mSocket);
#else
            close(mSocket);
#endif
            mSocket = -1;
        }
        mConnected = false;
        mSendBuffer.clear();
        mReceiveBuffer.clear();
    };

#if defined(__ANDROID__)
    // Keep network bursts from monopolizing the 20 Hz gameplay update that also
    // feeds the audio producer. Unread socket data remains queued by the kernel
    // and is consumed on the following frame.
    constexpr size_t maximumReceiveCallsPerUpdate = 4;
#else
    constexpr size_t maximumReceiveCallsPerUpdate = 16;
#endif
    for (size_t receiveCall = 0; receiveCall < maximumReceiveCallsPerUpdate; ++receiveCall) {
#if defined(_WIN32)
        int received = recv(mSocket, reinterpret_cast<char*>(temporaryBuffer), sizeof(temporaryBuffer), 0);
#else
        ssize_t received = recv(mSocket, temporaryBuffer, sizeof(temporaryBuffer), 0);
#endif

        if (received > 0) {
            if (mReceiveBuffer.size() + static_cast<size_t>(received) > maximumReceiveBufferSize) {
                SPDLOG_ERROR("[Multiplayer] Receive buffer limit exceeded");
                closeConnection();
                break;
            }
            mReceiveBuffer.insert(mReceiveBuffer.end(), temporaryBuffer, temporaryBuffer + received);
            continue;
        }

        if (received == 0) {
#if defined(_WIN32)
            closesocket(mSocket);
#else
            close(mSocket);
#endif
            mSocket = -1;
            mConnected = false;
            SPDLOG_WARN("[Multiplayer] Server closed the connection");
            break;
        }

#if defined(_WIN32)
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
#if defined(_WIN32)
            closesocket(mSocket);
#else
            close(mSocket);
#endif
            mSocket = -1;
            mConnected = false;
            SPDLOG_ERROR("[Multiplayer] Receive failed");
        }
        break;
    }

    constexpr size_t maximumQueuedJsonPackets = 16;
    while (true) {
        const auto delimiter = std::find(mReceiveBuffer.begin(), mReceiveBuffer.end(), 0);
        if (delimiter == mReceiveBuffer.end()) {
            if (mReceiveBuffer.size() > maximumJsonSize) {
                SPDLOG_ERROR("[Multiplayer] Anchor JSON packet too large");
                closeConnection();
            }
            break;
        }

        if (static_cast<size_t>(delimiter - mReceiveBuffer.begin()) > maximumJsonSize) {
            SPDLOG_ERROR("[Multiplayer] Anchor JSON packet too large");
            closeConnection();
            break;
        }
        if (mReceivedJson.size() >= maximumQueuedJsonPackets) {
            // Keep memory bounded during a valid server burst without causing a reconnect loop.
            mReceivedJson.pop_front();
            SPDLOG_WARN("[Multiplayer] Dropped stale queued JSON packet");
        }
        mReceivedJson.emplace_back(mReceiveBuffer.begin(), delimiter);
        mReceiveBuffer.erase(mReceiveBuffer.begin(), delimiter + 1);
    }
}

bool NetworkManager::IsConnecting() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConnecting;
}

bool NetworkManager::PollReceivedJson(std::string& json) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mReceivedJson.empty()) {
        return false;
    }

    json = std::move(mReceivedJson.front());
    mReceivedJson.pop_front();
    return true;
}
