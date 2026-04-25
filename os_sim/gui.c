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



static Font guiFont;
static int  fontLoaded = 0;

static int   autoRun       = 0;
static float autoSpeed     = 4.0f;
static float autoTimer     = 0.0f;
static int   memScroll     = 0;
static int   logScroll     = 0;
static int   procScroll    = 0;
static char  inputBuf[256] = "";
static int   inputActive   = 0;
static char  lastOutputLine[LOG_LINE_LEN] = "";
static char  outputLines[32][LOG_LINE_LEN];
static int   outputLineCount = 0;
static int   outputScroll = 0;
static int   outputFollowTail = 1;
#define GUI_MAX_PROC 32
#define PARTICLE_COUNT 150
static float swapFlashT[GUI_MAX_PROC] = {0};
static float schedHoverT[3] = {0};
static int   logFollowTail = 1;
static int   logPanelSerial = 0;

typedef struct Particle {
    Vector2 pos;
    Vector2 vel;
    float radius;
    float alpha;
} Particle;

static Particle particles[PARTICLE_COUNT];
static int particlesReady = 0;



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

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static Color mixColor(Color a, Color b, float t) {
    t = clamp01(t);
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

static Color alphaColor(Color c, unsigned char a) {
    c.a = a;
    return c;
}

static float approach(float current, float target, float speed, float dt) {
    float delta = target - current;
    float step = speed * dt;
    if (delta > step) return current + step;
    if (delta < -step) return current - step;
    return target;
}


static void drawTextV(const char* text, int x, int y, int rowH, int size, Color c) {
    drawText(text, x, y + (rowH - size) / 2, size, c);
}

static void clearGuiOutput(void) {
    lastOutputLine[0] = '\0';
    outputLineCount = 0;
    outputScroll = 0;
    outputFollowTail = 1;
}



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

static void initParticles(int w, int h) {
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        particles[i].pos = (Vector2){(float)GetRandomValue(0, w), (float)GetRandomValue(0, h)};
        particles[i].vel = (Vector2){
            (float)GetRandomValue(-26, 26) / 10.0f,
            (float)GetRandomValue(10, 34) / 10.0f
        };
        particles[i].radius = (float)GetRandomValue(14, 38) / 10.0f;
        particles[i].alpha = (float)GetRandomValue(54, 138);
    }
    particlesReady = 1;
}

static void drawParticleBackground(int w, int h, float dt) {
    if (!particlesReady)
        initParticles(w, h);

    DrawRectangle(0, 0, w, h, CLR_BG);

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        Particle* p = &particles[i];
        p->pos.x += p->vel.x * dt * 18.0f;
        p->pos.y += p->vel.y * dt * 18.0f;

        if (p->pos.x < -20) p->pos.x = (float)w + 20;
        if (p->pos.x > w + 20) p->pos.x = -20;
        if (p->pos.y < -20) p->pos.y = (float)h + 20;
        if (p->pos.y > h + 20) p->pos.y = -20;

        DrawCircleV(p->pos, p->radius, (Color){63, 216, 138, (unsigned char)p->alpha});
        DrawCircleV(p->pos, p->radius * 2.8f, (Color){63, 216, 138, (unsigned char)(p->alpha * 0.12f)});
    }

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        for (int j = i + 1; j < PARTICLE_COUNT; j++) {
            float dx = particles[i].pos.x - particles[j].pos.x;
            float dy = particles[i].pos.y - particles[j].pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < 17000.0f) {
                unsigned char a = (unsigned char)(44.0f * (1.0f - d2 / 17000.0f));
                DrawLineEx(particles[i].pos, particles[j].pos, 1.0f, (Color){63, 216, 138, a});
            }
        }
    }

    DrawRectangleGradientV(0, 0, w, h, (Color){12, 15, 18, 35}, (Color){8, 10, 12, 150});
}

static void drawParticleOverlay(void) {
    for (int i = 0; i < PARTICLE_COUNT; i += 3) {
        DrawCircleV(particles[i].pos, particles[i].radius * 0.8f, (Color){128, 240, 180, 24});
    }
}

