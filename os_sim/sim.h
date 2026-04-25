#ifndef SIM_H
#define SIM_H

#include "pcb.h"
#include "queue.h"


#define MAX_LOG_LINES 200
#define LOG_LINE_LEN  256


typedef struct {
    int   clock;
    int   running;
    int   hasError;
    int   waitingForInput;
    int   inputAutoRelease;
    char  inputPrompt[128];
    char  inputVarName[64];
    PCB*  inputProcess;

    char  schedulerMode[8];
    PCB*  currentlyRunning;
    char  currentInstr[256];
    char  lastError[LOG_LINE_LEN];


    char  log[MAX_LOG_LINES][LOG_LINE_LEN];
    int   logCount;
    int   logHead;
    int   logSerial;
} SimState;

extern SimState simState;
extern Queue readyQ;
extern Queue blockedQ;
extern int guiMode;


void simInit(const char* mode);


int simStep(void);


void simProvideInput(const char* value);


void simReset(const char* mode);


void simShutdown(void);


void simLog(const char* fmt, ...);


void simFail(const char* fmt, ...);


void simEnqueueReady(PCB* p);
void simMarkFinished(PCB* p);
void simSetCurrentInstruction(PCB* p, const char* instr);
void simClearCurrentInstruction(void);

#endif
