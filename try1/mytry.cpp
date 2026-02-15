#include <iostream>
#include <stdexcept>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>

std::map<std::string, std::string> gdatabase;
std::string filename;


void savedb() {
    std::ofstream file(filename.c_str());
    if (file) {
        for (std::map<std::string, std::string>::iterator it = gdatabase.begin(); it != gdatabase.end();++it) {
            file << it->first << " " << it -> second << "\n";
        }
        file.close();
    }
}

void loaddb() {
    std::ifstream file(filename.c_str());
    if (file) {
        std::string line, key, value;

        while (getline(file, line)) {
            std::stringstream ss(line);
            if (ss >> key >> value)
                gdatabase[key] = value;
        }
        file.close();
    }
}

void handle_sigint(int) {
    savedb();
    exit(0);
}

class Socket
{
private:
    int _sockfd;
    struct sockaddr_in _servaddr;

public:
    Socket(int port) : _sockfd(socket(AF_INET, SOCK_STREAM, 0))
    {
        if (_sockfd == -1)
        {
            throw std::runtime_error("Socket creation failed");
        }
        // man 7 socket
        // SOL_SOCKET
        // SO_REUSEADDR
        int opt = 1;
        if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <0) {
            throw std::runtime_error("Socket creation failed");
        }
        memset(&_servaddr, 0, sizeof(_servaddr));
        _servaddr.sin_family = AF_INET;
        _servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        _servaddr.sin_port = htons(port);
    }

    ~Socket()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
        }
    }

    int getFd(){
        return _sockfd;
    }

    void bindAndListen()
    {
        if (bind(_sockfd, (struct sockaddr *)&_servaddr, sizeof(_servaddr)) < 0)
        {
            throw std::runtime_error("Socket listen failed");
        }

        if (listen(_sockfd, 128) < 0){
            throw std::runtime_error("Socket listen failed");
        }
    }

    int acceptClient()
    {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSockFd = accept(_sockfd, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientSockFd < 0)
        {
            throw std::runtime_error("Failed to accept connection");
        }
        return clientSockFd;
    }

    // std::string pullMessage()
    // {
    //     return ("Totally not pulled message");
    // }
};

class Server
{
private:
    Socket _listeningSocket;
    std::vector<int> clients;

public:
    Server(int port) : _listeningSocket(port) {}

    int run()
    {
        // try
        // {
        //     _listeningSocket.bindAndListen();
        //     // Ready to accept connections. Logic for acception connection would go here.
        //     return 0;
        // }
        // catch (const std::exception &e)
        // {
        //     std::cerr << "Error during server run: " << e.what() << std::endl;
        //     return 1; // Return an error code if server fail to start
        // }
        _listeningSocket.bindAndListen();

        while (1) {
            fd_set readfds;

            FD_ZERO(&readfds);
            FD_SET(_listeningSocket.getFd(), &readfds);

            int maxfd = _listeningSocket.getFd();

            for (std::vector<int>::iterator it = clients.begin(); it != clients.end(); ++it) {
                FD_SET(*it, &readfds);
                if (maxfd < *it)
                    maxfd = *it;
            }
            
            if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0) continue;
            
            // New connection
            if (FD_ISSET(_listeningSocket.getFd(), &readfds)) {
                try {
                    int newclient = _listeningSocket.acceptClient();
                    clients.push_back(newclient);
                } catch (...) {}
            }

            // Existing
            for (std::vector<int>::iterator cit = clients.begin(); cit != clients.end();) {
                int cfd = *cit;
                if (FD_ISSET(cfd, &readfds)) {
                    char buffer[4096] = {0};
                    int received = recv(cfd, buffer, 4096,0);

                    if (received <=0) {
                        close(cfd);
                        cit = clients.erase(cit);
                        continue;
                    } else {
                        buffer[received] = 0;

                        std::string data(buffer);
                        std::stringstream ss(data);
                        std::string line;

                        while (std::getline(ss, line)){
                          std::string response = process_command(line);
                          send(cfd, response.c_str(), response.length(),0);
                        }
                    }
                }
                ++cit;
            }
        }
        return 0;
    }

    std::string process_command(std::string line) {
        std::stringstream ss(line);
        std::string command, key, value;
        ss >> command;

        // std::cout << "-" << command << "-" << std::endl;
        if (command == "POST"){
            if (ss >> key >> value) {
                gdatabase[key] = value;
                return "0\n";
            }
        } else if (command == "GET") {
            if (ss >> key) {
                if (gdatabase.find(key) != gdatabase.end()){
                    return "0 " + gdatabase[key] + "\n";
                }
                return "1\n";
            }

        }else if (command == "DELETE") {
            if (ss >> key) {
            if (gdatabase.erase(key)){
                return "0\n";
            }
            return "1\n";
        }
        }
        return "2\n";

    }
};

int main(int argc, char **argv)
{
    if (argc != 3) {
        return 1;
    }
    int port = atoi(argv[1]);
    filename = argv[2];

    loaddb();
    signal(SIGINT, handle_sigint);

    Server server(port);
    return server.run();
}