static void drawPanel(int x, int y, int w, int h, const char* title) {

    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    DrawRectangleRounded((Rectangle){(float)(x + 2), (float)(y + 3), (float)w, (float)h}, 0.035f, 8, (Color){0, 0, 0, 70});
    DrawRectangleRounded(r, 0.035f, 8, CLR_PANEL);

    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)PANEL_HDR_H}, 0.035f, 8, CLR_PANEL_HDR);

    DrawRectangle(x, y + PANEL_HDR_H, w, 1, CLR_PANEL_BDR);

    drawTextV(title, x + 12, y, PANEL_HDR_H, FONT_H1, CLR_ACCENT);

    DrawRectangleRoundedLines(r, 0.035f, 8, CLR_PANEL_BDR);
}


static int drawPill(int x, int y, int h, const char* text, Color bg, Color fg, int padX) {
    int tw = measureText(text, FONT_SMALL);
    int w  = tw + padX * 2;
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.45f, 6, bg);
    drawTextV(text, x + padX, y, h, FONT_SMALL, fg);
    return w;
}


static int drawButton(int x, int y, int w, int h, const char* label, Color bg, Color hover, Color fg, int enabled) {
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    Vector2 m = GetMousePosition();
    int isHover = enabled && CheckCollisionPointRec(m, r);
    Color bc = isHover ? hover : bg;
    if (!enabled) { bc = (Color){45, 48, 62, 255}; fg = CLR_TEXT_MUTED; }

    if (isHover)
        DrawRectangleRounded((Rectangle){r.x - 1, r.y - 1, r.width + 2, r.height + 2}, 0.24f, 8, alphaColor(hover, 70));
    DrawRectangleRounded(r, 0.22f, 8, bc);
    int tw = measureText(label, FONT_BODY);
    drawTextV(label, x + (w - tw) / 2, y, h, FONT_BODY, fg);

    return enabled && isHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}


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



