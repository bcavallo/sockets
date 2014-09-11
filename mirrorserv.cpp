#include <algorithm>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

int tcpbindsockettoport(int port) {

    struct sockaddr_in destAddr;

    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    destAddr.sin_addr.s_addr = INADDR_ANY;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "socket error: " << strerror(errno) << std::endl;
        return sockfd;
    }

    int status;
    status = bind(sockfd, (struct sockaddr *) &destAddr, sizeof(destAddr));
    if (status < 0) {
        std::cerr << "bind error: " << strerror(errno) << std::endl;
        return status;
    }

    status = listen(sockfd, 5);
    if (status < 0) {
        std::cerr << "listen error: " << strerror(errno) << std::endl;
        return status;
    }
    return sockfd;
}

static int count = 5;
static int buffsize = 10;

int readthenwrite(int sockfd) {
    char * buff = new char[buffsize];
        int inc = read(sockfd, buff, count);
        if (inc < 0 && errno != EINTR) {
            std::cerr << "read error: " << strerror(errno) << std::endl;
            delete[] buff;
            return 0;
        }
        if (inc == 0) {
            return 0;
        }
        write(sockfd, buff, inc);
    delete[] buff;
    return inc;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "there must be two arguments."  << std::endl;
        return 2;
    }

    int sockfd = tcpbindsockettoport(atoi(argv[1]));

    std::vector<int> socks;
    fd_set readset;
    int total = 0;

    while (true) {
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);
        int maxfd = sockfd;
        for (auto it = socks.begin(); it != socks.end(); ++it) {
            FD_SET(*it, &readset);
            if (*it > maxfd) maxfd = *it;
        }
        int res = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (res < 0 && errno != EINTR) {
            std::cerr << "select error: " << strerror(errno) << std::endl;
            return -1;
        }
        if (res == 0) {
            std::cout << "select timed out" << std::endl;
            return 1;
        } 
        if (res > 0) {
            if (FD_ISSET(sockfd, &readset)) {
                struct sockaddr_in sourceAddr;
                socklen_t addrLen = sizeof(sourceAddr);
                int newsock = accept(sockfd,
                    (struct sockaddr *) &sourceAddr, &addrLen);
                if (newsock < 0) {
                    std::cerr << "accept error: " << strerror(errno)
                        << std::endl;
                    return 1;
                }
                FD_SET(newsock, &readset);
                total += 1;
                socks.push_back(newsock);
                if (newsock > maxfd) maxfd = newsock;
            }
            FD_CLR(sockfd, &readset);
            for (int i = 0; i < maxfd + 1; ++i) {
                if (FD_ISSET(i, &readset)) {
                    int inc = readthenwrite(i);
                    if (inc == 0) {
                        close(i);
                        total -= 1;
                        std::remove(socks.begin(), socks.end(), i);
                        socks.resize(total);
                    }
                }
            }
        }
    }
}
