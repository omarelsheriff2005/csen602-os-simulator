#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "sim.h"
#include "memory.h"
#include "pcb.h"
#include "queue.h"
#include "mutex.h"
#include "scheduler.h"

/* ── GUI state ───────────────────────────────────────────────── */

static Font guiFont;
static int  fontLoaded = 0;

static int   autoRun       = 0;
static float autoSpeed     = 2.0f;
static float autoTimer     = 0.0f;
static int   memScroll     = 0;
static int   logScroll     = 0;
static int   procScroll    = 0;
static char  inputBuf[256] = "";
static int   inputActive   = 0;
#define GUI_MAX_PROC 32
static float swapFlashT[GUI_MAX_PROC] = {0};   /* per-process highlight timer */

/* ── Text helpers (use TTF if loaded) ────────────────────────── */

static void drawText(const char* text, int x, int y, int size, Color c) {
    if (fontLoaded)
        DrawTextEx(guiFont, text, (Vector2){(float)x, (float)y}, (float)size, 0.5f, c);
    else
        DrawText(text, x, y, size, c);
}

static int measureText(const char* text, int size) {
    if (fontLoaded)
        return (int)MeasureTextEx(guiFont, text, (float)size, 0.5f).x;
    return MeasureText(text, size);
}

/* Vertically centered text inside a row of height rowH */
static void drawTextV(const char* text, int x, int y, int rowH, int size, Color c) {
    drawText(text, x, y + (rowH - size) / 2, size, c);
}

/* ── Primitives ──────────────────────────────────────────────── */

static Color stateColor(ProcessState s) {
    switch (s) {
        case READY:    return CLR_READY;
        case RUNNING:  return CLR_RUNNING;
        case BLOCKED:  return CLR_BLOCKED;
        case FINISHED: return CLR_FINISHED;
        default:       return CLR_TEXT_DIM;
    }
}

static const char* stateShort(ProcessState s) {
    switch (s) {
        case READY:    return "READY";
        case RUNNING:  return "RUN";
        case BLOCKED:  return "BLOCK";
        case FINISHED: return "DONE";
        default:       return "NEW";
    }
}

static void drawPanel(int x, int y, int w, int h, const char* title) {
    /* body */
    DrawRectangle(x, y, w, h, CLR_PANEL);
    /* header */
    DrawRectangle(x, y, w, PANEL_HDR_H, CLR_PANEL_HDR);
    /* accent line */
    DrawRectangle(x, y + PANEL_HDR_H, w, 1, CLR_PANEL_BDR);
    /* title */
    drawTextV(title, x + 12, y, PANEL_HDR_H, FONT_H1, CLR_ACCENT);
    /* border */
    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h}, 1, CLR_PANEL_BDR);
}

/* A pill badge (rounded chip) with centered text */
static int drawPill(int x, int y, int h, const char* text, Color bg, Color fg, int padX) {
    int tw = measureText(text, FONT_SMALL);
    int w  = tw + padX * 2;
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.45f, 6, bg);
    drawTextV(text, x + padX, y, h, FONT_SMALL, fg);
    return w;
}