static int drawSchedulerOption(int i, int x, int y, int w, int h, const char* label, Color accent, int selected, int enabled, float dt) {
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    Vector2 m = GetMousePosition();
    int hover = enabled && CheckCollisionPointRec(m, r);
    float target = (hover || selected) ? 1.0f : 0.0f;
    schedHoverT[i] = approach(schedHoverT[i], target, 7.5f, dt);

    float t = schedHoverT[i];
    float lift = selected ? 2.0f : t * 4.0f;
    Color base = selected ? mixColor(accent, CLR_PANEL_HDR, 0.10f) : CLR_BTN_BG;
    Color bg = mixColor(base, accent, selected ? 0.14f : 0.10f * t);
    Color fg = enabled ? mixColor(CLR_TEXT_DIM, CLR_TEXT, selected ? 1.0f : t) : CLR_TEXT_MUTED;

    Rectangle glow = {r.x - 2, r.y - lift - 2, r.width + 4, r.height + 4};
    DrawRectangleRounded(glow, 0.18f, 8, alphaColor(accent, (unsigned char)(30 + 55 * t)));
    DrawRectangleRounded((Rectangle){r.x, r.y - lift, r.width, r.height}, 0.16f, 8, bg);
    DrawRectangle((int)r.x + 10, (int)(r.y + r.height - 4 - lift), (int)(r.width - 20), 3, alphaColor(accent, selected ? 255 : (unsigned char)(80 + 120 * t)));

    int tw = measureText(label, FONT_BODY);
    drawTextV(label, x + (w - tw) / 2, (int)(y - lift), h, FONT_BODY, fg);

    return enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void drawTopBar(int w, float dt) {
    DrawRectangle(0, 0, w, HDR_H, (Color){18, 23, 25, 235});
    DrawRectangle(0, HDR_H, w, 1, CLR_PANEL_BDR);


    drawTextV("OS Simulation", PAD + 4, 0, HDR_H, FONT_TITLE, CLR_TEXT);
    int tw = measureText("OS Simulation", FONT_TITLE);
    drawTextV("CSEN 602", PAD + 4 + tw + 12, 0, HDR_H, FONT_SMALL, CLR_TEXT_DIM);


    const char* modes[]  = {"rr", "hrrn", "mlfq"};
    const char* labels[] = {"Round Robin", "HRRN", "MLFQ"};
    Color optionColors[] = {
        (Color){63, 216, 138, 255},
        (Color){45, 212, 191, 255},
        (Color){163, 230, 53, 255}
    };
    int tgW = 426;
    int tgX = (w - tgW) / 2;
    int tgY = (HDR_H - 38) / 2;
    drawTextV("Scheduler", tgX - 92, 0, HDR_H, FONT_SMALL, CLR_TEXT_DIM);

    for (int i = 0; i < 3; i++) {
        int bw = 134;
        int bx = tgX + i * (bw + 8);
        int selected = (strcmp(simState.schedulerMode, modes[i]) == 0);
        int canSwitch = (!simState.running || simState.clock == 0);
        if (drawSchedulerOption(i, bx, tgY, bw, 38, labels[i], optionColors[i], selected, canSwitch, dt)) {
            clearGuiOutput();
            simReset(modes[i]);
        }
    }


    char clk[32];
    snprintf(clk, sizeof(clk), "CLOCK  %04d", simState.clock);
    int cw = measureText(clk, FONT_TITLE);
    drawTextV(clk, w - cw - PAD - 4, 0, HDR_H, FONT_TITLE, CLR_ACCENT);
}



static void drawControlStrip(int y, int w) {
    DrawRectangle(0, y, w, CTRL_H, CLR_PANEL);
    DrawRectangle(0, y, w, 1, CLR_PANEL_BDR);
    DrawRectangle(0, y + CTRL_H - 1, w, 1, CLR_PANEL_BDR);

    int bx = PAD;
    int by = y + (CTRL_H - 40) / 2;
    int bh = 40;

    int canStep = simState.running && !simState.waitingForInput;


    if (drawButton(bx, by, 104, bh, "STEP", CLR_ACCENT, CLR_ACCENT_HI, CLR_BG, canStep)) {
        autoRun = 0;
        simStep();
    }
    bx += 104 + GAP;


    if (autoRun) {
        if (drawButton(bx, by, 104, bh, "PAUSE", CLR_BTN_BG, CLR_BTN_HOVER, CLR_TEXT, 1))
            autoRun = 0;
    } else {
        if (drawButton(bx, by, 104, bh, "RUN", CLR_READY, CLR_ACCENT_HI, CLR_BG, canStep))
            autoRun = 1;
    }
    bx += 104 + GAP;


    if (drawButton(bx, by, 104, bh, "RESET", CLR_BTN_DANGER, (Color){178, 68, 72, 255}, CLR_TEXT, 1)) {
        autoRun = 0;
        clearGuiOutput();
        simReset(simState.schedulerMode);
    }
    bx += 104 + PAD * 2;


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



static void drawMemoryPanel(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "MEMORY  (40 words)");

    int cx = x + 10;
    int cy = y + PANEL_HDR_H + 6;
    int innerH = h - PANEL_HDR_H - 10;
    int rowH = 22;
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


        int ownerPid = -1;
        for (int p = 0; p < processCount; p++) {
            PCB* pc = allProcesses[p];
            if (pc->inMemory && i >= pc->memLower && i <= pc->memUpper) {
                ownerPid = pc->pid;
                break;
            }
        }


        if ((i / 2) % 2)
            DrawRectangle(cx - 4, ry, w - 12, rowH, CLR_ROW_ALT);


        if (ownerPid > 0) {
            int pidx = ownerPid - 1;
            Color strip = stateColor(allProcesses[pidx]->state);
            if (pidx >= 0 && pidx < GUI_MAX_PROC && swapFlashT[pidx] > 0) strip = CLR_SWAP_FLASH;
            DrawRectangle(cx - 4, ry, 4, rowH, strip);
        }

        char line[96];
        const char* val = memory[i] ? memory[i] : "";
        snprintf(line, sizeof(line), "%02d", i);
        drawText(line, cx + 6, ry + 3, FONT_MONO, CLR_TEXT_MUTED);

        if (memory[i]) {
            char trimmed[64];
            snprintf(trimmed, sizeof(trimmed), "%.48s", val);
            drawText(trimmed, cx + 42, ry + 3, FONT_MONO, CLR_TEXT);
        } else {
            drawText(".", cx + 42, ry + 3, FONT_MONO, CLR_TEXT_MUTED);
        }
    }
    EndScissorMode();


    if (maxScroll > 0) {
        int trackH = innerH;
        int barH = trackH * visible / MEM_SIZE;
        if (barH < 24) barH = 24;
        float ratio = (float)memScroll / maxScroll;
        int barY = cy + (int)(ratio * (trackH - barH));
        DrawRectangleRounded((Rectangle){(float)(x + w - 6), (float)barY, 3, (float)barH}, 0.5f, 3, CLR_ACCENT);
    }
}



