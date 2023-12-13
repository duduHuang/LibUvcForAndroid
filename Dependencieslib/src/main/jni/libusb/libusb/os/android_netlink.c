/* -*- Mode: C; c-basic-offset:8 ; indent-tabs-mode:t -*- */
/*
 * non-rooted Android usbfs backend for libusb
 * Copyright (C) 2007-2009 Daniel Drake <dsd@gentoo.org>
 * Copyright (c) 2001 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright (c) 2013 Nathan Hjelm <hjelmn@mac.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define LOG_TAG "libusb/netlink"
#if 0    // デバッグ情報を出さない時1
#ifndef LOG_NDEBUG
#define	LOG_NDEBUG		// LOGV/LOGD/MARKを出力しない時
#endif
#undef USE_LOGALL			// 指定したLOGxだけを出力
#else
#define USE_LOGALL
#undef LOG_NDEBUG
#undef NDEBUG
#define GET_RAW_DESCRIPTOR
#endif

#include "config.h"
#include "libusb.h"
#include "libusbi.h"
#include "android_usbfs.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <arpa/inet.h>


#include <linux/netlink.h>

#define NL_GROUP_KERNEL 1
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC    0
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK    0
#endif


static int android_netlink_socket = -1;
static usbi_event_t netlink_control_event = USBI_INVALID_EVENT;
static pthread_t libusb_android_event_thread;

static void *android_netlink_event_thread_main(void *arg);

static int set_fd_cloexec_nb(int fd, int socktype) {
    int flags;

#if defined(FD_CLOEXEC)
    if (!(socktype & SOCK_CLOEXEC)) {
        flags = fcntl(android_netlink_socket, F_GETFD);
        if (0 > flags)
            return -1;

        if (-1 == fcntl(android_netlink_socket, F_SETFD, flags | FD_CLOEXEC))
            return -1;
    }
#endif

    if (!(socktype & SOCK_NONBLOCK)) {
        flags = fcntl(android_netlink_socket, F_GETFL);
        if (0 > flags) {
            return -1;
        }

        if (-1 == fcntl(android_netlink_socket, F_SETFL, flags | O_NONBLOCK))
            return -1;
    }

    return 0;
}

int android_netlink_start_event_monitor(void) {
    ENTER();
    struct sockaddr_nl sa_nl = {.nl_family = AF_NETLINK, .nl_groups = NL_GROUP_KERNEL};
    int socktype = SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC;
    int opt = 1;
    int ret;

    android_netlink_socket = socket(PF_NETLINK, socktype, NETLINK_KOBJECT_UEVENT);
    if (-1 == android_netlink_socket && EINVAL == errno) {
        usbi_dbg(NULL, "failed to create netlink socket of type %d, attempting SOCK_RAW", socktype);
        socktype = SOCK_RAW;
        android_netlink_socket = socket(PF_NETLINK, socktype, NETLINK_KOBJECT_UEVENT);
    }

    if (-1 == android_netlink_socket) {
        LOGE("failed to create android_netlink_socket:errno=%d",
             errno);    // 13:Permission deniedが返ってくる
        goto err;
    }

    ret = set_fd_cloexec_nb(android_netlink_socket, socktype);
    if (0 != ret)
        goto err_close_socket;


    ret = bind(android_netlink_socket, (struct sockaddr *) &sa_nl, sizeof(sa_nl));
    if (0 != ret) {
        goto err_close_socket;
    }

    /* TODO -- add authentication */
    ret = setsockopt(android_netlink_socket, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt));
    if (ret == -1) {
        usbi_err(NULL, "failed to set netlink socket SO_PASSCRED option, errno=%d", errno);
        goto err_close_socket;
    }

    ret = usbi_create_event(&netlink_control_event);
    if (ret) {
        LOGE("failed to create netlink control event");
        usbi_err(NULL, "failed to create netlink control event");
        goto err_close_socket;
    }

    ret = pthread_create(&libusb_android_event_thread, NULL, android_netlink_event_thread_main,
                         NULL);
    if (0 != ret) {
        usbi_err(NULL, "failed to create netlink event thread (%d)", ret);
        goto err_destroy_event;
    }

    return LIBUSB_SUCCESS;

    err_destroy_event:
    usbi_destroy_event(&netlink_control_event);
    netlink_control_event = (usbi_event_t) USBI_INVALID_EVENT;
    err_close_socket:
    close(android_netlink_socket);
    android_netlink_socket = -1;
    err:
    return LIBUSB_ERROR_OTHER;
}

