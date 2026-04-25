#ifndef GUI_H
#define GUI_H

#include "raylib.h"


#define WIN_W  1440
#define WIN_H   860


#define PAD      14
#define GAP      10
#define HDR_H    64
#define CTRL_H   60
#define LOG_H   230
#define PANEL_HDR_H 36


#define FONT_TITLE   26
#define FONT_H1      20
#define FONT_BODY    17
#define FONT_SMALL   14
#define FONT_MONO    15
#define FONT_SZ      FONT_BODY


#define CLR_BG         (Color){ 12,  15,  18, 255 }
#define CLR_PANEL      (Color){ 24,  29,  32, 235 }
#define CLR_PANEL_HDR  (Color){ 31,  38,  41, 245 }
#define CLR_PANEL_BDR  (Color){ 56,  70,  66, 255 }
#define CLR_ROW_ALT    (Color){ 30,  36,  39, 225 }

#define CLR_TEXT       (Color){ 235, 241, 237, 255 }
#define CLR_TEXT_DIM   (Color){ 166, 181, 174, 255 }
#define CLR_TEXT_MUTED (Color){ 105, 121, 114, 255 }

#define CLR_ACCENT     (Color){  63, 216, 138, 255 }
#define CLR_ACCENT_HI  (Color){ 128, 240, 180, 255 }


#define CLR_READY      (Color){  74, 222, 128, 255 }
#define CLR_RUNNING    (Color){ 185, 240, 116, 255 }
#define CLR_BLOCKED    (Color){ 239,  94,  94, 255 }
#define CLR_FINISHED   (Color){ 122, 143, 136, 255 }


#define CLR_SWAP_FLASH (Color){  45, 212, 191, 255 }
#define CLR_BTN_BG     (Color){  38,  48,  50, 255 }
#define CLR_BTN_HOVER  (Color){  48,  67,  62, 255 }
#define CLR_BTN_DANGER (Color){ 142,  54,  58, 255 }

void guiRun(void);

#endif
