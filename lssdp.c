#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>     // close
#include <arpa/inet.h>  // inet_ntop
#include <sys/ioctl.h>  // ioctl, SIOCGIFCONF
#include <net/if.h>     // struct ifconf, struct ifreq
#include "lssdp.h"

/* This is defined on Mac OS X */
#ifndef _SIZEOF_ADDR_IFREQ
#define _SIZEOF_ADDR_IFREQ sizeof
#endif

#define LSSDP_MESSAGE_MAX_LEN 2048

#define lssdp_debug(fmt, agrs...) lssdp_log("DEBUG", __LINE__, __func__, fmt, ##agrs)
#define lssdp_warn(fmt, agrs...)  lssdp_log("WARN",  __LINE__, __func__, fmt, ##agrs)
#define lssdp_error(fmt, agrs...) lssdp_log("ERROR", __LINE__, __func__, fmt, ##agrs)

static int lssdp_log(const char * level, int line, const char * func, const char * format, ...);


/** Global Variable **/
static int (* lssdp_log_callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) = NULL;


// 01. lssdp_set_log_callback
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message)) {
    lssdp_log_callback = callback;
    return 0;
}

// 02. lssdp_get_network_interface
int lssdp_get_network_interface(lssdp_ctx * lssdp) {
    if (lssdp == NULL) {
        lssdp_error("lssdp should not be NULL\n");
        return -1;
    }

    // reset lssdp->interface
    memset(lssdp->interface, 0, sizeof(struct lssdp_interface) * LSSDP_INTERFACE_LIST_SIZE);

    int result = -1;

    /* Reference to this article:
     * http://stackoverflow.com/a/8007079
     */

    // create socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        lssdp_error("create socket failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    char buffer[4096] = {0};
    struct ifconf ifc = {
        .ifc_len = sizeof(buffer),
        .ifc_buf = (caddr_t) buffer
    };

    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        lssdp_error("ioctl failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    size_t index, num = 0;
    struct ifreq * ifr;
    for (index = 0; index < ifc.ifc_len; index += _SIZEOF_ADDR_IFREQ(*ifr)) {
        ifr = (struct ifreq *)(buffer + index);
        if (ifr->ifr_addr.sa_family != AF_INET) {
            // only support IPv4
            continue;
        }

        // get interface ip string
        char ip[16] = {0};  // ip = "xxx.xxx.xxx.xxx"
        struct sockaddr_in * addr_in = (struct sockaddr_in *) &ifr->ifr_addr;
        if (inet_ntop(AF_INET, &addr_in->sin_addr, ip, sizeof(ip)) == NULL) {
            lssdp_error("inet_ntop failed, errno = %s (%d)\n", strerror(errno), errno);
            goto end;
        }

        // too many network interface
        if (num >= LSSDP_INTERFACE_LIST_SIZE) {
            lssdp_warn("the number of network interface is over than max size %d\n", LSSDP_INTERFACE_LIST_SIZE);
            lssdp_debug("%2d. %s : %s\n", num, ifr->ifr_name, ip);
        } else {
            // 1. set interface.name
            snprintf(lssdp->interface[num].name, LSSDP_INTERFACE_NAME_LEN - 1, "%s", ifr->ifr_name);

            // 2. set interface.ip = [ xxx, xxx, xxx, xxx ]
            char * token_ptr;
            size_t i;
            for (i = 0; i < 4; i++) {
                lssdp->interface[num].ip[i] = atoi(strtok_r(i == 0 ? ip : NULL, ".", &token_ptr));
            }
        }

        // increase interface number
        num++;
    }

    result = 0;
end:
    if (fd > 0) close(fd);
    return result;
}


/** Internal Function **/

static int lssdp_log(const char * level, int line, const char * func, const char * format, ...) {
    if (lssdp_log_callback == NULL) {
        return -1;
    }

    char message[LSSDP_MESSAGE_MAX_LEN] = {0};

    // create message by va_list
    va_list args;
    va_start(args, format);
    vsnprintf(message, LSSDP_MESSAGE_MAX_LEN, format, args);
    va_end(args);

    // invoke log callback function
    lssdp_log_callback(__FILE__, "SSDP", level, line, func, message);
    return 0;
}
