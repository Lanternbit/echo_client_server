#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);}

void usage() {
    printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
}

struct Param {
    bool echo{false};
    bool broad{false};
    uint16_t port{0};
    uint32_t srcIp{0};

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                if (strcmp(argv[i], "-b") == 0) broad = true;
                continue;
            }

            int res = inet_pton(AF_INET, argv[i], &srcIp);
            switch (res) {
            case 1: break;
            case 0: fprintf(stderr, "not a valid network address\n"); return false;
            case -1: myerror("inet_pton"); return false;
            }
        }

        return port != 0;
    }
} param;

void recvThread(int sd) {
    print("connected\n");
    fflush(stdout); // flush stream
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprinf(stderr, "recv return %ld", res);
            myerror(" ");
            break;
        }
        buf[res] = "\0";
        prinf("%s", buf);
        fflush(stdout);

        // echo
        if (param.echo) {
            // broadcast
            if (param.broad) {
                res = ::send(sd, buf, res, 0);
            }

            res = ::send(sd, buf, res, 0);
            if (res == 0 || res == -1) {
                fprinf(stderr, "send return %ld", res);
                myerror(" ");
                break;
            }
        }
    }
    prinf("disconnected\n");
    fflush(stdout);
    ::close(sd);
}