static void drawExecutionPanel(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "EXECUTION");

    int cx = x + 14;
    int cy = y + PANEL_HDR_H + 10;


    PCB* run = simState.waitingForInput && simState.inputProcess
        ? simState.inputProcess
        : simState.currentlyRunning;

    drawText("NOW RUNNING", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 18;


    int cardH = 90;
    DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)cardH}, 0.1f, 6,
                         run ? (Color){50, 55, 72, 255} : CLR_ROW_ALT);
    if (run) {

        char pidLabel[16];
        snprintf(pidLabel, sizeof(pidLabel), "P%d", run->pid);
        DrawRectangleRounded((Rectangle){(float)(cx + 12), (float)(cy + 14), 60, 60}, 0.22f, 8, CLR_RUNNING);
        int tw = measureText(pidLabel, FONT_TITLE);
        drawText(pidLabel, cx + 12 + (60 - tw) / 2, cy + 14 + (60 - FONT_TITLE) / 2, FONT_TITLE, CLR_BG);

        int tx = cx + 88;
        int ty = cy + 10;
        char buf[128];
        const char* instr = simState.currentInstr[0] ? simState.currentInstr : "(idle)";
        const char* memLabel = run->inMemory ? "" : "  [SWAPPED]";

        snprintf(buf, sizeof(buf), "PC: %d    Mem: %d-%d%s",
                 run->programCounter, run->memLower, run->memUpper, memLabel);
        drawText(buf, tx, ty, FONT_BODY, CLR_TEXT); ty += 24;

        snprintf(buf, sizeof(buf), "Instr: %.40s", instr);
        drawText(buf, tx, ty, FONT_MONO, CLR_ACCENT); ty += 24;

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

    if (lastOutputLine[0]) {
        int boxH = 178;
        int rowH = 16;
        int visibleOut = (boxH - 42) / rowH;
        int maxOutScroll = outputLineCount - visibleOut;
        if (maxOutScroll < 0) maxOutScroll = 0;
        Rectangle outRect = {(float)cx, (float)cy, (float)(w - 28), (float)boxH};
        if (CheckCollisionPointRec(GetMousePosition(), outRect)) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                outputScroll -= (int)wheel;
                outputFollowTail = wheel < 0;
            }
        }
        if (outputFollowTail)
            outputScroll = maxOutScroll;
        if (outputScroll < 0) outputScroll = 0;
        if (outputScroll > maxOutScroll) outputScroll = maxOutScroll;

        DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)boxH}, 0.12f, 8, (Color){16, 42, 31, 230});
        DrawRectangle(cx + 10, cy + 12, 4, boxH - 24, CLR_READY);
        drawText("PROGRAM OUTPUT", cx + 24, cy + 9, FONT_SMALL, CLR_TEXT_DIM);

        BeginScissorMode(cx + 24, cy + 30, w - 70, boxH - 38);
        for (int i = 0; i < visibleOut && outputScroll + i < outputLineCount; i++) {
            int idx = outputScroll + i;
            drawText(outputLines[idx], cx + 24, cy + 31 + i * rowH, FONT_MONO, CLR_READY);
        }
        EndScissorMode();

        if (maxOutScroll > 0) {
            int trackX = x + w - 44;
            int trackY = cy + 30;
            int trackH = boxH - 42;
            int thumbH = trackH * visibleOut / outputLineCount;
            if (thumbH < 28) thumbH = 28;
            int thumbY = trackY + (int)((float)outputScroll / maxOutScroll * (trackH - thumbH));
            DrawRectangleRounded((Rectangle){(float)trackX, (float)trackY, 5, (float)trackH}, 0.7f, 4, (Color){44, 58, 54, 180});
            DrawRectangleRounded((Rectangle){(float)(trackX - 1), (float)thumbY, 7, (float)thumbH}, 0.7f, 4, CLR_ACCENT);
        }
        cy += boxH + 14;
    }


    drawText("READY QUEUE", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 20;
    drawQueueChips(cx, cy, w - 28, 30, &readyQ);
    cy += 36;


    drawText("BLOCKED QUEUE", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 20;
    drawQueueChips(cx, cy, w - 28, 30, &blockedQ);
    cy += 36;


    int tableY = cy;
    drawText("ALL PROCESSES", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 20;


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
    cy += 19;
    DrawRectangle(cx, cy, w - 28, 1, CLR_PANEL_BDR);
    cy += 4;

    int rowH = 28;
    int remaining = (y + h) - cy - 8;
    int visibleProc = remaining / rowH;


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


        int pidx = p->pid - 1;
        if (pidx >= 0 && pidx < GUI_MAX_PROC && swapFlashT[pidx] > 0)
            DrawRectangle(cx - 4, ry, 3, rowH, CLR_SWAP_FLASH);

        char buf[32];
        snprintf(buf, sizeof(buf), "P%d", p->pid);
        drawTextV(buf, colPID, ry, rowH, FONT_BODY, CLR_TEXT);


        const char* ss = stateShort(p->state);
        drawPill(colState, ry + (rowH - 22) / 2, 22, ss, stateColor(p->state), CLR_BG, 8);

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
        int cardH = 48;
        int hasWait = !isEmpty(&m->blockedQueue);
        if (hasWait) cardH = 76;

        DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)cardH}, 0.12f, 6, CLR_ROW_ALT);


        Color dot = m->locked ? CLR_BLOCKED : CLR_READY;
        DrawCircle(cx + 16, cy + 24, 6, dot);

        drawText(m->name, cx + 34, cy + 7, FONT_BODY, CLR_TEXT);

        char info[64];
        if (m->locked) snprintf(info, sizeof(info), "LOCKED by P%d", m->ownerPid);
        else           snprintf(info, sizeof(info), "Free");
        drawText(info, cx + 34, cy + 28, FONT_SMALL, m->locked ? CLR_BLOCKED : CLR_READY);

        if (hasWait) {
            drawText("waiting:", cx + 16, cy + 52, FONT_SMALL, CLR_TEXT_MUTED);
            drawQueueChips(cx + 86, cy + 49, w - 28 - 86, 26, &m->blockedQueue);
        }
        cy += cardH + 8;
    }


    cy += 6;
    drawText("MLFQ SUB-QUEUES", cx, cy, FONT_SMALL, CLR_TEXT_DIM);
    cy += 20;

    int isMLFQ = (strcmp(simState.schedulerMode, "mlfq") == 0);
    for (int i = 0; i < 4; i++) {
        int cardH = 36;
        DrawRectangleRounded((Rectangle){(float)cx, (float)cy, (float)(w - 28), (float)cardH}, 0.2f, 6, CLR_ROW_ALT);

        char lbl[16];
        snprintf(lbl, sizeof(lbl), "Q%d", i);
        Color lc = isMLFQ ? CLR_ACCENT : CLR_TEXT_MUTED;
        drawTextV(lbl, cx + 10, cy, cardH, FONT_BODY, lc);

        char q[16];
        snprintf(q, sizeof(q), "quantum:%d", 1 << i);
        drawTextV(q, cx + 44, cy, cardH, FONT_SMALL, CLR_TEXT_MUTED);

        if (isMLFQ)
            drawQueueChips(cx + 150, cy, w - 28 - 150, cardH, &mlfqQueues[i]);
        else
            drawTextV("(inactive)", cx + 150, cy, cardH, FONT_SMALL, CLR_TEXT_MUTED);

        cy += cardH + 4;
    }
}



