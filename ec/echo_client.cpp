#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <iostream>

void myerror(const char* msg) {
    fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);
}

void usage() {
    printf("syntax : echo-client <ip> <port>\n");
    printf("sample : echo-client 192.168.10.2 1234\n");
}

struct Param {
    char* ip{nullptr};
    char* port{nullptr};

    bool parse(int argc, char* argv[]) {
        if (argc < 3) return false;
        ip = argv[1];
        port = argv[2];

        for (char* p = port; *p != 0; p++) {
            if (!isdigit(*p)) return false;
        }
        return true;
    }
} param;

void recvThread(int serverSock) {
    printf("connected\n");
    fflush(stdout);
    static const int BUFSIZE = 65535;
    char buf[BUFSIZE];
    while (true) {
        ssize_t res = recv(serverSock, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "recv return %ld\n", res);
            myerror("recv");
            break;
        }
        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);
    }
    printf("disconnected\n");
    fflush(stdout);
    close(serverSock);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    struct addrinfo hints, *res, *res0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int error = getaddrinfo(param.ip, param.port, &hints, &res0);
    if (error) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    int sockfd;
    for (res = res0; res; res = res->ai_next) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
            break;  // success
        }

        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res0);

    if (sockfd < 0) {
        fprintf(stderr, "cannot connect to %s:%s\n", param.ip, param.port);
        return -1;
    }

    std::thread t(recvThread, sockfd);
    t.detach();

    while (true) {
        std::string s;
        std::getline(std::cin, s);
        s += "\r\n";
        ssize_t sent = send(sockfd, s.data(), s.size(), 0);
        if (sent == 0 || sent == -1) {
            fprintf(stderr, "send return %ld\n", sent);
            myerror("send");
            break;
        }
    }

    close(sockfd);
    return 0;
}
