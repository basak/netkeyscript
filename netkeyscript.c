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
#define NETKEYSCRIPT_PROTO_PASSPHRASE 0
#define NETKEYSCRIPT_PROTO_REQUEST 1
#define NETKEYSCRIPT_PROTO_RECEIVED 2

#define _GNU_SOURCE 1
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define SEEN_RUNNING 1
#define SEEN_ADDRESS 2
#define SEEN_READY (SEEN_RUNNING | SEEN_ADDRESS)

int ifup_start_listening(int fd) {
    struct sockaddr_nl sa;

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa))) {
	perror("bind");
	return 0;
    }
    return 1;
}

void ifup_scan_event(struct nlmsghdr *nh, int len, int *seen_flags) {
    struct ifinfomsg *ifi;

    for (;NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
	switch (nh->nlmsg_type) {
	    case NLMSG_DONE:
		return;

	    case NLMSG_ERROR:
		return;

	    case RTM_NEWLINK:
		ifi = NLMSG_DATA(nh);
		if (ifi->ifi_flags & IFF_RUNNING) {
		    *seen_flags |= SEEN_RUNNING;
		}
		break;

	    case RTM_NEWADDR:
		*seen_flags |= SEEN_ADDRESS;
		break;
	}
    }
}

int ifup_read_event(int fd, int *seen_flags) {
    struct pollfd fds;
    int len;
    char buf[4096];
    struct iovec iov;
    struct msghdr msg;

    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    fds.fd = fd;
    fds.events = POLLIN;

    if (TEMP_FAILURE_RETRY(poll(&fds, 1, -1)) < 0) {
	perror("poll");
	return 0;
    }
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    len = recvmsg(fd, &msg, 0);
    if (len < 0) {
	perror("recvmsg");
	return 0;
    }

    if (seen_flags)
	ifup_scan_event((struct nlmsghdr *)buf, len, seen_flags);

    return 1;
}

int ifup(void) {
    int inet6_fd;
    int netlink_fd;
    struct ifreq req;
    int seen_flags = 0;

    inet6_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (inet6_fd < 0) {
	perror("socket");
	return 0;
    }
    netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (netlink_fd < 0) {
	perror("socket");
	close(inet6_fd);
	return 0;
    }
    if (!ifup_start_listening(netlink_fd)) {
	close(netlink_fd);
	close(inet6_fd);
	return 0;
    }

    memset(&req, 0, sizeof(req));
    strcpy(req.ifr_name, "eth0");
    if (ioctl(inet6_fd, SIOCGIFFLAGS, &req) < 0) {
	perror("ioctl");
	close(netlink_fd);
	close(inet6_fd);
	return 0;
    }
    if (!(req.ifr_flags & IFF_UP)) {
	req.ifr_flags |= IFF_UP;
	if (ioctl(inet6_fd, SIOCSIFFLAGS, &req) < 0) {
	    perror("ioctl");
	    close(netlink_fd);
	    close(inet6_fd);
	    return 0;
	}
    }

    if (!(req.ifr_flags & IFF_RUNNING)) {
	fputs("netkeyscript: waiting for eth0 to be running with an address "
		"assigned\n", stderr);
	while ((seen_flags & SEEN_READY) != SEEN_READY) {
	    if (!ifup_read_event(netlink_fd, &seen_flags)) {
		close(netlink_fd);
		close(inet6_fd);
		return 0;
	    }
	}
	if (ioctl(inet6_fd, SIOCGIFFLAGS, &req) < 0) {
	    perror("ioctl");
	    close(netlink_fd);
	    close(inet6_fd);
	    return 0;
	}
	if (!(req.ifr_flags & IFF_RUNNING)) {
	    fputs("netkeyscript: eth0 still not running but continuing "
		    "anyway\n", stderr);
	}
    }
    close(netlink_fd);
    close(inet6_fd);
    return 1;
}

int ifdown(void) {
    int inet6_fd;
    struct ifreq req;

    inet6_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (inet6_fd < 0) {
	perror("socket");
	return 0;
    }

    memset(&req, 0, sizeof(req));
    strcpy(req.ifr_name, "eth0");
    if (ioctl(inet6_fd, SIOCGIFFLAGS, &req) < 0) {
	perror("ioctl");
	close(inet6_fd);
	return 0;
    }
    if (req.ifr_flags & IFF_UP) {
	req.ifr_flags &= ~IFF_UP;
	if (ioctl(inet6_fd, SIOCSIFFLAGS, &req) < 0) {
	    perror("ioctl");
	    close(inet6_fd);
	    return 0;
	}
    }
    return 1;
}

int send_command(int fd, uint8_t command) {
    struct sockaddr_in6 addr;
    ssize_t result;

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    if (!inet_pton(AF_INET6, "ff02::1", &addr.sin6_addr)) {
	perror("inet_pton");
	return 0;
    }
    addr.sin6_port = htons(NETKEYSCRIPT_PORT);
    result = sendto(fd, &command, sizeof(command), 0,
		    (struct sockaddr *)&addr, sizeof(addr));
    if (result < 0) {
	perror("sendto");
	return 0;
    }
    return 1;
}

ssize_t read_passphrase(int socket, uint8_t *msg, ssize_t msg_size) {
    fd_set fds;
    struct timeval timeout;
    ssize_t result;

    for (;;) {
	fputs("\a", stderr);
	if (!send_command(socket, NETKEYSCRIPT_PROTO_REQUEST))
	    return -1;
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
	    if (msg_size > 0 && msg[0] == NETKEYSCRIPT_PROTO_PASSPHRASE) {
		send_command(socket, NETKEYSCRIPT_PROTO_RECEIVED);
		return result;
	    }
	}
    }
}

int main(int argc, char **argv) {
    int fd;
    uint8_t msg[2048];
    ssize_t msg_size;
    struct ipv6_mreq mreq;
    struct sockaddr_in6 addr;
    const int zero = 0;

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
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &zero,
		   sizeof(zero))) {
	perror("setsockopt");
	return 1;
    }

    fputs("netkeyscript: waiting for passphrase\n", stderr);
    msg_size = read_passphrase(fd, msg, sizeof(msg));
    if (msg_size < 0) {
	return 1;
    }
    /* +1/-1 to skip the command prefix code */
    if (fwrite(msg+1, msg_size-1, 1, stdout) < 1) {
	fputs("fwrite: error\n", stderr);
	return 1;
    }
    ifdown();
    return 0;
}