int android_netlink_stop_event_monitor(void) {
    int r;
    char dummy = 1;

    if (-1 == android_netlink_socket) {
        /* already closed. nothing to do */
        return LIBUSB_SUCCESS;
    }

    /* Write some dummy data to the control pipe and
    usbi_signal_event(&netlink_control_event);
     * wait for the thread to exit */
    r = pthread_join(libusb_android_event_thread, NULL);
    if (r)
        usbi_warn(NULL, "failed to join netlink event thread (%d)", r);

    usbi_destroy_event(&netlink_control_event);
    netlink_control_event = (usbi_event_t) USBI_INVALID_EVENT;

    close(android_netlink_socket);
    android_netlink_socket = -1;

    return LIBUSB_SUCCESS;
}

static const char *netlink_message_parse(const char *buffer, size_t len, const char *key) {
    const char *end = buffer + len;
    size_t keylen = strlen(key);

    while (buffer < end && *buffer) {
        if (strncmp(buffer, key, keylen) == 0 && buffer[keylen] == '=')
            return buffer + keylen + 1;
        buffer += strlen(buffer) + 1;
    }

    return NULL;
}

/* parse parts of netlink message common to both libudev and the kernel */
static int android_netlink_parse(char *buffer, size_t len, int *detached, const char **sys_name,
                                 uint8_t *busnum, uint8_t *devaddr) {
    const char *tmp, *pLastSlash;
    int i;

    errno = 0;

    *sys_name = NULL;
    *detached = 0;
    *busnum = 0;
    *devaddr = 0;

    tmp = netlink_message_parse((const char *) buffer, len, "ACTION");
    if (tmp == NULL)
        return -1;
    if (0 == strcmp(tmp, "remove")) {
        *detached = 1;
    } else if (0 == strcmp(tmp, "add")) {
        // pass through
    } else if (0 != strcmp(tmp, "change")) {
        usbi_dbg("unknown device action [%s]", tmp);
        return -1;
    }

    /* check that this is a usb message */
    tmp = netlink_message_parse(buffer, len, "SUBSYSTEM");
    if (NULL == tmp || 0 != strcmp(tmp, "usb")) {
        /* not usb. ignore */
        return -1;
    }

    tmp = netlink_message_parse(buffer, len, "DEVTYPE");
    if (!tmp || strcmp(tmp, "usb_device") != 0) {
        /* not usb. ignore */
        return -1;
    }

    tmp = netlink_message_parse(buffer, len, "BUSNUM");
    if (tmp) {
        *busnum = (uint8_t)(strtoul(tmp, NULL, 10) & 0xff);
        if (errno) {
            errno = 0;
            return -1;
        }

        tmp = netlink_message_parse(buffer, len, "DEVNUM");
        if (NULL == tmp)
            return -1;

        *devaddr = (uint8_t)(strtoul(tmp, NULL, 10) & 0xff);
        if (errno) {
            errno = 0;
            return -1;
        }
    } else {
        /* no bus number. try "DEVICE" */
        tmp = netlink_message_parse(buffer, len, "DEVICE");
        if (NULL == tmp) {
            /* not usb. ignore */
            return -1;
        }

        /* Parse a device path such as /dev/bus/usb/003/004 */
        pLastSlash = (char *) strrchr(tmp, '/');
        if (NULL == pLastSlash)
            return -1;

        *busnum = (uint8_t)(strtoul(pLastSlash - 3, NULL, 10) & 0xff);
        if (errno) {
            errno = 0;
            return -1;
        }

        *devaddr = (uint8_t)(strtoul(pLastSlash + 1, NULL, 10) & 0xff);
        if (errno) {
            errno = 0;
            return -1;
        }

        return 0;
    }

    tmp = netlink_message_parse(buffer, len, "DEVPATH");
    if (NULL == tmp)
        return -1;

    pLastSlash = strrchr(tmp, '/');
    if (pLastSlash)
        *sys_name = pLastSlash + 1;

    /* found a usb device */
    return 0;
}

