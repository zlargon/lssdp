#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "lssdp.h"

/* network_interface.c
 *
 * 1. call lssdp_network_interface_update per second
 * 2. when network interface is changed, show interface list
 */

void log_callback(const char * file, const char * tag, int level, int line, const char * func, const char * message) {
    char * level_name = "DEBUG";
    if (level == LSSDP_LOG_INFO)   level_name = "INFO";
    if (level == LSSDP_LOG_WARN)   level_name = "WARN";
    if (level == LSSDP_LOG_ERROR)  level_name = "ERROR";

    printf("[%-5s][%s] %s", level_name, tag, message);
}

int show_interface_list(lssdp_ctx * lssdp) {
    printf("\nNetwork Interface List (%zu):\n", lssdp->interface_num);
    size_t i;
    for (i = 0; i < lssdp->interface_num; i++) {
        printf("%zu. %-6s: %-15s (%d.%d.%d.%d)\n",
            i + 1,
            lssdp->interface[i].name,
            lssdp->interface[i].ip,
            (lssdp->interface[i].netmask >> 0)  & 0xff,
            (lssdp->interface[i].netmask >> 8)  & 0xff,
            (lssdp->interface[i].netmask >> 16) & 0xff,
            (lssdp->interface[i].netmask >> 24) & 0xff
        );
    }
    printf("%s\n", i == 0 ? "Empty" : "");
    return 0;
}

int main() {
    lssdp_set_log_callback(log_callback);

    lssdp_ctx lssdp = {
        .network_interface_changed_callback = show_interface_list
    };

    // Main Loop
    for (;; sleep(1)) {
        puts(".");
        lssdp_network_interface_update(&lssdp);
    }

    return EXIT_SUCCESS;
}
