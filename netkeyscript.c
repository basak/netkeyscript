/*
 * Copyright 2012 Robie Basak
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
*/

#define NETKEYSCRIPT_PORT 30621

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int ifup(void) {
    int fd;
    struct ifreq req;

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
	perror("socket");
	return 0;
    }

    memset(&req, 0, sizeof(req));
    strcpy(req.ifr_name, "eth0");
    if (ioctl(fd, SIOCGIFFLAGS, &req) < 0) {
	perror("ioctl");
	return 0;
    }
    if (!(req.ifr_flags & IFF_UP)) {
	req.ifr_flags |= IFF_UP;
	if (ioctl(fd, SIOCSIFFLAGS, &req) < 0) {
	    perror("ioctl");
	    return 0;
	}
    }
    return 1;
}

ssize_t read_passphrase(int socket, uint8_t *msg, ssize_t msg_size) {
    fd_set fds;
    struct timeval timeout;
    ssize_t result;

    for (;;) {
	fputs("\a", stderr);
	FD_ZERO(&fds);
	FD_SET(socket, &fds);
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	result = select(socket+1, &fds, NULL, NULL, &timeout);
	if (result < 0 && errno != EINTR) {
	    perror("select");
	    return -1;
	} else if (result > 0) {
	    result = recv(socket, msg, msg_size, 0);
	    if (result < 0) {
		perror("recv");
		return -1;
	    }
	    return result;
	}
    }
}

int main(int argc, char **argv) {
    int fd;
    uint8_t msg[2048];
    ssize_t msg_size;
    struct ipv6_mreq mreq;
    struct sockaddr_in6 addr;

    if (!ifup())
	return 1;

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
	perror("socket");
	return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(NETKEYSCRIPT_PORT);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	perror("bind");
	return 1;
    }

    memset(&mreq, 0, sizeof(mreq));
    if (!inet_pton(AF_INET6, "ff02::1", &mreq.ipv6mr_multiaddr)) {
	perror("inet_pton");
	return 1;
    }
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq,
		   sizeof(mreq)) < 0) {
	perror("setsockopt");
	return 1;
    }

    fputs("netkeyscript: waiting for passphrase\n", stderr);
    msg_size = read_passphrase(fd, msg, sizeof(msg));
    if (msg_size < 0) {
	return 1;
    }
    if (fwrite(msg, msg_size, 1, stdout) < 1) {
	fputs("fwrite: error\n", stderr);
	return 1;
    }
    return 0;
}