/* A button: returns 1 when clicked */
static int drawButton(int x, int y, int w, int h, const char* label, Color bg, Color hover, Color fg, int enabled) {
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    Vector2 m = GetMousePosition();
    int isHover = enabled && CheckCollisionPointRec(m, r);
    Color bc = isHover ? hover : bg;
    if (!enabled) { bc = (Color){45, 48, 62, 255}; fg = CLR_TEXT_MUTED; }

    DrawRectangleRounded(r, 0.25f, 6, bc);
    int tw = measureText(label, FONT_BODY);
    drawTextV(label, x + (w - tw) / 2, y, h, FONT_BODY, fg);

    return enabled && isHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* Draw a horizontal list of process pills for a queue */
static void drawQueueChips(int x, int y, int maxW, int rowH, Queue* q) {
    if (q->head == NULL) {
        drawTextV("-- empty --", x, y, rowH, FONT_SMALL, CLR_TEXT_MUTED);
        return;
    }
    int tx = x;
    QNode* node = q->head;
    while (node && tx < x + maxW - 40) {
        char buf[16];
        snprintf(buf, sizeof(buf), "P%d", node->pcb->pid);
        int pw = drawPill(tx, y + (rowH - 22) / 2, 22, buf, stateColor(node->pcb->state), CLR_BG, 10);
        tx += pw + 6;
        if (node->next) {
            drawTextV(">", tx, y, rowH, FONT_SMALL, CLR_TEXT_MUTED);
            tx += 12;
        }
        node = node->next;
    }
}

/* ── TOP BAR (title + clock + scheduler toggle) ───────────────── */

static void drawTopBar(int w) {
    DrawRectangle(0, 0, w, HDR_H, CLR_PANEL_HDR);
    DrawRectangle(0, HDR_H, w, 1, CLR_PANEL_BDR);

    /* Title (left) */
    drawTextV("OS Simulation", PAD + 4, 0, HDR_H, FONT_TITLE, CLR_TEXT);
    int tw = measureText("OS Simulation", FONT_TITLE);
    drawTextV("CSEN 602", PAD + 4 + tw + 10, 0, HDR_H, FONT_BODY, CLR_TEXT_DIM);

    /* Scheduler toggle (center) */
    const char* modes[]  = {"rr", "hrrn", "mlfq"};
    const char* labels[] = {"Round Robin", "HRRN", "MLFQ"};
    int tgW = 360;
    int tgX = (w - tgW) / 2;
    int tgY = (HDR_H - 32) / 2;
    drawTextV("Scheduler:", tgX - 90, 0, HDR_H, FONT_BODY, CLR_TEXT_DIM);

    for (int i = 0; i < 3; i++) {
        int bw = 115;
        int bx = tgX + i * (bw + 6);
        int selected = (strcmp(simState.schedulerMode, modes[i]) == 0);
        Color bg = selected ? CLR_ACCENT : CLR_BTN_BG;
        Color fg = selected ? CLR_BG     : CLR_TEXT_DIM;
        Color hv = selected ? CLR_ACCENT_HI : CLR_BTN_HOVER;

        int canSwitch = (!simState.running || simState.clock == 0);
        if (drawButton(bx, tgY, bw, 32, labels[i], bg, hv, fg, canSwitch))
            simReset(modes[i]);
    }

    /* Clock (right) */
    char clk[32];
    snprintf(clk, sizeof(clk), "CLOCK  %04d", simState.clock);
    int cw = measureText(clk, FONT_TITLE);
    drawTextV(clk, w - cw - PAD - 4, 0, HDR_H, FONT_TITLE, CLR_ACCENT);
}

/* ── CONTROL STRIP ────────────────────────────────────────────── */

static void drawControlStrip(int y, int w) {
    DrawRectangle(0, y, w, CTRL_H, CLR_PANEL);
    DrawRectangle(0, y, w, 1, CLR_PANEL_BDR);
    DrawRectangle(0, y + CTRL_H - 1, w, 1, CLR_PANEL_BDR);

    int bx = PAD;
    int by = y + (CTRL_H - 36) / 2;
    int bh = 36;

    int canStep = simState.running && !simState.waitingForInput;

    /* STEP */
    if (drawButton(bx, by, 88, bh, "STEP", CLR_ACCENT, CLR_ACCENT_HI, CLR_BG, canStep)) {
        autoRun = 0;
        simStep();
    }
    bx += 88 + GAP;

    /* RUN / PAUSE toggle */
    if (autoRun) {
        if (drawButton(bx, by, 88, bh, "PAUSE", CLR_BTN_BG, CLR_BTN_HOVER, CLR_TEXT, 1))
            autoRun = 0;
    } else {
        if (drawButton(bx, by, 88, bh, "RUN", CLR_READY, (Color){120,220,150,255}, CLR_BG, canStep))
            autoRun = 1;
    }
    bx += 88 + GAP;

    /* RESET */
    if (drawButton(bx, by, 88, bh, "RESET", CLR_BTN_DANGER, (Color){170, 70, 80, 255}, CLR_TEXT, 1)) {
        autoRun = 0;
        simReset(simState.schedulerMode);
    }
    bx += 88 + PAD * 2;

    /* Speed slider */
    drawTextV("SPEED", bx, y, CTRL_H, FONT_SMALL, CLR_TEXT_DIM);
    bx += 54;
    Rectangle slide = {(float)bx, (float)(by + 10), 240, 16};
    /* Track */
    DrawRectangleRounded(slide, 0.5f, 4, CLR_BTN_BG);
    /* Fill */
    float t = (autoSpeed - 0.5f) / (20.0f - 0.5f);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    DrawRectangleRounded((Rectangle){slide.x, slide.y, slide.width * t, slide.height}, 0.5f, 4, CLR_ACCENT);
    /* Knob */
    float kx = slide.x + slide.width * t;
    DrawCircle((int)kx, (int)(slide.y + slide.height / 2), 9, CLR_ACCENT_HI);

    /* Interaction */
    Vector2 m = GetMousePosition();
    Rectangle hit = {slide.x - 8, slide.y - 10, slide.width + 16, slide.height + 20};
    if (CheckCollisionPointRec(m, hit) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float nt = (m.x - slide.x) / slide.width;
        if (nt < 0) nt = 0;
        if (nt > 1) nt = 1;
        autoSpeed = 0.5f + nt * (20.0f - 0.5f);
    }
    bx += 240 + GAP;
    char stext[16];
    snprintf(stext, sizeof(stext), "%.1fx", autoSpeed);
    drawTextV(stext, bx, y, CTRL_H, FONT_BODY, CLR_TEXT);
    bx += 60;

    /* Status pill (right side) */
    const char* status = "IDLE";
    Color statBg = CLR_BTN_BG, statFg = CLR_TEXT_DIM;
    if (simState.hasError && !simState.running) {
        status = "ERROR";
        statBg = CLR_BLOCKED; statFg = CLR_TEXT;
    } else if (simState.waitingForInput) {
        status = "WAITING FOR INPUT";
        statBg = CLR_RUNNING; statFg = CLR_BG;
    } else if (autoRun && simState.running) {
        status = "RUNNING";
        statBg = CLR_READY; statFg = CLR_BG;
    } else if (simState.running) {
        status = "PAUSED";
        statBg = CLR_ACCENT; statFg = CLR_BG;
    } else if (processCount > 0) {
        status = "COMPLETE";
        statBg = CLR_FINISHED; statFg = CLR_TEXT;
    }
    int sw = measureText(status, FONT_SMALL) + 24;
    drawPill(w - PAD - sw, by + 6, 24, status, statBg, statFg, 12);
}

/* ── MEMORY PANEL ────────────────────────────────────────────── */

static void drawMemoryPanel(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "MEMORY  (40 words)");

    int cx = x + 10;
    int cy = y + PANEL_HDR_H + 6;
    int innerH = h - PANEL_HDR_H - 10;
    int rowH = 18;
    int visible = innerH / rowH;
    int maxScroll = MEM_SIZE - visible;
    if (maxScroll < 0) maxScroll = 0;

    Rectangle panelArea = {(float)x, (float)y, (float)w, (float)h};
    if (CheckCollisionPointRec(GetMousePosition(), panelArea)) {
        memScroll -= (int)GetMouseWheelMove() * 2;
        if (memScroll < 0) memScroll = 0;
        if (memScroll > maxScroll) memScroll = maxScroll;
    }

    BeginScissorMode(cx - 4, cy, w - 12, innerH);
    for (int i = memScroll; i < MEM_SIZE && i < memScroll + visible + 1; i++) {
        int ry = cy + (i - memScroll) * rowH;

        /* Which PID owns this address? */
        int ownerPid = -1;
        for (int p = 0; p < processCount; p++) {
            PCB* pc = allProcesses[p];
            if (pc->inMemory && i >= pc->memLower && i <= pc->memUpper) {
                ownerPid = pc->pid;
                break;
            }
        }

        /* Alternating row background */
        if ((i / 2) % 2)
            DrawRectangle(cx - 4, ry, w - 12, rowH, CLR_ROW_ALT);

        /* Process color strip on left */
        if (ownerPid > 0) {
            int pidx = ownerPid - 1;
            Color strip = stateColor(allProcesses[pidx]->state);
            if (pidx >= 0 && pidx < GUI_MAX_PROC && swapFlashT[pidx] > 0) strip = CLR_SWAP_FLASH;
            DrawRectangle(cx - 4, ry, 3, rowH, strip);
        }

        char line[96];
        const char* val = memory[i] ? memory[i] : "";
        snprintf(line, sizeof(line), "%02d", i);
        drawText(line, cx + 4, ry + 2, FONT_MONO, CLR_TEXT_MUTED);

        if (memory[i]) {
            char trimmed[64];
            snprintf(trimmed, sizeof(trimmed), "%.48s", val);
            drawText(trimmed, cx + 36, ry + 2, FONT_MONO, CLR_TEXT);
        } else {
            drawText(".", cx + 36, ry + 2, FONT_MONO, CLR_TEXT_MUTED);
        }
    }
    EndScissorMode();

    /* Scrollbar */
    if (maxScroll > 0) {
        int trackH = innerH;
        int barH = trackH * visible / MEM_SIZE;
        if (barH < 24) barH = 24;
        float ratio = (float)memScroll / maxScroll;
        int barY = cy + (int)(ratio * (trackH - barH));
        DrawRectangleRounded((Rectangle){(float)(x + w - 6), (float)barY, 3, (float)barH}, 0.5f, 3, CLR_ACCENT);
    }
}