static int android_netlink_read_message(void) {
    char cred_buffer[CMSG_SPACE(sizeof(struct ucred))];
    char msg_buffer[2048];    // XXX changed from unsigned char to char because the first argument of android_netlink_parse is char *
    const char *sys_name = NULL;
    uint8_t busnum, devaddr;
    int detached, r;
    size_t len;
    struct cmsghdr *cmsg;
    struct ucred *cred;
    struct sockaddr_nl sa_nl;
    struct iovec iov = {.iov_base = msg_buffer, .iov_len = sizeof(msg_buffer)};
    struct msghdr msg = {
            .msg_iov = &iov, .msg_iovlen = 1,
            .msg_control = cred_buffer, .msg_controllen = sizeof(cred_buffer),
            .msg_name = &sa_nl, .msg_namelen = sizeof(sa_nl)
    };

    /* read netlink message */
    memset(msg_buffer, 0, sizeof(msg_buffer));
    len = recvmsg(android_netlink_socket, &msg, 0);
    if (len == -1) {
        if (errno != EAGAIN)
            usbi_dbg("error receiving message from netlink");
        return -1;
    }

    if (len < 32 || (msg.msg_flags & MSG_TRUNC)) {
        usbi_err(NULL, "invalid netlink message length");
        return -1;
    }

    if (sa_nl.nl_groups != NL_GROUP_KERNEL || sa_nl.nl_pid != 0) {
        usbi_dbg(NULL, "ignoring netlink message from unknown group/PID (%u/%u)",
                 (unsigned int) sa_nl.nl_groups, (unsigned int) sa_nl.nl_pid);
        return -1;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg || cmsg->cmsg_type != SCM_CREDENTIALS) {
        usbi_dbg(NULL, "ignoring netlink message with no sender credentials");
        return -1;
    }

    cred = (struct ucred *) CMSG_DATA(cmsg);
    if (cred->uid != 0) {
        usbi_dbg(NULL, "ignoring netlink message with non-zero sender UID %u",
                 (unsigned int) cred->uid);
        return -1;
    }
    r = android_netlink_parse(msg_buffer, len, &detached, &sys_name, &busnum, &devaddr);
    if (r)
        return r;

    usbi_dbg("netlink hotplug found device busnum: %hhu, devaddr: %hhu, sys_name: %s, removed: %s",
             busnum, devaddr, sys_name, detached ? "yes" : "no");

    /* signal device is available (or not) to all contexts */
    if (detached)
        android_device_disconnected(busnum, devaddr, sys_name);
    else
        android_hotplug_enumerate(busnum, devaddr, sys_name);

    return 0;
}

static void *android_netlink_event_thread_main(void *arg) {
    char dummy;

    struct pollfd fds[] = {
            {.fd = USBI_EVENT_OS_HANDLE(&netlink_control_event),
                    .events = USBI_EVENT_POLL_EVENTS},
            {.fd = android_netlink_socket,
                    .events = POLLIN},
    };
    int r;
    /* silence compiler warning */
    (void) arg;


#if defined(HAVE_PTHREAD_SETNAME_NP)
    r = pthread_setname_np(pthread_self(), "libusb_event");
    if (r)
        usbi_warn(NULL, "failed to set hotplug event thread name, error=%d", r);
#endif


    while (1) {
        r = poll(fds, 2, -1);
        if (r <= 0) {
            /* check for temporary failure */
            if (errno == EINTR)
                continue;
            usbi_warn(NULL, "netlink control pipe read failed");
            break;
        }
        if (fds[0].revents) {
            /* activity on control event, exit */
            break;
        }
        if (fds[1].revents) {
            usbi_mutex_static_lock(&android_hotplug_lock);
            android_netlink_read_message();
            usbi_mutex_static_unlock(&android_hotplug_lock);
        }
    }

    return NULL;
}

void android_netlink_hotplug_poll(void) {
    int r;

    usbi_mutex_static_lock(&android_hotplug_lock);
    do {
        r = android_netlink_read_message();
    } while (r == 0);
    usbi_mutex_static_unlock(&android_hotplug_lock);
}
