#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#ifndef NO_GUI
#include "gui.h"
#endif



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
#ifdef NO_GUI
        fprintf(stderr, "GUI support was not included in this build. Run `make gui` to build it.\n");
        simShutdown();
        return 1;
#else
        guiMode = 1;
        guiRun();
#endif
    } else {

        while (simStep())
            ;
    }

    simShutdown();
    return 0;
}
