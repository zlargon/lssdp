#include <stdio.h>
#include "lssdp.h"

/** Global Variable **/
static int (* lssdp_log_callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) = NULL;


// 01. lssdp_set_log_callback
int lssdp_set_log_callback(int (* callback)(const char * file, const char * tag, const char * level, int line, const char * func, const char * message)) {
    lssdp_log_callback = callback;
    return 0;
}

// Dummy Function
void lssdp_hello() {
#ifdef __OSX__
    puts("hello OSX");
#else
    puts("hello");
#endif
}
