#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <iostream>

// 오류 메시지 출력 함수
void myerror(const char* msg) {
    fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);
}

// 사용법 안내 함수
void usage() {
    printf("syntax : echo-client <ip> <port>\n");
    printf("sample : echo-client 192.168.10.2 1234\n");
}

// 파라미터 구조체, IP와 포트 저장
struct Param {
    char* ip{nullptr};
    char* port{nullptr};

    // 커맨드 라인 인자를 파싱하여 IP와 포트를 검증하는 함수
    bool parse(int argc, char* argv[]) {
        if (argc < 3) return false;
        ip = argv[1];
        port = argv[2];

        // 포트 문자열이 숫자로만 이루어져 있는지 검사
        for (char* p = port; *p != 0; p++) {
            if (!isdigit(*p)) return false;
        }
        return true;
    }
} param;

// 데이터 수신을 담당하는 별도의 스레드 함수
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
    hints.ai_family = AF_INET;       // IPv4 사용
    hints.ai_socktype = SOCK_STREAM; // TCP 소켓 타입

    // 주소 정보를 가져오기
    int error = getaddrinfo(param.ip, param.port, &hints, &res0);
    if (error) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    int sockfd;
    // 가능한 주소 정보를 순회하면서 소켓 생성 및 서버 연결 시도
    for (res = res0; res; res = res->ai_next) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
            break;  // 연결 성공 시 반복 종료
        }

        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res0); // 메모리 해제

    if (sockfd < 0) {
        fprintf(stderr, "cannot connect to %s:%s\n", param.ip, param.port);
        return -1;
    }

    std::thread t(recvThread, sockfd); // 수신 스레드 시작
    t.detach(); // 스레드 분리 (메인 스레드와 독립적으로 수행)

    // 사용자 입력 받기 및 서버에 메시지 전송
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

    close(sockfd); // 소켓 닫기
    return 0;
}