/* ── RUNNING + QUEUES PANEL ──────────────────────────────────── */

static void drawExecutionPanel(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "EXECUTION");

    int cx = x + 14;
    int cy = y + PANEL_HDR_H + 10;

    /* CURRENTLY RUNNING */
    PCB* run = simState.waitingForInput && simState.inputProcess
        ? simState.inputProcess
        : simState.currentlyRunning;

    drawText("NOW RUNNING", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 18;

    /* Big running card */
    int cardH = 76;
    DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)cardH}, 0.1f, 6,
                         run ? (Color){50, 55, 72, 255} : CLR_ROW_ALT);
    if (run) {
        /* PID pill */
        char pidLabel[16];
        snprintf(pidLabel, sizeof(pidLabel), "P%d", run->pid);
        DrawRectangleRounded((Rectangle){(float)(cx + 10), (float)(cy + 10), 56, 56}, 0.25f, 6, CLR_RUNNING);
        int tw = measureText(pidLabel, FONT_TITLE);
        drawText(pidLabel, cx + 10 + (56 - tw) / 2, cy + 10 + (56 - FONT_TITLE) / 2, FONT_TITLE, CLR_BG);

        int tx = cx + 80;
        int ty = cy + 8;
        char buf[128];
        const char* instr = simState.currentInstr[0] ? simState.currentInstr : "(idle)";
        const char* memLabel = run->inMemory ? "" : "  [SWAPPED]";

        snprintf(buf, sizeof(buf), "PC: %d    Mem: %d-%d%s",
                 run->programCounter, run->memLower, run->memUpper, memLabel);
        drawText(buf, tx, ty, FONT_BODY, CLR_TEXT); ty += 20;

        snprintf(buf, sizeof(buf), "Instr: %.40s", instr);
        drawText(buf, tx, ty, FONT_MONO, CLR_ACCENT); ty += 20;

        snprintf(buf, sizeof(buf), "Queue: Q%d    Wait: %d    Burst: %d",
                 run->queueLevel, run->waitTime, run->burstTime);
        drawText(buf, tx, ty, FONT_SMALL, CLR_TEXT_DIM);
    } else {
        const char* idleText = simState.hasError ? simState.lastError : "-- CPU idle --";
        int tw = measureText(idleText, FONT_BODY);
        drawText(idleText, cx + (w - 28 - tw) / 2, cy + (cardH - FONT_BODY) / 2, FONT_BODY,
                 simState.hasError ? CLR_BLOCKED : CLR_TEXT_MUTED);
    }
    cy += cardH + 14;

    /* READY queue */
    drawText("READY QUEUE", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 18;
    drawQueueChips(cx, cy, w - 28, 26, &readyQ);
    cy += 32;

    /* BLOCKED queue */
    drawText("BLOCKED QUEUE", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 18;
    drawQueueChips(cx, cy, w - 28, 26, &blockedQ);
    cy += 32;

    /* PROCESS TABLE */
    int tableY = cy;
    drawText("ALL PROCESSES", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 18;

    /* Header row */
    int colPID = cx;
    int colState = cx + 40;
    int colQ    = cx + 110;
    int colWait = cx + 160;
    int colMem = cx + 220;
    drawText("PID",    colPID,   cy, FONT_SMALL, CLR_TEXT_MUTED);
    drawText("STATE",  colState, cy, FONT_SMALL, CLR_TEXT_MUTED);
    drawText("Q-LVL",  colQ,     cy, FONT_SMALL, CLR_TEXT_MUTED);
    drawText("WAIT",   colWait,  cy, FONT_SMALL, CLR_TEXT_MUTED);
    drawText("MEM",    colMem,   cy, FONT_SMALL, CLR_TEXT_MUTED);
    cy += 16;
    DrawRectangle(cx, cy, w - 28, 1, CLR_PANEL_BDR);
    cy += 4;

    int rowH = 22;
    int remaining = (y + h) - cy - 8;
    int visibleProc = remaining / rowH;

    /* Scroll */
    Rectangle tblRect = {(float)cx, (float)cy, (float)(w - 28), (float)remaining};
    if (CheckCollisionPointRec(GetMousePosition(), tblRect)) {
        procScroll -= (int)GetMouseWheelMove();
        if (procScroll < 0) procScroll = 0;
        int maxS = processCount - visibleProc;
        if (maxS < 0) maxS = 0;
        if (procScroll > maxS) procScroll = maxS;
    }

    BeginScissorMode(cx - 4, cy, w - 28 + 4, remaining);
    for (int i = procScroll; i < processCount && i < procScroll + visibleProc + 1; i++) {
        PCB* p = allProcesses[i];
        int ry = cy + (i - procScroll) * rowH;

        if ((i % 2) == 0)
            DrawRectangle(cx - 4, ry, w - 24, rowH, CLR_ROW_ALT);

        /* swap flash */
        int pidx = p->pid - 1;
        if (pidx >= 0 && pidx < GUI_MAX_PROC && swapFlashT[pidx] > 0)
            DrawRectangle(cx - 4, ry, 3, rowH, CLR_SWAP_FLASH);

        char buf[32];
        snprintf(buf, sizeof(buf), "P%d", p->pid);
        drawTextV(buf, colPID, ry, rowH, FONT_BODY, CLR_TEXT);

        /* State pill */
        const char* ss = stateShort(p->state);
        drawPill(colState, ry + (rowH - 18) / 2, 18, ss, stateColor(p->state), CLR_BG, 8);

        snprintf(buf, sizeof(buf), "Q%d", p->queueLevel);
        drawTextV(buf, colQ, ry, rowH, FONT_BODY, CLR_TEXT);
        snprintf(buf, sizeof(buf), "%d", p->waitTime);
        drawTextV(buf, colWait, ry, rowH, FONT_BODY, CLR_TEXT);
        if (p->inMemory) {
            snprintf(buf, sizeof(buf), "%d-%d", p->memLower, p->memUpper);
            drawTextV(buf, colMem, ry, rowH, FONT_BODY, CLR_TEXT_DIM);
        } else {
            drawTextV("SWAPPED", colMem, ry, rowH, FONT_BODY, CLR_SWAP_FLASH);
        }
    }
    EndScissorMode();

    if (processCount == 0)
        drawTextV("No processes yet", cx, tableY + 40, rowH, FONT_BODY, CLR_TEXT_MUTED);
}

/* ── MUTEX + MLFQ PANEL ──────────────────────────────────────── */

static void drawMutexAndMLFQ(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "SYNCHRONIZATION");

    int cx = x + 14;
    int cy = y + PANEL_HDR_H + 10;

    extern Mutex mutexUserInput, mutexUserOutput, mutexFile;
    Mutex* mutexes[] = { &mutexUserInput, &mutexUserOutput, &mutexFile };

    drawText("MUTEXES", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 20;

    for (int i = 0; i < 3; i++) {
        Mutex* m = mutexes[i];
        int cardH = 40;
        int hasWait = !isEmpty(&m->blockedQueue);
        if (hasWait) cardH = 66;

        DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)cardH}, 0.12f, 6, CLR_ROW_ALT);

        /* Indicator dot */
        Color dot = m->locked ? CLR_BLOCKED : CLR_READY;
        DrawCircle(cx + 14, cy + 20, 5, dot);

        drawText(m->name, cx + 28, cy + 6, FONT_BODY, CLR_TEXT);

        char info[64];
        if (m->locked) snprintf(info, sizeof(info), "LOCKED by P%d", m->ownerPid);
        else           snprintf(info, sizeof(info), "Free");
        drawText(info, cx + 28, cy + 22, FONT_SMALL, m->locked ? CLR_BLOCKED : CLR_READY);

        if (hasWait) {
            drawText("waiting:", cx + 14, cy + 42, FONT_SMALL, CLR_TEXT_MUTED);
            drawQueueChips(cx + 80, cy + 40, w - 28 - 80, 22, &m->blockedQueue);
        }
        cy += cardH + 6;
    }

    /* MLFQ section */
    cy += 6;
    drawText("MLFQ SUB-QUEUES", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 20;

    int isMLFQ = (strcmp(simState.schedulerMode, "mlfq") == 0);
    for (int i = 0; i < 4; i++) {
        int cardH = 30;
        DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)cardH}, 0.2f, 6, CLR_ROW_ALT);

        char lbl[16];
        snprintf(lbl, sizeof(lbl), "Q%d", i);
        Color lc = isMLFQ ? CLR_ACCENT : CLR_TEXT_MUTED;
        drawTextV(lbl, cx + 10, cy, cardH, FONT_BODY, lc);

        char q[16];
        snprintf(q, sizeof(q), "quantum:%d", 1 << i);
        drawTextV(q, cx + 44, cy, cardH, FONT_SMALL, CLR_TEXT_MUTED);

        if (isMLFQ)
            drawQueueChips(cx + 140, cy, w - 28 - 140, cardH, &mlfqQueues[i]);
        else
            drawTextV("(inactive)", cx + 140, cy, cardH, FONT_SMALL, CLR_TEXT_MUTED);

        cy += cardH + 4;
    }
}

