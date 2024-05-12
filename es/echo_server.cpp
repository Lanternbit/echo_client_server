#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

// 전역 변수로 클라이언트 소켓 목록과 뮤텍스를 관리
std::vector<int> client_sockets;
std::mutex mtx;
std::vector<std::thread> threads; // 스레드를 저장하는 벡터 추가

// 오류 발생 시 오류 메시지와 함께 errno를 출력하는 함수
void myerror(const char* msg) {
    fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);
}

// 프로그램 사용법을 출력하는 함수
void usage() {
    printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
}

// 서버 파라미터를 저장하고 파싱하는 구조체
struct Param {
    bool echo{false}; // 에코 옵션
    bool broad{false}; // 브로드캐스트 옵션
    uint16_t port{0}; // 포트 번호

    // 명령줄 인자를 파싱하여 파라미터를 설정하는 함수
    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                continue;
            }
            if (strcmp(argv[i], "-b") == 0 && echo) {
                broad = true;
                continue;
            }
            int possiblePort = atoi(argv[i]);
            if (possiblePort > 0) {  // 유효한 포트 번호를 확인
                port = possiblePort;
            }
        }
        return port != 0;
    }
} param;

// 모든 클라이언트에게 메시지를 브로드캐스트하는 함수
void broadcastMessage(const char* message, size_t len) {
    std::lock_guard<std::mutex> lock(mtx);
    for (int clientSock : client_sockets) {
        ssize_t res = ::send(clientSock, message, len, 0);
        if (res <= 0) {
            fprintf(stderr, "send return %ld\n", res);
            myerror("broadcast send");
            continue;
        }
    }
}

// 클라이언트에서 데이터를 받고 처리하는 스레드 함수
void recvThread(int clientSock) {
    printf("Client connected\n");
    fflush(stdout);
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    {
        std::lock_guard<std::mutex> lock(mtx);
        client_sockets.push_back(clientSock);
    }

    while (true) {
        ssize_t res = ::recv(clientSock, buf, BUFSIZE - 1, 0);
        if (res <= 0) {
            fprintf(stderr, "recv return %ld\n", res);
            myerror("recv");
            break;
        }
        buf[res] = '\0';
        printf("Received: %s", buf);
        fflush(stdout);

        // 에코 설정이 활성화되어 있으면, 메시지를 반환하거나 브로드캐스트
        if (param.echo) {
            if (param.broad) {
                broadcastMessage(buf, res);
            } else {
                res = ::send(clientSock, buf, res, 0);
                if (res <= 0) {
                    fprintf(stderr, "send return %ld\n", res);
                    myerror("send");
                    break;
                }
            }
        }
    }
    printf("Client disconnected\n");
    fflush(stdout);
    ::close(clientSock);

    // 클라이언트 소켓을 벡터에서 제거
    std::lock_guard<std::mutex> lock(mtx);
    client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), clientSock), client_sockets.end());
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    int cs = ::socket(AF_INET, SOCK_STREAM, 0);
    if (cs == -1) {
        myerror("socket");
        return -1;
    }

    // SO_REUSEADDR 옵션을 설정하여 주소 재사용을 허용
    {
        int optval = 1;
        if (::setsockopt(cs, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            myerror("setsockopt");
            return -1;
        }
    }

    // 서버 주소를 설정하고 바인드
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(param.port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(cs, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            myerror("bind");
            return -1;
        }
    }

    // 클라이언트의 연결 요청을 대기
    if (::listen(cs, 5) == -1) {
        myerror("listen");
        return -1;
    }

    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int new_cs = ::accept(cs, (struct sockaddr*)&addr, &len);
        if (new_cs == -1) {
            myerror("accept");
            continue;  // 오류 발생 시 다음 연결을 계속 대기
        }
        std::thread t(recvThread, new_cs);
        threads.push_back(std::move(t));  // 스레드를 벡터에 저장
        threads.back().detach();  // 스레드를 분리
    }
    ::close(cs);
    return 0;
}