static void drawLogPanel(int x, int y, int w, int h) {
    drawPanel(x, y, w, h, "EVENT LOG");

    int cx = x + 12;
    int cy = y + PANEL_HDR_H + 10;
    int innerH = h - PANEL_HDR_H - 16;
    int rowH = 22;
    int visible = innerH / rowH;
    int total = simState.logCount;
    int maxScroll = total - visible;
    if (maxScroll < 0) maxScroll = 0;

    if (simState.logSerial < logPanelSerial)
        logPanelSerial = 0;
    if (simState.logSerial != logPanelSerial) {
        int oldestSerial = simState.logSerial - simState.logCount;
        if (oldestSerial < 0) oldestSerial = 0;
        for (int serial = logPanelSerial; serial < simState.logSerial; serial++) {
            int idx;
            const char* line;
            if (serial < oldestSerial)
                continue;
            idx = (simState.logHead + (serial - oldestSerial)) % MAX_LOG_LINES;
            if (idx < 0 || idx >= MAX_LOG_LINES)
                continue;
            line = simState.log[idx];
            if (strstr(line, "OUTPUT:")) {
                strncpy(lastOutputLine, line, sizeof(lastOutputLine) - 1);
                lastOutputLine[sizeof(lastOutputLine) - 1] = '\0';
                if (outputLineCount < 32) {
                    strncpy(outputLines[outputLineCount], line, LOG_LINE_LEN - 1);
                    outputLines[outputLineCount][LOG_LINE_LEN - 1] = '\0';
                    outputLineCount++;
                } else {
                    for (int i = 1; i < 32; i++)
                        strncpy(outputLines[i - 1], outputLines[i], LOG_LINE_LEN);
                    strncpy(outputLines[31], line, LOG_LINE_LEN - 1);
                    outputLines[31][LOG_LINE_LEN - 1] = '\0';
                }
                outputFollowTail = 1;
                logFollowTail = 1;
            }
        }
        logPanelSerial = simState.logSerial;
    }

    Rectangle panelArea = {(float)x, (float)y, (float)w, (float)h};
    Rectangle scrollTrack = {(float)(x + w - 14), (float)cy, 6, (float)innerH};
    static int draggingLog = 0;
    if (CheckCollisionPointRec(GetMousePosition(), panelArea)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            logScroll -= (int)wheel * 3;
            logFollowTail = wheel < 0;
        }
        if (logScroll < 0) logScroll = 0;
        if (logScroll > maxScroll) logScroll = maxScroll;
        if (logScroll >= maxScroll - 1)
            logFollowTail = 1;
    }

    if (maxScroll > 0) {
        int barH = innerH * visible / total;
        if (barH < 32) barH = 32;
        float ratio = (float)logScroll / maxScroll;
        int barY = cy + (int)(ratio * (innerH - barH));
        Rectangle thumb = {(float)(x + w - 15), (float)barY, 8, (float)barH};
        Vector2 m = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, (Rectangle){scrollTrack.x - 8, scrollTrack.y, 22, scrollTrack.height})) {
            draggingLog = 1;
            logFollowTail = 0;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            draggingLog = 0;
        if (draggingLog) {
            float nt = (m.y - cy - barH / 2.0f) / (float)(innerH - barH);
            nt = clamp01(nt);
            logScroll = (int)(nt * maxScroll + 0.5f);
            if (logScroll >= maxScroll - 1)
                logFollowTail = 1;
        }
        DrawRectangleRounded(scrollTrack, 0.7f, 4, (Color){44, 58, 54, 180});
        DrawRectangleRounded(thumb, 0.7f, 4, alphaColor(CLR_ACCENT, 230));
    }


    if (logFollowTail || logScroll >= maxScroll - 1)
        logScroll = maxScroll;

    if (total == 0) {
        drawText("No events yet", cx, cy, FONT_MONO, CLR_TEXT_MUTED);
        return;
    }

    BeginScissorMode(cx, cy, w - 32, innerH);
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

        if ((i % 2) == 0)
            DrawRectangle(cx - 6, ry - 1, w - 44, rowH, (Color){18, 23, 25, 120});
        drawText(line, cx, ry + 2, FONT_MONO, tc);
    }
    EndScissorMode();
}