/* ── LOG PANEL ───────────────────────────────────────────────── */

static void drawLogPanel(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "EVENT LOG");

    int cx = x + 10;
    int cy = y + PANEL_HDR_H + 6;
    int innerH = h - PANEL_HDR_H - 10;
    int rowH = 16;
    int visible = innerH / rowH;
    int total = simState.logCount;
    int maxScroll = total - visible;
    if (maxScroll < 0) maxScroll = 0;

    Rectangle panelArea = {(float)x, (float)y, (float)w, (float)h};
    static int userScrolled = 0;
    if (CheckCollisionPointRec(GetMousePosition(), panelArea)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            logScroll -= (int)wheel * 2;
            userScrolled = 1;
        }
        if (logScroll < 0) logScroll = 0;
        if (logScroll > maxScroll) logScroll = maxScroll;
    }

    /* Auto-follow tail unless user has scrolled up */
    if (!userScrolled || logScroll >= maxScroll - 3)
        logScroll = maxScroll;

    if (total == 0) {
        drawText("No events yet", cx, cy, FONT_MONO, CLR_TEXT_MUTED);
        return;
    }

    BeginScissorMode(cx, cy, w - 20, innerH);
    for (int i = 0; i < visible + 1 && (logScroll + i) < total; i++) {
        int logIdx = logScroll + i;
        int idx;
        if (simState.logCount < MAX_LOG_LINES) idx = logIdx;
        else idx = (simState.logHead + logIdx) % MAX_LOG_LINES;
        if (idx < 0 || idx >= MAX_LOG_LINES) continue;

        int ry = cy + i * rowH;
        const char* line = simState.log[idx];
        Color tc = CLR_TEXT_DIM;
        if (strstr(line, "ERROR"))              tc = CLR_BLOCKED;
        else if (strstr(line, "WARNING"))       tc = CLR_RUNNING;
        else if (strstr(line, "[SWAP]"))        tc = CLR_SWAP_FLASH;
        else if (strstr(line, "[CREATE]"))      tc = CLR_READY;
        else if (strstr(line, "[SCHED]"))       tc = CLR_ACCENT_HI;
        else if (strstr(line, "[EXEC]"))        tc = CLR_ACCENT;
        else if (strstr(line, "[INPUT]"))       tc = CLR_RUNNING;
        else if (strstr(line, "[MUTEX]"))       tc = CLR_ACCENT;
        else if (strstr(line, "[SYS]"))         tc = CLR_TEXT;
        else if (strstr(line, "OUTPUT:"))       tc = CLR_READY;
        else if (strstr(line, "FINISHED"))      tc = CLR_FINISHED;
        else if (strstr(line, "Clock Cycle"))   tc = CLR_ACCENT_HI;

        drawText(line, cx, ry, FONT_MONO, tc);
    }
    EndScissorMode();

    /* Scrollbar */
    if (maxScroll > 0) {
        int barH = innerH * visible / total;
        if (barH < 24) barH = 24;
        float ratio = (float)logScroll / maxScroll;
        int barY = cy + (int)(ratio * (innerH - barH));
        DrawRectangleRounded((Rectangle){(float)(x + w - 8), (float)barY, 3, (float)barH}, 0.5f, 3, CLR_ACCENT);
    }
}

