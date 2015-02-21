#ifndef __LSSDP_H
#define __LSSDP_H

/* struct : lssdp_ctx */
#define LSSDP_INTERFACE_NAME_LEN    16  // IFNAMSIZ
#define LSSDP_INTERFACE_LIST_SIZE   16

typedef struct lssdp_ctx {
    // Network Interface
    struct lssdp_interface {
        char            name    [LSSDP_INTERFACE_NAME_LEN]; // name[16]
        unsigned char   ip      [4];                        // ip = [ 127, 0, 0, 1 ]
    } interface[LSSDP_INTERFACE_LIST_SIZE];                 // interface[16]

    int port;
    int sock;

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

#endif
