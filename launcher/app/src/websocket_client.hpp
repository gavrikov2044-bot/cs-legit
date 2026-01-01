/*
 * WebSocket Client for Real-Time Updates
 * Connects to server for instant status changes
 */

#pragma once
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace WebSocketClient {
    
    // WebSocket state
    std::atomic<bool> g_connected{false};
    std::atomic<bool> g_shouldStop{false};
    SOCKET g_socket = INVALID_SOCKET;
    std::thread g_wsThread;
    
    // Callbacks
    std::function<void(const std::string&, const std::string&)> g_onGameStatusUpdate = nullptr;
    std::function<void(const std::string&, const std::string&)> g_onNewBuild = nullptr;
    
    // Base64 encoding (for WebSocket handshake)
    std::string Base64Encode(const std::string& input) {
        static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        int val = 0, valb = -6;
        
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                output.push_back(table[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        
        if (valb > -6) output.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
        while (output.size() % 4) output.push_back('=');
        
        return output;
    }
    
    // Simple JSON parser for WebSocket messages
    std::string ExtractJsonField(const std::string& json, const std::string& field) {
        std::string search = "\"" + field + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos += search.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        
        size_t end = pos;
        bool inString = (pos > 0 && json[pos - 1] == '"');
        
        if (inString) {
            while (end < json.length() && json[end] != '"') end++;
        } else {
            while (end < json.length() && json[end] != ',' && json[end] != '}') end++;
        }
        
        return json.substr(pos, end - pos);
    }
    
    // WebSocket handshake
    bool PerformHandshake(SOCKET sock) {
        // Generate random key (simplified)
        char key[17] = "x3JJHMbDL1EzLkh9";
        std::string keyB64 = Base64Encode(std::string(key, 16));
        
        // Build handshake request
        std::string request =
            "GET / HTTP/1.1\r\n"
            "Host: " SERVER_HOST_HEADER "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + keyB64 + "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        
        // Send handshake
        int sent = send(sock, request.c_str(), (int)request.length(), 0);
        if (sent <= 0) return false;
        
        // Receive response
        char buffer[1024];
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) return false;
        
        buffer[received] = '\0';
        std::string response(buffer);
        
        // Check for "101 Switching Protocols"
        return response.find("101") != std::string::npos;
    }
    
    // Decode WebSocket frame
    std::string DecodeFrame(const char* data, int length) {
        if (length < 2) return "";
        
        bool fin = (data[0] & 0x80) != 0;
        int opcode = data[0] & 0x0F;
        bool masked = (data[1] & 0x80) != 0;
        uint64_t payloadLen = data[1] & 0x7F;
        
        int pos = 2;
        
        if (payloadLen == 126) {
            if (length < 4) return "";
            payloadLen = (data[2] << 8) | data[3];
            pos = 4;
        } else if (payloadLen == 127) {
            if (length < 10) return "";
            payloadLen = 0;
            for (int i = 0; i < 8; i++) {
                payloadLen = (payloadLen << 8) | data[2 + i];
            }
            pos = 10;
        }
        
        if (pos + payloadLen > length) return "";
        
        std::string payload(data + pos, (size_t)payloadLen);
        return payload;
    }
    
    // WebSocket receive loop
    void ReceiveLoop() {
        char buffer[4096];
        
        while (!g_shouldStop && g_connected) {
            int received = recv(g_socket, buffer, sizeof(buffer), 0);
            
            if (received <= 0) {
                g_connected = false;
                break;
            }
            
            // Decode WebSocket frame
            std::string message = DecodeFrame(buffer, received);
            if (message.empty()) continue;
            
            // Parse JSON message
            std::string type = ExtractJsonField(message, "type");
            
            if (type == "game_status_update") {
                std::string gameId = ExtractJsonField(message, "gameId");
                std::string status = ExtractJsonField(message, "status");
                
                if (g_onGameStatusUpdate) {
                    g_onGameStatusUpdate(gameId, status);
                }
            }
            else if (type == "new_build_available") {
                std::string gameId = ExtractJsonField(message, "gameId");
                std::string version = ExtractJsonField(message, "version");
                
                if (g_onNewBuild) {
                    g_onNewBuild(gameId, version);
                }
            }
        }
        
        if (g_socket != INVALID_SOCKET) {
            closesocket(g_socket);
            g_socket = INVALID_SOCKET;
        }
    }
    
    // Connect to WebSocket server
    bool Connect(const char* host, int port) {
        if (g_connected) return true;
        
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
        
        // Create socket
        g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_socket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }
        
        // Set timeout
        int timeout = 5000; // 5 seconds
        setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(g_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        // Connect
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, host, &serverAddr.sin_addr);
        
        if (connect(g_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(g_socket);
            g_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
        
        // Perform WebSocket handshake
        if (!PerformHandshake(g_socket)) {
            closesocket(g_socket);
            g_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
        
        g_connected = true;
        g_shouldStop = false;
        
        // Start receive thread
        g_wsThread = std::thread(ReceiveLoop);
        
        return true;
    }
    
    // Disconnect from WebSocket server
    void Disconnect() {
        g_shouldStop = true;
        g_connected = false;
        
        if (g_socket != INVALID_SOCKET) {
            closesocket(g_socket);
            g_socket = INVALID_SOCKET;
        }
        
        if (g_wsThread.joinable()) {
            g_wsThread.join();
        }
        
        WSACleanup();
    }
    
    // Set callback for game status updates
    void OnGameStatusUpdate(std::function<void(const std::string&, const std::string&)> callback) {
        g_onGameStatusUpdate = callback;
    }
    
    // Set callback for new builds
    void OnNewBuild(std::function<void(const std::string&, const std::string&)> callback) {
        g_onNewBuild = callback;
    }
    
    // Check if connected
    bool IsConnected() {
        return g_connected;
    }
}