/* ── INPUT MODAL ─────────────────────────────────────────────── */

static void drawInputModal(int screenW, int screenH) {
    if (!simState.waitingForInput) { inputActive = 0; return; }

    DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 170});

    int dw = 520, dh = 200;
    int dx = (screenW - dw) / 2;
    int dy = (screenH - dh) / 2;

    /* shadow */
    DrawRectangleRounded((Rectangle){(float)(dx + 4), (float)(dy + 6), (float)dw, (float)dh}, 0.04f, 8,
                         (Color){0, 0, 0, 120});
    DrawRectangleRounded((Rectangle){(float)dx, (float)dy, (float)dw, (float)dh}, 0.04f, 8, CLR_PANEL);
    DrawRectangleRoundedLines((Rectangle){(float)dx, (float)dy, (float)dw, (float)dh}, 0.04f, 8, CLR_ACCENT);

    /* header strip */
    DrawRectangleRounded((Rectangle){(float)dx, (float)dy, (float)dw, 36}, 0.04f, 8, CLR_PANEL_HDR);
    drawTextV("INPUT REQUIRED", dx + 16, dy, 36, FONT_H1, CLR_ACCENT);

    /* prompt */
    drawText(simState.inputPrompt, dx + 24, dy + 50, FONT_BODY, CLR_TEXT);

    /* text input */
    Rectangle inputRect = {(float)(dx + 24), (float)(dy + 84), (float)(dw - 48), 36};
    DrawRectangleRounded(inputRect, 0.15f, 4, CLR_BG);
    DrawRectangleRoundedLines(inputRect, 0.15f, 4, CLR_ACCENT);

    inputActive = 1;
    /* Manual text input handling with raygui for caret */
    GuiTextBox(inputRect, inputBuf, sizeof(inputBuf), inputActive);

    /* Submit button */
    int submit = 0;
    if (drawButton(dx + dw - 140 - 16, dy + 140, 140, 38, "SUBMIT (Enter)", CLR_ACCENT, CLR_ACCENT_HI, CLR_BG, 1))
        submit = 1;
    if (IsKeyPressed(KEY_ENTER)) submit = 1;

    if (submit && strlen(inputBuf) > 0) {
        simProvideInput(inputBuf);
        inputBuf[0] = '\0';
        inputActive = 0;
    }
}

