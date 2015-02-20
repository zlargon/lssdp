#ifndef __LSSDP_H
#define __LSSDP_H

/*
 * 01. lssdp_set_log_callback
 *
 * @param callback
 * @return = 0    success
 */
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message));

/* struct : lssdp_interface */
#define LSSDP_INTERFACE_NAME_LEN    16  // IFNAMSIZ
typedef struct lssdp_interface {
    char            name    [LSSDP_INTERFACE_NAME_LEN];
    unsigned char   ip      [4];        // ip = [ 127, 0, 0, 1 ]
} lssdp_interface;

/*
 * 02. lssdp_get_interface_list
 *
 * @param interface_list
 * @param list_size
 * @return = 0      success
 *         < 0      failed
 */
int lssdp_get_interface_list(lssdp_interface interface_list[], size_t list_size);

#endif
