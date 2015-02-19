#ifndef __LSSDP_H
#define __LSSDP_H

/*
 * 01. lssdp_set_log_callback
 *
 * @param callback
 * @return = 0    success
 */
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message));

// Dummy Function
void lssdp_hello();

#endif