/* ── Track swap events for flash highlighting ────────────────── */

static int prevLogSerial = 0;
static void updateSwapFlash(float dt) {
    for (int i = 0; i < GUI_MAX_PROC; i++)
        if (swapFlashT[i] > 0) swapFlashT[i] -= dt;

    if (simState.logSerial < prevLogSerial)
        prevLogSerial = 0;

    /* Scan new log entries for [SWAP] */
    {
        int oldestSerial = simState.logSerial - simState.logCount;
        if (oldestSerial < 0) oldestSerial = 0;

        for (int serial = prevLogSerial; serial < simState.logSerial; serial++) {
            int idx;
            const char* l;

            if (serial < oldestSerial)
                continue;

            idx = (simState.logHead + (serial - oldestSerial)) % MAX_LOG_LINES;
            if (idx < 0 || idx >= MAX_LOG_LINES) continue;
            l = simState.log[idx];
            if (!strstr(l, "[SWAP]"))
                continue;

            {
                const char* pp = strstr(l, "Process ");
                if (pp) {
                    int pid = 0;
                    sscanf(pp, "Process %d", &pid);
                    if (pid > 0 && pid <= GUI_MAX_PROC) swapFlashT[pid - 1] = 1.5f;
                }
            }
        }
    }
    prevLogSerial = simState.logSerial;
}

