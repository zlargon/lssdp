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
#define lssdp_error(fmt, agrs...) lssdp_log("ERROR", __LINE__, __func__, fmt, ##agrs)

static int lssdp_log(const char * level, int line, const char * func, const char * format, ...);


/** Global Variable **/
static int (* lssdp_log_callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) = NULL;


// 01. lssdp_set_log_callback
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message)) {
    lssdp_log_callback = callback;
    return 0;
}

// 02. lssdp_get_interface_list
int lssdp_get_interface_list(lssdp_interface interface_list[], size_t interface_list_size) {
    if (interface_list == NULL) {
        lssdp_error("interface_list should not be NULL\n");
        return -1;
    }

    if (interface_list_size == 0) {
        lssdp_error("interface_list_size should not be 0\n");
        return -1;
    }

    // reset interface_list
    memset(interface_list, 0, sizeof(lssdp_interface) * interface_list_size);

    /* Reference to this article:
     * http://stackoverflow.com/a/8007079
     */

    // create socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        lssdp_error("create socket failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    /* result */
    int result = -1;

    char buffer[4096] = {0};
    struct ifconf ifc = {
        .ifc_len = sizeof(buffer),
        .ifc_buf = (caddr_t) buffer
    };

    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        lssdp_error("ioctl failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    size_t index = 0;           // interface index
    size_t buffer_index = 0;
    while (index < interface_list_size && buffer_index <= ifc.ifc_len) {
        struct ifreq * ifr = (struct ifreq *)(buffer + buffer_index);

        /* IPv4 */
        if (ifr->ifr_addr.sa_family == AF_INET) {

            // 1. set interface name
            snprintf(interface_list[index].name, LSSDP_INTERFACE_NAME_LEN - 1, "%s", ifr->ifr_name);

            // 2. set interface ip
            char ip[16] = {0};  // ip = "xxx.xxx.xxx.xxx"
            struct sockaddr_in * addr_in = (struct sockaddr_in *) &ifr->ifr_addr;
            if (inet_ntop(AF_INET, &addr_in->sin_addr, ip, sizeof(ip)) == NULL) {
                lssdp_error("inet_ntop failed, errno = %s (%d)\n", strerror(errno), errno);
                goto end;
            }

            // interface[index].ip = [ xxx, xxx, xxx, xxx ]
            char * token_ptr;
            size_t i;
            for (i = 0; i < 4; i++) {
                char * number_str = strtok_r(i == 0 ? ip : NULL, ".", &token_ptr);
                interface_list[index].ip[i] = atoi(number_str);
            }
            index++;
        }

        /* TODO: IPv6 */

        // increase buffer_index
        buffer_index += _SIZEOF_ADDR_IFREQ(*ifr);
    }

    result = 0;
end:
    close(fd);
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