static void drawInputModal(int screenW, int screenH) {
    if (!simState.waitingForInput) { inputActive = 0; return; }

    DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 170});

    int dw = 560, dh = 220;
    int dx = (screenW - dw) / 2;
    int dy = (screenH - dh) / 2;


    DrawRectangleRounded((Rectangle){(float)(dx + 4), (float)(dy + 6), (float)dw, (float)dh}, 0.04f, 8,
                         (Color){0, 0, 0, 120});
    DrawRectangleRounded((Rectangle){(float)dx, (float)dy, (float)dw, (float)dh}, 0.04f, 8, CLR_PANEL);
    DrawRectangleRoundedLines((Rectangle){(float)dx, (float)dy, (float)dw, (float)dh}, 0.04f, 8, CLR_ACCENT);


    DrawRectangleRounded((Rectangle){(float)dx, (float)dy, (float)dw, 42}, 0.04f, 8, CLR_PANEL_HDR);
    drawTextV("INPUT REQUIRED", dx + 18, dy, 42, FONT_H1, CLR_ACCENT);


    drawText(simState.inputPrompt, dx + 26, dy + 58, FONT_BODY, CLR_TEXT);


    Rectangle inputRect = {(float)(dx + 26), (float)(dy + 96), (float)(dw - 52), 42};
    DrawRectangleRounded(inputRect, 0.15f, 4, CLR_BG);
    DrawRectangleRoundedLines(inputRect, 0.15f, 4, CLR_ACCENT);

    inputActive = 1;

    int ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (ctrlDown && IsKeyPressed(KEY_V)) {
        const char* clip = GetClipboardText();
        if (clip) {
            int len = (int)strlen(inputBuf);
            for (int i = 0; clip[i] != '\0' && len < (int)sizeof(inputBuf) - 1; i++) {
                if (clip[i] >= 32 && clip[i] <= 126) {
                    inputBuf[len++] = clip[i];
                    inputBuf[len] = '\0';
                }
            }
        }
    }

    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(inputBuf);
        if (!ctrlDown && key >= 32 && key <= 126 && len < (int)sizeof(inputBuf) - 1) {
            inputBuf[len] = (char)key;
            inputBuf[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && inputBuf[0] != '\0') {
        int len = (int)strlen(inputBuf);
        inputBuf[len - 1] = '\0';
    }

    BeginScissorMode((int)inputRect.x + 12, (int)inputRect.y + 5, (int)inputRect.width - 24, (int)inputRect.height - 10);
    drawText(inputBuf[0] ? inputBuf : "Type value here", (int)inputRect.x + 12, (int)inputRect.y + 10, FONT_BODY, inputBuf[0] ? CLR_TEXT : CLR_TEXT_MUTED);
    if (((int)(GetTime() * 2.0) % 2) == 0) {
        int caretX = (int)inputRect.x + 13 + measureText(inputBuf, FONT_BODY);
        DrawRectangle(caretX, (int)inputRect.y + 10, 2, FONT_BODY + 4, CLR_ACCENT);
    }
    EndScissorMode();


    int submit = 0;
    if (drawButton(dx + dw - 164 - 18, dy + 158, 164, 42, "SUBMIT (Enter)", CLR_ACCENT, CLR_ACCENT_HI, CLR_BG, 1))
        submit = 1;
    if (IsKeyPressed(KEY_ENTER)) submit = 1;

    if (submit && strlen(inputBuf) > 0) {
        simProvideInput(inputBuf);
        inputBuf[0] = '\0';
        inputActive = 0;
    }
}



