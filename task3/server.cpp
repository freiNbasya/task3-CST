#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

std::mutex consoleMutex;
std::mutex clientsMutex;

struct Client {
    SOCKET socket;
    int room;
};

std::vector<Client> clients;

struct Message {
    std::string message;
    SOCKET senderSocket;
    int room;
};
std::mutex messageQueueMutex;
std::condition_variable messageAvailableCondition;
std::queue<Message> messageQueue;

void addMessageToQueue(const Message& message) {
    std::lock_guard<std::mutex> lock(messageQueueMutex);
    messageQueue.push(message);
    messageAvailableCondition.notify_one();
}

void broadcastMessage(const std::string& message, SOCKET senderSocket, int room) {
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::cout << "Client " << senderSocket << ": " << message << std::endl;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const Client& client : clients) {
            if (client.socket != senderSocket && client.room == room) {
                send(client.socket, message.c_str(), message.size() + 1, 0);
            }
        }
    }
}

void broadcastMessages() {
    while (true) {
        std::unique_lock<std::mutex> lock(messageQueueMutex);
        messageAvailableCondition.wait(lock, [] { return !messageQueue.empty(); });
        while (!messageQueue.empty()) {
            Message message = messageQueue.front();
            messageQueue.pop();
            broadcastMessage(message.message, message.senderSocket, message.room);
        }
    }
}

void addClient(SOCKET clientSocket, int room) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.push_back({ clientSocket, room });
}

void handleClient(SOCKET clientSocket) {
    char buffer[4096];
    int room = -1;
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        room = std::stoi(buffer);
        std::cout << "Client " << clientSocket << " connected to room " << room << std::endl;
    }

    addClient(clientSocket, room);

    while (true) {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Client " << clientSocket << " disconnected from room " << room << std::endl;
            break;
        }

        buffer[bytesReceived] = '\0';
        std::string message(buffer);

        if (message.substr(0, 7) == "REJOIN_") {
            int newRoom = std::stoi(message.substr(7));
            if (newRoom >= 1 && newRoom <= 3) {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Client " << clientSocket << " rejoined room " << newRoom << std::endl;
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    for (size_t i = 0; i < clients.size(); ++i) {
                        room = newRoom;
                        if (clients[i].socket == clientSocket) clients[i].room = newRoom;
                    }
                }
            }
        }
        else {
            addMessageToQueue({ message, clientSocket, room });
        }
    }
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) ==
        SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 8080...\n";
    std::thread broadcastThread(broadcastMessages);
    broadcastThread.detach();

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed.\n";
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Client " << clientSocket << " connected.\n";
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
