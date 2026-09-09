#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class NetworkManager {
public:
    static NetworkManager& Instance();

    bool Connect(const std::string& host, uint16_t port);
    void Disconnect();

    bool IsConnected() const;
    bool IsConnecting() const;

    bool Send(const std::vector<uint8_t>& data);
    bool SendJson(const std::string& json);
    void Update();
    bool PollReceivedJson(std::string& json);

private:
    NetworkManager() = default;
    ~NetworkManager();
    bool FlushSendBufferLocked();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

private:
    int mSocket = -1;
    bool mConnected = false;
    bool mConnecting = false;
    uint64_t mConnectionGeneration = 0;
    std::thread mConnectThread;
    std::vector<uint8_t> mSendBuffer;
    std::vector<uint8_t> mReceiveBuffer;
    std::deque<std::string> mReceivedJson;
    mutable std::mutex mMutex;
};
