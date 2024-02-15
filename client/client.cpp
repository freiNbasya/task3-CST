#include <iostream>
#include <thread>
#include <string>
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

void receiveMessages(SOCKET clientSocket) {
    char buffer[4096];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Server disconnected.\n";
            break;
        }
        buffer[bytesReceived] = '\0';
        std::cout << "Client: " << buffer << std::endl;
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    InetPton(AF_INET, L"127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(8080);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";
    std::string roomID;
    std::cout << "Enter room ID (1, 2, or 3): ";
    std::getline(std::cin, roomID);
    send(clientSocket, roomID.c_str(), roomID.size() + 1, 0);
    std::thread receiveThread(receiveMessages, clientSocket);
    std::string message;
    while (true) {
        std::getline(std::cin, message);
        send(clientSocket, message.c_str(), message.size() + 1, 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}