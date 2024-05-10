#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

std::vector<int> client_sockets;
std::mutex mtx;

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);}

void usage() {
    printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
}

struct Param {
    bool echo{false};
    bool broad{false};
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        bool lastArgEcho = false;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                lastArgEcho = true;
                continue;
            } else if (strcmp(argv[i], "-b") == 0 && lastArgEcho) {
                broad = true;
                continue;
            }

            port = atoi(argv[i]);
        }

        return port != 0;
    }
} param;

void broadcastMessage(const char* message, size_t len) {
    std::lock_guard<std::mutex> lock(mtx);
    for (int clientSock : client_sockets) {
        ssize_t res = ::send(clientSock, message, len, 0);

        if (res == 0 || res == -1) {
            fprintf(stderr, "send return %ld", res);
            myerror("broadcast send");
            break;
        }
    }
}

void recvThread(int clientSock) {
    printf("connected\n");
    fflush(stdout); // flush stream
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    // Register client
    {
        std::lock_guard<std::mutex> lock(mtx);
        client_sockets.push_back(clientSock);
    }

    while (true) {
        ssize_t res = ::recv(clientSock, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "recv return %ld\n", res);
            myerror("recv");
            break;
        }
        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);

        // echo
        if (param.echo) {
            // broadcast
            if (param.broad) {
                broadcastMessage(buf, res);
            } else {
                res = ::send(clientSock, buf, res, 0);

                if (res == 0 || res == -1) {
                    fprintf(stderr, "send return %ld", res);
                    myerror("send");
                    break;
                }
            }
        }
    }
    printf("disconnected\n");
    fflush(stdout);
    ::close(clientSock);

    // Remove client
    std::lock_guard<std::mutex> lock(mtx);
    client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), clientSock), client_sockets.end());
}

int main (int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    int cs = ::socket(AF_INET, SOCK_STREAM, 0);
    if (cs == -1) {
        myerror("socket");
        return -1;
    }

    {
        int optval = 1;
        int res = ::setsockopt(cs, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (res == -1) {
            myerror("setsockopt");
            return -1;
        }
    }

    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(param.port);

        ssize_t res = ::bind(cs, (struct sockaddr*)&addr, sizeof(addr));
        if (res == -1) {
            myerror("bind");
            return -1;
        }
    }

    {
        int res = listen(cs, 5);
        if (res == -1) {
            myerror("listen");
            return -1;
        }
    }

    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int new_cs = ::accept(cs, (struct sockaddr*)&addr, &len);
        if (new_cs == -1) {
            myerror("accept");
            break;
        }
        std::thread* t = new std::thread(recvThread, new_cs);
        t->detach();
    }
    ::close(cs);
}
