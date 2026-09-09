#ifdef ENABLE_REMOTE_CONTROL

#include "Network.h"
#include <spdlog/spdlog.h>
#include <libultraship/libultraship.h>
#include <chrono>
#include <cstring>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// MARK: - Public

void Network::Enable(const char* host, uint16_t port) {
    if (isEnabled) {
        return;
    }

    networkHost = host != nullptr ? host : "127.0.0.1";
    networkPort = port;
    isEnabled = true;

    // First check if there is a thread running, if so, join it
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    receiveThread = std::thread(&Network::ReceiveFromServer, this);
}

void Network::Disable() {
    if (!isEnabled) {
        return;
    }

    isEnabled = false;
    {
        std::lock_guard<std::mutex> lock(networkMutex);
        if (networkSocket >= 0) {
#if defined(_WIN32)
            closesocket(networkSocket);
#else
            shutdown(networkSocket, SHUT_RDWR);
            close(networkSocket);
#endif
            networkSocket = -1;
        }
    }
    if (receiveThread.joinable()) receiveThread.join();
}

void Network::OnIncomingData(char payload[512]) {
}

void Network::OnIncomingJson(nlohmann::json payload) {
}

void Network::OnConnected() {
}

void Network::OnDisconnected() {
}

void Network::SendDataToRemote(const char* payload) {
    if (payload == nullptr) return;
    std::lock_guard<std::mutex> lock(networkMutex);
    if (networkSocket < 0 || !isConnected) return;
    const char* cursor = payload;
    size_t remaining = strlen(payload) + 1;
    while (remaining > 0) {
        const int sent = send(networkSocket, cursor, remaining, 0);
        if (sent <= 0) break;
        cursor += sent;
        remaining -= sent;
    }
}

void Network::SendJsonToRemote(nlohmann::json payload) {
    SendDataToRemote(payload.dump().c_str());
}

// MARK: - Private

void Network::ReceiveFromServer() {
    while (isEnabled) {
        struct addrinfo hints {};
        struct addrinfo* addresses = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        int socketFd = -1;
        if (getaddrinfo(networkHost.c_str(), std::to_string(networkPort).c_str(), &hints, &addresses) == 0) {
            for (auto* address = addresses; address != nullptr && isEnabled; address = address->ai_next) {
                socketFd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
                if (socketFd >= 0 && connect(socketFd, address->ai_addr, address->ai_addrlen) == 0) break;
                if (socketFd >= 0) {
#if defined(_WIN32)
                    closesocket(socketFd);
#else
                    close(socketFd);
#endif
                }
                socketFd = -1;
            }
            freeaddrinfo(addresses);
        }
        if (socketFd < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(networkMutex);
            networkSocket = socketFd;
            isConnected = true;
        }
        OnConnected();

        while (isEnabled && isConnected) {
            char remoteDataReceived[512];
            memset(remoteDataReceived, 0, sizeof(remoteDataReceived));
            const int len = recv(socketFd, remoteDataReceived, sizeof(remoteDataReceived) - 1, 0);
            if (len <= 0) break;
            HandleRemoteData(remoteDataReceived);
            receivedData.append(remoteDataReceived, static_cast<size_t>(len));

            // Proess all complete packets
            size_t delimiterPos = receivedData.find('\0');
            while (delimiterPos != std::string::npos) {
                // Extract the complete packet until the delimiter
                std::string packet = receivedData.substr(0, delimiterPos);
                // Remove the packet (including the delimiter) from the received data
                receivedData.erase(0, delimiterPos + 1);
                HandleRemoteJson(packet);
                // Find the next delimiter
                delimiterPos = receivedData.find('\0');
            }
        }

        if (isConnected) {
            {
                std::lock_guard<std::mutex> lock(networkMutex);
                if (networkSocket >= 0) {
#if defined(_WIN32)
                    closesocket(networkSocket);
#else
                    close(networkSocket);
#endif
                    networkSocket = -1;
                }
            }
            isConnected = false;
            OnDisconnected();
        }
    }
}

void Network::HandleRemoteData(char payload[512]) {
    OnIncomingData(payload);
}

void Network::HandleRemoteJson(std::string payload) {
    SPDLOG_TRACE("[Network] Received JSON payload");
    nlohmann::json jsonPayload;
    try {
        jsonPayload = nlohmann::json::parse(payload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Failed to parse received JSON: {}", e.what());
        return;
    }

    OnIncomingJson(jsonPayload);
}

#endif // ENABLE_REMOTE_CONTROL