static int prevLogSerial = 0;
static void updateSwapFlash(float dt) {
    for (int i = 0; i < GUI_MAX_PROC; i++)
        if (swapFlashT[i] > 0) swapFlashT[i] -= dt;

    if (simState.logSerial < prevLogSerial)
        prevLogSerial = 0;


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



void guiRun(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "OS Simulation - CSEN 602");
    SetWindowMinSize(1200, 760);
    SetTargetFPS(60);



    GuiLoadStyleDefault();


    int cps[95];
    for (int i = 0; i < 95; i++) cps[i] = 32 + i;

    guiFont = LoadFontEx("C:\\Windows\\Fonts\\segoeui.ttf", 40, cps, 95);
    if (guiFont.texture.id == 0 || guiFont.glyphCount < 10)
        guiFont = LoadFontEx("C:\\Windows\\Fonts\\consola.ttf", 40, cps, 95);
    if (guiFont.texture.id == 0 || guiFont.glyphCount < 10)
        guiFont = LoadFontEx("C:\\Windows\\Fonts\\arial.ttf", 40, cps, 95);
    if (guiFont.texture.id == 0 || guiFont.glyphCount < 10)
        guiFont = GetFontDefault();

    if (guiFont.texture.id != 0 && guiFont.glyphCount >= 10) {
        fontLoaded = 1;
        SetTextureFilter(guiFont.texture, TEXTURE_FILTER_BILINEAR);
        GuiSetFont(guiFont);
        GuiSetStyle(DEFAULT, TEXT_SIZE, FONT_BODY);
        GuiSetStyle(TEXTBOX, TEXT_SIZE, FONT_BODY);
    }

    TraceLog(LOG_INFO, "FONT: loaded=%d glyphs=%d baseSize=%d texId=%u",
             fontLoaded, guiFont.glyphCount, guiFont.baseSize, guiFont.texture.id);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int w = GetScreenWidth();
        int h = GetScreenHeight();


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
        drawParticleBackground(w, h, dt);


        drawTopBar(w, dt);
        drawControlStrip(HDR_H, w);


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
        drawParticleOverlay();


        int logY = mainY + mainH + PAD;
        drawLogPanel(PAD, logY, w - PAD * 2, LOG_H);


        drawInputModal(w, h);

        EndDrawing();
    }

    if (fontLoaded) UnloadFont(guiFont);
    CloseWindow();
}
