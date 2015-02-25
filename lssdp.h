#ifndef __LSSDP_H
#define __LSSDP_H

#include <stdbool.h>
#include <stdint.h>     // uint32_t

/* Struct : lssdp_nbr */
#define LSSDP_FIELD_LEN         128
#define LSSDP_LOCATION_LEN      256
typedef struct lssdp_nbr {
    char            usn         [LSSDP_FIELD_LEN];          // Unique Service Name (Device Name or MAC)
    char            location    [LSSDP_LOCATION_LEN];       // URL or IP(:Port)

    /* Additional SSDP Header Fields */
    char            sm_id       [LSSDP_FIELD_LEN];
    char            device_type [LSSDP_FIELD_LEN];
    unsigned long   update_time;
    struct lssdp_nbr * next;
} lssdp_nbr;


/* Struct : lssdp_ctx */
#define LSSDP_INTERFACE_NAME_LEN    16                      // IFNAMSIZ
#define LSSDP_INTERFACE_LIST_SIZE   16
#define LSSDP_IP_LEN                16
typedef struct lssdp_ctx {
    int             sock;                                   // SSDP socket
    int             port;                                   // SSDP port
    lssdp_nbr *     neighbor_list;                          // SSDP neighbor list
    long            neighbor_timeout;                       // milliseconds
    bool            debug;                                  // show debug log

    /* Network Interface */
    struct lssdp_interface {
        char        name        [LSSDP_INTERFACE_NAME_LEN]; // name[16]
        char        ip          [LSSDP_IP_LEN];             // ip[16] = "xxx.xxx.xxx.xxx"
        uint32_t    s_addr;                                 // address in network byte order
    } interface[LSSDP_INTERFACE_LIST_SIZE];                 // interface[16]

    /* SSDP Header Fields */
    struct {
        char        st          [LSSDP_FIELD_LEN];          // Search Target
        char        usn         [LSSDP_FIELD_LEN];          // Unique Service Name

        // Location (optional)
        struct {
            char    prefix      [LSSDP_FIELD_LEN];          // Protocal: "https://" or "http://"
            char    domain      [LSSDP_FIELD_LEN];          // if domain is empty, using Interface IP as default
            char    suffix      [LSSDP_FIELD_LEN];          // URI or Port: "/index.html" or ":80"
        } location;

        /* Additional SSDP Header Fields */
        char        sm_id       [LSSDP_FIELD_LEN];
        char        device_type [LSSDP_FIELD_LEN];
    } header;

    /* Callback Function */
    int (* network_interface_changed_callback) (struct lssdp_ctx * lssdp);
    int (* neighbor_list_changed_callback)     (struct lssdp_ctx * lssdp);
    int (* packet_received_callback)           (struct lssdp_ctx * lssdp, const char * packet, size_t packet_len);

} lssdp_ctx;


/*
 * 01. lssdp_get_network_interface
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_get_network_interface(lssdp_ctx * lssdp);

/*
 * 02. lssdp_create_socket
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_create_socket(lssdp_ctx * lssdp);

/*
 * 03. lssdp_send_msearch
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_send_msearch(lssdp_ctx * lssdp);

/*
 * 04. lssdp_send_notify
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_send_notify(lssdp_ctx * lssdp);

/*
 * 05. lssdp_read_socket
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_read_socket(lssdp_ctx * lssdp);

/*
 * 06. lssdp_check_neighbor_timeout
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_check_neighbor_timeout(lssdp_ctx * lssdp);

/*
 * 07. lssdp_set_log_callback
 *
 * @param callback
 * @return = 0    success
 */
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message));

#endif
