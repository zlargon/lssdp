#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>      // isprint, isspace
#include <errno.h>

#include <unistd.h>     // close
#include <arpa/inet.h>  // inet_ntop
#include <sys/ioctl.h>  // ioctl, SIOCGIFCONF
#include <sys/time.h>   // gettimeofday
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

/** Struct: lssdp_packet **/
typedef struct lssdp_packet {
    char            method      [LSSDP_FIELD_LEN];      // M-SEARCH, NOTIFY, RESPONSE
    char            st          [LSSDP_FIELD_LEN];      // Search Target
    char            usn         [LSSDP_FIELD_LEN];      // Unique Service Name
    char            location    [LSSDP_FIELD_LEN];      // Location

    /* Additional SSDP Header Fields */
    char            sm_id       [LSSDP_FIELD_LEN];
    char            device_type [LSSDP_FIELD_LEN];
    unsigned long   update_time;
} lssdp_packet;

/** Internal Function **/
static int send_multicast_data(const char * data, const struct lssdp_interface interface, int ssdp_port);
static int lssdp_send_response(lssdp_ctx * lssdp, struct sockaddr_in address);
static int lssdp_packet_parser(const char * data, size_t data_len, lssdp_packet * packet);
static int parse_field_line(const char * data, size_t start, size_t end, lssdp_packet * packet);
static int get_colon_index(const char * string, size_t start, size_t end);
static int trim_spaces(const char * string, size_t * start, size_t * end);
static long get_current_time();
static int lssdp_log(const char * level, int line, const char * func, const char * format, ...);
static int show_network_interface(lssdp_ctx * lssdp);
static int neighbor_list_add(lssdp_ctx * lssdp, const lssdp_packet packet);

/** Global Variable **/
static struct {
    const char * MSEARCH;
    const char * NOTIFY;
    const char * RESPONSE;

    const char * HEADER_MSEARCH;
    const char * HEADER_NOTIFY;
    const char * HEADER_RESPONSE;

    const char * ADDR_LOCALHOST;
    const char * ADDR_MULTICAST;

    int (* log_callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message);

} Global = {
    // SSDP Method
    .MSEARCH  = "M-SEARCH",
    .NOTIFY   = "NOTIFY",
    .RESPONSE = "RESPONSE",

    // SSDP Header
    .HEADER_MSEARCH  = "M-SEARCH * HTTP/1.1\r\n",
    .HEADER_NOTIFY   = "NOTIFY * HTTP/1.1\r\n",
    .HEADER_RESPONSE = "HTTP/1.1 200 OK\r\n",

    // IP Address
    .ADDR_LOCALHOST = "127.0.0.1",
    .ADDR_MULTICAST = "239.255.255.250",

    // Log Callback
    .log_callback = NULL
};


// 01. lssdp_set_log_callback
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message)) {
    Global.log_callback = callback;
    return 0;
}

// 02. lssdp_get_network_interface
int lssdp_get_network_interface(lssdp_ctx * lssdp) {
    if (lssdp == NULL) {
        lssdp_error("lssdp should not be NULL\n");
        return -1;
    }

    const size_t SIZE_OF_INTERFACE_LIST = sizeof(struct lssdp_interface) * LSSDP_INTERFACE_LIST_SIZE;

    // copy orginal interface
    struct lssdp_interface original_interface[LSSDP_INTERFACE_LIST_SIZE] = {};
    memcpy(original_interface, lssdp->interface, SIZE_OF_INTERFACE_LIST);

    // reset lssdp->interface
    memset(lssdp->interface, 0, SIZE_OF_INTERFACE_LIST);

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
        char ip[LSSDP_IP_LEN] = {0};  // ip = "xxx.xxx.xxx.xxx"
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
            // set interface
            snprintf(lssdp->interface[num].name, LSSDP_INTERFACE_NAME_LEN, "%s", ifr->ifr_name); // name
            snprintf(lssdp->interface[num].ip,   LSSDP_IP_LEN,             "%s", ip);            // ip string
            lssdp->interface[num].s_addr = addr_in->sin_addr.s_addr;                             // address in network byte order
        }

        // increase interface number
        num++;
    }

    result = 0;
