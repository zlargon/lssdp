#ifndef __LSSDP_H
#define __LSSDP_H

#include <stdint.h>     // uint32_t

/* struct : lssdp_nbr */
#define LSSDP_FIELD_LEN     256
typedef struct lssdp_nbr {
    char            name        [LSSDP_FIELD_LEN];          // Device Name (or MAC)
    char            sm_id       [LSSDP_FIELD_LEN];
    char            device_type [LSSDP_FIELD_LEN];
    char            location    [LSSDP_FIELD_LEN];          // URL or IP(:Port)
    unsigned long   update_time;
    struct lssdp_nbr * next;
} lssdp_nbr;

/* struct : lssdp_ctx */
#define LSSDP_INTERFACE_NAME_LEN    16  // IFNAMSIZ
#define LSSDP_INTERFACE_LIST_SIZE   16
#define LSSDP_IP_LEN                16
typedef struct lssdp_ctx {
    int             sock;                                   // SSDP socket
    int             port;                                   // SSDP port
    lssdp_nbr *     neighbor_list;                          // SSDP neighbor list

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

        // Location = host + [:port] + [/uri]
        struct {
            char    host        [LSSDP_FIELD_LEN];          // optional, if host is empty, using each interface IP as default
            int     port;                                   // optional
            char    uri         [LSSDP_FIELD_LEN];          // optional
        } location;

        /* Additional SSDP Header Fields */
        char        sm_id       [LSSDP_FIELD_LEN];
        char        device_type [LSSDP_FIELD_LEN];
    } header;

    /* Callback Function */
    int (* data_callback)(const struct lssdp_ctx * lssdp, const char * data, size_t data_len);

} lssdp_ctx;

/*
 * 01. lssdp_set_log_callback
 *
 * @param callback
 * @return = 0    success
 */
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message));

/*
 * 02. lssdp_get_network_interface
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_get_network_interface(lssdp_ctx * lssdp);

/*
 * 03. lssdp_create_socket
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_create_socket(lssdp_ctx * lssdp);

/*
 * 04. lssdp_read_socket
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_read_socket(lssdp_ctx * lssdp);

/*
 * 05. lssdp_send_msearch
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_send_msearch(lssdp_ctx * lssdp);

/*
 * 06. lssdp_send_notify
 *
 * @param lssdp
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_send_notify(lssdp_ctx * lssdp);

#endif
