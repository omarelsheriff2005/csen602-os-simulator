#ifndef SIM_H
#define SIM_H

#include "pcb.h"
#include "queue.h"

/* Maximum log entries kept for GUI display */
#define MAX_LOG_LINES 200
#define LOG_LINE_LEN  256

/* Simulation state — accessible by GUI for rendering */
typedef struct {
    int   clock;
    int   running;            /* 1 = simulation active, 0 = all done */
    int   hasError;           /* 1 = at least one error occurred */
    int   waitingForInput;    /* 1 = blocked waiting for user input */
    char  inputPrompt[128];   /* prompt text when waiting */
    char  inputVarName[64];   /* variable name to assign input to */
    PCB*  inputProcess;       /* process that requested input */

    char  schedulerMode[8];   /* "rr", "hrrn", "mlfq" */
    PCB*  currentlyRunning;   /* process currently executing (NULL if none) */
    char  currentInstr[256];  /* last executed instruction text */
    char  lastError[LOG_LINE_LEN];

    /* Log buffer (circular) */
    char  log[MAX_LOG_LINES][LOG_LINE_LEN];
    int   logCount;
    int   logHead;            /* index of oldest entry */
    int   logSerial;          /* total log lines ever written */
} SimState;

extern SimState simState;
extern Queue readyQ;
extern Queue blockedQ;
extern int guiMode;  /* 0 = terminal, 1 = GUI */

/* Initialise the simulation (call once) */
void simInit(const char* mode);

/* Advance one clock cycle. Returns 0 if simulation ended. */
int simStep(void);

/* Provide input value when simState.waitingForInput == 1 */
void simProvideInput(const char* value);

/* Reset simulation to start */
void simReset(const char* mode);

/* Free simulator-owned resources */
void simShutdown(void);

/* Add a line to the log buffer */
void simLog(const char* fmt, ...);

/* Stop the simulation with a fatal error message */
void simFail(const char* fmt, ...);

/* Queue helpers that respect the active scheduler */
void simEnqueueReady(PCB* p);
void simMarkFinished(PCB* p);
void simSetCurrentInstruction(PCB* p, const char* instr);
void simClearCurrentInstruction(void);

#endif
