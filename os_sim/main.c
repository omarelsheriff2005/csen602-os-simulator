#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "gui.h"

/*
 * Usage:
 *   ./os_sim rr            Terminal mode, Round Robin
 *   ./os_sim hrrn          Terminal mode, HRRN
 *   ./os_sim mlfq          Terminal mode, MLFQ
 *   ./os_sim gui            GUI mode, default RR (switchable in GUI)
 *   ./os_sim gui rr         GUI mode, start with RR
 */

int main(int argc, char* argv[]) {
    int useGui = 0;
    const char* mode = "rr";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "gui") == 0)
            useGui = 1;
        else if (strcmp(argv[i], "rr") == 0 ||
                 strcmp(argv[i], "hrrn") == 0 ||
                 strcmp(argv[i], "mlfq") == 0)
            mode = argv[i];
    }

    simInit(mode);

    if (useGui) {
        guiMode = 1;
        guiRun();
    } else {
        /* Terminal mode — run simulation to completion */
        while (simStep())
            ;
    }

    simShutdown();
    return 0;
}