/* ── MAIN GUI LOOP ───────────────────────────────────────────── */

void guiRun(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "OS Simulation - CSEN 602");
    SetWindowMinSize(1200, 760);
    SetTargetFPS(60);

    /* Force raygui's default style to load NOW, before we load our font.
       Otherwise the first GuiSetFont call triggers GuiLoadStyleDefault lazily,
       which calls UnloadTexture on the previous guiFont.texture slot — and
       that slot happens to be the texture ID we just got. Result: our atlas
       is destroyed right after loading. */
    GuiLoadStyleDefault();

    /* Build explicit ASCII 32..126 codepoint array. */
    int cps[95];
    for (int i = 0; i < 95; i++) cps[i] = 32 + i;

    guiFont = LoadFontEx("C:\\Windows\\Fonts\\segoeui.ttf", 32, cps, 95);
    if (guiFont.texture.id == 0 || guiFont.glyphCount < 10)
        guiFont = LoadFontEx("C:\\Windows\\Fonts\\consola.ttf", 32, cps, 95);
    if (guiFont.texture.id == 0 || guiFont.glyphCount < 10)
        guiFont = LoadFontEx("C:\\Windows\\Fonts\\arial.ttf", 32, cps, 95);
    if (guiFont.texture.id == 0 || guiFont.glyphCount < 10)
        guiFont = GetFontDefault();

    if (guiFont.texture.id != 0 && guiFont.glyphCount >= 10) {
        fontLoaded = 1;
        SetTextureFilter(guiFont.texture, TEXTURE_FILTER_BILINEAR);
        GuiSetFont(guiFont);
        GuiSetStyle(DEFAULT, TEXT_SIZE, FONT_BODY);
    }

    TraceLog(LOG_INFO, "FONT: loaded=%d glyphs=%d baseSize=%d texId=%u",
             fontLoaded, guiFont.glyphCount, guiFont.baseSize, guiFont.texture.id);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        /* Auto-run */
        if (autoRun && simState.running && !simState.waitingForInput) {
            autoTimer += dt;
            float interval = 1.0f / autoSpeed;
            while (autoTimer >= interval) {
                autoTimer -= interval;
                if (!simStep()) { autoRun = 0; break; }
            }
        } else {
            autoTimer = 0;
        }

        updateSwapFlash(dt);

        BeginDrawing();
        ClearBackground(CLR_BG);

        /* Top bar + control strip */
        drawTopBar(w);
        drawControlStrip(HDR_H, w);

        /* Main region (3 columns) */
        int mainY = HDR_H + CTRL_H + PAD;
        int mainH = h - mainY - LOG_H - PAD * 2;

        int colLeftW   = 300;
        int colRightW  = 380;
        int colMidW    = w - colLeftW - colRightW - PAD * 4;

        int lx = PAD;
        int mx = lx + colLeftW + PAD;
        int rx = mx + colMidW + PAD;

        drawMemoryPanel(lx, mainY, colLeftW, mainH);
        drawExecutionPanel(mx, mainY, colMidW, mainH);
        drawMutexAndMLFQ(rx, mainY, colRightW, mainH);

        /* Log panel */
        int logY = mainY + mainH + PAD;
        drawLogPanel(PAD, logY, w - PAD * 2, LOG_H);

        /* Modal */
        drawInputModal(w, h);

        EndDrawing();
    }

    if (fontLoaded) UnloadFont(guiFont);
    CloseWindow();
}