end:
    if (fd >= 0 && close(fd) != 0) {
        lssdp_error("close fd %d failed, errno = %s (%d)\n", strerror(errno), errno);
    }

    // compare with original interface
    if (memcmp(original_interface, lssdp->interface, SIZE_OF_INTERFACE_LIST) != 0) {
        // invoke network interface changed callback
        if (lssdp->network_interface_changed_callback != NULL) {
            lssdp->network_interface_changed_callback(lssdp);
        }
    }

    return result;
}

// 03. lssdp_create_socket
int lssdp_create_socket(lssdp_ctx * lssdp) {
    if (lssdp == NULL) {
        lssdp_error("lssdp should not be NULL\n");
        return -1;
    }

    if (lssdp->sock >= 0) {
        if (close(lssdp->sock) != 0) {
            lssdp_error("close socket %d failed, errno = %s (%d)\n", lssdp->sock, strerror(errno), errno);
            return -1;
        };
        lssdp_debug("close SSDP socket %d\n", lssdp->sock);
        lssdp->sock = -1;
    }

    // create UDP socket
    lssdp->sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (lssdp->sock < 0) {
        lssdp_error("create socket failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    int result = -1;

    // set non-blocking
    int opt = 1;
    if (ioctl(lssdp->sock, FIONBIO, &opt) != 0) {
        lssdp_error("ioctl failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // set reuse address
    if (setsockopt(lssdp->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        lssdp_error("setsockopt SO_REUSEADDR failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // bind socket
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(lssdp->port),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    if (bind(lssdp->sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        lssdp_error("bind failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // set IP_ADD_MEMBERSHIP
    struct ip_mreq imr = {
        .imr_multiaddr.s_addr = inet_addr(Global.ADDR_MULTICAST),
        .imr_interface.s_addr = htonl(INADDR_ANY)
    };
    if (setsockopt(lssdp->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq)) != 0) {
        lssdp_error("setsockopt IP_ADD_MEMBERSHIP failed: %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    lssdp_debug("create SSDP socket %d\n", lssdp->sock);
    result = 0;
end:
    if (result == -1) {
        if (close(lssdp->sock) != 0) {
            lssdp_error("close socket %d failed, errno = %s (%d)\n", lssdp->sock, strerror(errno), errno);
            return -1;
        };
        lssdp->sock = -1;
    }
    return result;
}

// 04. lssdp_read_socket
int lssdp_read_socket(lssdp_ctx * lssdp) {
    char buffer[2048] = {};
    struct sockaddr_in address = {};
    socklen_t address_len = sizeof(struct sockaddr_in);

    ssize_t recv_len = recvfrom(lssdp->sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&address, &address_len);
    if (recv_len == -1) {
        lssdp_error("recvfrom failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    int result = -1;

    // ignore the SSDP packet received from self
    size_t i;
    for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
        if (lssdp->interface[i].s_addr == address.sin_addr.s_addr) {
            result = 0;
            goto end;
        }
    }

    // parse SSDP packet to struct
    lssdp_packet packet = {};
    if (lssdp_packet_parser(buffer, recv_len, &packet) != 0) {
        goto end;
    }

    // check search target
    if (strcmp(packet.st, lssdp->header.st) != 0) {
        // search target is not match
        goto end;
    }

    // M-SEARCH: send RESPONSE back
    if (strcmp(packet.method, Global.MSEARCH) == 0) {
        lssdp_send_response(lssdp, address);
        result = 0;
        goto end;
    }

    // RESPONSE, NOTIFY: add to neighbor_list
    neighbor_list_add(lssdp, packet);

end:
    // invoke packet received callback
    if (lssdp->packet_received_callback != NULL) {
        lssdp->packet_received_callback(lssdp, buffer, recv_len);
    }

    return result;
}

// 05. lssdp_send_msearch
int lssdp_send_msearch(lssdp_ctx * lssdp) {
    // 1. set M-SEARCH packet
    char msearch[1024] = {};
    snprintf(msearch, sizeof(msearch),
        "%s"
        "HOST:%s:%d\r\n"
        "MAN:\"ssdp:discover\"\r\n"
        "ST:%s\r\n"
        "MX:1\r\n"
        "\r\n",
        Global.HEADER_MSEARCH,
        Global.ADDR_MULTICAST, lssdp->port, // HOST
        lssdp->header.st                    // ST (Search Target)
    );

    // 2. send M-SEARCH to each interface
    size_t i;
    for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
        struct lssdp_interface * interface = &lssdp->interface[i];
        if (strlen(interface->name) == 0) {
            break;
        }

        // avoid sending multicast to local host
        if (interface->s_addr == inet_addr(Global.ADDR_LOCALHOST)) {
            continue;
        }

        send_multicast_data(msearch, *interface, lssdp->port);
    }
    return 0;
}

// 06. lssdp_send_notify
int lssdp_send_notify(lssdp_ctx * lssdp) {
    // 1. set location suffix
    char suffix[256] = {0};
    const int port = lssdp->header.location.port;
    if (0 < port && port <= 0xFFFF) {
        sprintf(suffix, ":%d", port);
    }
    const char * uri = lssdp->header.location.uri;
    if (strlen(uri) > 0) {
        sprintf(suffix + strlen(suffix), "/%s", uri);
    }

    // 2. send NOTIFY to each interface
    size_t i;
    for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
        struct lssdp_interface * interface = &lssdp->interface[i];
        if (strlen(interface->name) == 0) {
            break;
        }

        // avoid sending multicast to local host
        if (interface->s_addr == inet_addr(Global.ADDR_LOCALHOST)) {
            continue;
        }

        // set notify packet
        char notify[1024] = {};
        char * host = lssdp->header.location.host;
        snprintf(notify, sizeof(notify),
            "%s"
            "HOST:%s:%d\r\n"
            "CACHE-CONTROL:max-age=120\r\n"
            "ST:%s\r\n"
            "USN:%s\r\n"
            "LOCATION:%s%s\r\n"
            "SM_ID:%s\r\n"
            "DEV_TYPE:%s\r\n"
            "NTS:ssdp:alive\r\n"
            "\r\n",
            Global.HEADER_NOTIFY,
            Global.ADDR_MULTICAST, lssdp->port,              // HOST
            lssdp->header.st,                                // ST
            lssdp->header.usn,                               // USN
            strlen(host) > 0 ? host : interface->ip, suffix, // LOCATION
            lssdp->header.sm_id,                             // SM_ID    (addtional field)
            lssdp->header.device_type                        // DEV_TYPE (addtional field)
        );

        send_multicast_data(notify, *interface, lssdp->port);
    }
    return 0;
}

// 07. lssdp_check_neighbor_timeout
int lssdp_check_neighbor_timeout(lssdp_ctx * lssdp) {
    long current_time = get_current_time();
    if (current_time < 0) {
        return -1;
    }

    lssdp_nbr * prev = NULL;
    lssdp_nbr * nbr;
    for (nbr = lssdp->neighbor_list; nbr != NULL; nbr = nbr->next) {
        long pass_time = current_time - nbr->update_time;
        if (pass_time < lssdp->neighbor_timeout) {
            prev = nbr;
            continue;
        }

        lssdp_warn("remove timeout SSDP neighbor: %s (%s) (%ldms)\n", nbr->sm_id, nbr->location, pass_time);

        if (prev == NULL) {
            // it's first neighbor in list
            lssdp->neighbor_list = nbr->next;
        } else {
            prev->next = nbr->next;
        }
        free(nbr);

        // invoke neighbor list changed callback
        if (lssdp->neighbor_list_changed_callback != NULL) {
            lssdp->neighbor_list_changed_callback(lssdp);
        }
    }
    return 0;
}


/** Internal Function **/

static int send_multicast_data(const char * data, const struct lssdp_interface interface, int ssdp_port) {
    if (data == NULL) {
        lssdp_error("data should not be NULL\n");
        return -1;
    }

    size_t data_len = strlen(data);
    if (data_len == 0) {
        lssdp_error("data length should not be empty\n");
        return -1;
    }

    if (strlen(interface.name) == 0) {
        lssdp_error("interface.name should not be empty\n");
        return -1;
    }

    if (ssdp_port < 0 || ssdp_port > 0xFFFF) {
        lssdp_error("ssdp_port (%d) is invalid\n");
        return -1;
    }

    int result = -1;

    // 1. create UDP socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        lssdp_error("create socket failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // 2. bind socket
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = interface.s_addr
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lssdp_error("bind failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // 3. disable IP_MULTICAST_LOOP
    char opt = 0;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &opt, sizeof(opt)) < 0) {
        lssdp_error("setsockopt IP_MULTICAST_LOOP failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // 4. set destination address
    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ssdp_port)
    };
    if (inet_aton(Global.ADDR_MULTICAST, &dest_addr.sin_addr) == 0) {
        lssdp_error("inet_aton failed, errno = %s (%d)\n", strerror(errno), errno);
        goto end;
    }

    // 5. send data
    if (sendto(fd, data, data_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
        lssdp_error("sendto %s (%s) failed, errno = %s (%d)\n", interface.name, interface.ip, strerror(errno), errno);
        goto end;
    }

    result = 0;
end:
    if (fd >= 0 && close(fd) != 0) {
        lssdp_error("close fd %d failed, errno = %s (%d)\n", strerror(errno), errno);
    }
    return result;
}

static int lssdp_send_response(lssdp_ctx * lssdp, struct sockaddr_in address) {
    // get M-SEARCH IP
    char msearch_ip[LSSDP_IP_LEN] = {0};
    if (inet_ntop(AF_INET, &address.sin_addr, msearch_ip, sizeof(msearch_ip)) == NULL) {
        lssdp_error("inet_ntop failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    /* 1. find the interface which is in LAN
     *    e.g:
     *         192.168.1.x -> 192.168.1.y (in LAN)
     *         192.168.1.x -> 10.5.2.y    (not in LAN)
     */
    uint32_t mask = 0x00ffffff;
    struct lssdp_interface * interface = NULL;
    int i;
    for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
        if ((lssdp->interface[i].s_addr & mask) == (address.sin_addr.s_addr & mask)) {
            interface = &lssdp->interface[i];
            break;
        }
    }

    // interface is not found
    if (interface == NULL) {
        lssdp_error("M-SEARCH Packet IP (%s) is not exist in Local Area Network!\n", msearch_ip);
        show_network_interface(lssdp);
        return -1;
    }

    // 2. set location suffix
    char suffix[256] = {0};
    const int port = lssdp->header.location.port;
    if (0 < port && port <= 0xFFFF) {
        sprintf(suffix, ":%d", port);
    }
    const char * uri = lssdp->header.location.uri;
    if (strlen(uri) > 0) {
        sprintf(suffix + strlen(suffix), "/%s", uri);
    }

    // 3. set response packet
    char response[1024] = {};
    char * host = lssdp->header.location.host;
    int response_len = snprintf(response, sizeof(response),
        "%s"
        "CACHE-CONTROL:max-age=120\r\n"
        "DATE:\r\n"
        "EXT:\r\n"
        "LOCATION:%s%s\r\n"
        "SERVER:OS/version UPnP/1.1 product/version\r\n"
        "ST:%s\r\n"
        "USN:%s\r\n"
        "SM_ID:%s\r\n"
        "DEV_TYPE:%s\r\n"
        "\r\n",
        Global.HEADER_RESPONSE,
        strlen(host) > 0 ? host : interface->ip, suffix, // LOCATION
        lssdp->header.st,                                // ST
        lssdp->header.usn,                               // USN
        lssdp->header.sm_id,                             // SM_ID    (addtional field)
        lssdp->header.device_type                        // DEV_TYPE (addtional field)
    );

    // 4. set port to address
    address.sin_port = htons(lssdp->port);

    // 5. send data
    if (sendto(lssdp->sock, response, response_len, 0, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) == -1) {
        lssdp_error("send RESPONSE to %s failed, errno = %s (%d)\n", msearch_ip, strerror(errno), errno);
        return -1;
    }

    return 0;
}

static int lssdp_packet_parser(const char * data, size_t data_len, lssdp_packet * packet) {
    if (data == NULL) {
        lssdp_error("data should not be NULL\n");
        return -1;
    }

    if (data_len != strlen(data)) {
        lssdp_error("data_len (%zu) is not match to the data length (%zu)\n", data_len, strlen(data));
        return -1;
    }

    if (packet == NULL) {
        lssdp_error("packet should not be NULL\n");
        return -1;
    }

    // 1. compare SSDP Method Header: M-SEARCH, NOTIFY, RESPONSE
    size_t i;
    if ((i = strlen(Global.HEADER_MSEARCH)) < data_len && memcmp(data, Global.HEADER_MSEARCH, i) == 0) {
        strcpy(packet->method, Global.MSEARCH);
    } else if ((i = strlen(Global.HEADER_NOTIFY)) < data_len && memcmp(data, Global.HEADER_NOTIFY, i) == 0) {
        strcpy(packet->method, Global.NOTIFY);
    } else if ((i = strlen(Global.HEADER_RESPONSE)) < data_len && memcmp(data, Global.HEADER_RESPONSE, i) == 0) {
        strcpy(packet->method, Global.RESPONSE);
    } else {
        lssdp_warn("received unknown SSDP packet\n");
        lssdp_debug("%s\n", data);
        return -1;
    }

    // 2. parse each field line
    size_t start = i;
    for (i = start; i < data_len; i++) {
        if (data[i] == '\n' && i - 1 > start && data[i - 1] == '\r') {
            parse_field_line(data, start, i - 2, packet);
            start = i + 1;
        }
    }

    // 3. set update_time
    long current_time = get_current_time();
    if (current_time < 0) {
        return -1;
    }
    packet->update_time = current_time;
    return 0;
}

static int parse_field_line(const char * data, size_t start, size_t end, lssdp_packet * packet) {
    // 1. find the colon
    if (data[start] == ':') {
        lssdp_warn("the first character of line should not be colon\n");
        lssdp_debug("%s\n", data);
        return -1;
    }

    int colon = get_colon_index(data, start + 1, end);
    if (colon == -1) {
        lssdp_warn("there is no colon in line\n");
        lssdp_debug("%s\n", data);
        return -1;
    }

    if (colon == end) {
        // value is empty
        return -1;
    }


    // 2. get field, field_len
    size_t i = start;
    size_t j = colon - 1;
    if (trim_spaces(data, &i, &j) == -1) {
        return -1;
    }
    const char * field = &data[i];
    size_t field_len = j - i + 1;


    // 3. get value, value_len
    i = colon + 1;
    j = end;
    if (trim_spaces(data, &i, &j) == -1) {
        return -1;
    };
    const char * value = &data[i];
    size_t value_len = j - i + 1;


    // 4. set each field's value to packet
    if (field_len == strlen("st") && strncasecmp(field, "st", field_len) == 0) {
        memcpy(packet->st, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
        return 0;
    }

    if (field_len == strlen("usn") && strncasecmp(field, "usn", field_len) == 0) {
        memcpy(packet->usn, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
        return 0;
    }

    if (field_len == strlen("location") && strncasecmp(field, "location", field_len) == 0) {
        memcpy(packet->location, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
        return 0;
    }

    if (field_len == strlen("sm_id") && strncasecmp(field, "sm_id", field_len) == 0) {
        memcpy(packet->sm_id, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
        return 0;
    }

    if (field_len == strlen("dev_type") && strncasecmp(field, "dev_type", field_len) == 0) {
        memcpy(packet->device_type, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
        return 0;
    }

    // the field is not in the struct packet
    return 0;
}

static int get_colon_index(const char * string, size_t start, size_t end) {
    size_t i;
    for (i = start; i <= end; i++) {
        if (string[i] == ':') {
            return i;
        }
    }
    return -1;
}

static int trim_spaces(const char * string, size_t * start, size_t * end) {
    int i = *start;
    int j = *end;

    while (i <= *end   && (!isprint(string[i]) || isspace(string[i]))) i++;
    while (j >= *start && (!isprint(string[j]) || isspace(string[j]))) j--;

    if (i > j) {
        return -1;
    }

    *start = i;
    *end   = j;
    return 0;
}

static long get_current_time() {
    struct timeval time = {};
    if (gettimeofday(&time, NULL) == -1) {
        lssdp_error("gettimeofday failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }
    return (time.tv_sec * 1000) + (time.tv_usec / 1000);
}

static int lssdp_log(const char * level, int line, const char * func, const char * format, ...) {
    if (Global.log_callback == NULL) {
        return -1;
    }

    char message[LSSDP_MESSAGE_MAX_LEN] = {0};

    // create message by va_list
    va_list args;
    va_start(args, format);
    vsnprintf(message, LSSDP_MESSAGE_MAX_LEN, format, args);
    va_end(args);

    // invoke log callback function
    Global.log_callback(__FILE__, "SSDP", level, line, func, message);
    return 0;
}

static int show_network_interface(lssdp_ctx * lssdp) {
    lssdp_debug("Network Interface List:\n");
    lssdp_debug("-------------------------\n");
    int i;
    for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
        if (strlen(lssdp->interface[i].name) == 0) {
            // there is no more interface in list
            break;
        }

        lssdp_debug("%d. %s : %s\n", i + 1, lssdp->interface[i].name, lssdp->interface[i].ip);
    }
    if (i == 0) lssdp_debug("Empty!\n");
    lssdp_debug("-------------------------\n");
    return 0;
}

static int neighbor_list_add(lssdp_ctx * lssdp, const lssdp_packet packet) {
    lssdp_nbr * last_nbr = lssdp->neighbor_list;

    lssdp_nbr * nbr;
    for (nbr = lssdp->neighbor_list; nbr != NULL; last_nbr = nbr, nbr = nbr->next) {
        if (strcmp(nbr->location, packet.location) != 0) {
            // location is not match
            continue;
        }

        /* location is not found in SSDP list: update neighbor */

        // usn
        if (strcmp(nbr->usn, packet.usn) != 0) {
            lssdp_warn("neighbor usn was changed. %s -> %s\n", nbr->usn, packet.usn);
            memcpy(nbr->usn, packet.usn, LSSDP_FIELD_LEN);
        }

        // sm_id
        if (strcmp(nbr->sm_id, packet.sm_id) != 0) {
            lssdp_warn("neighbor sm_id was changed. %s -> %s\n", nbr->sm_id, packet.sm_id);
            memcpy(nbr->sm_id, packet.sm_id, LSSDP_FIELD_LEN);
        }

        // device type
        if (strcmp(nbr->device_type, packet.device_type) != 0) {
            lssdp_warn("neighbor device_type was changed. %s -> %s\n", nbr->device_type, packet.device_type);
            memcpy(nbr->device_type, packet.device_type, LSSDP_FIELD_LEN);
        }

        // update_time
        nbr->update_time = packet.update_time;
        return 0;
    }


    /* location is not found in SSDP list: add to list */

    // 1. memory allocate lssdp_nbr
    nbr = (lssdp_nbr *) malloc(sizeof(lssdp_nbr));
    if (nbr == NULL) {
        lssdp_error("malloc failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    // 2. setup neighbor
    memcpy(nbr->usn,         packet.usn,         LSSDP_FIELD_LEN);
    memcpy(nbr->sm_id,       packet.sm_id,       LSSDP_FIELD_LEN);
    memcpy(nbr->device_type, packet.device_type, LSSDP_FIELD_LEN);
    memcpy(nbr->location,    packet.location,    LSSDP_FIELD_LEN);
    nbr->update_time = packet.update_time;
    nbr->next = NULL;

    // 3. add neighbor to the end of list
    if (last_nbr == NULL) {
        // it's the first neighbor
        lssdp->neighbor_list = nbr;
    } else {
        last_nbr->next = nbr;
    }

    // invoke neighbor list changed callback
    if (lssdp->neighbor_list_changed_callback != NULL) {
        lssdp->neighbor_list_changed_callback(lssdp);
    }

    return 0;
}
