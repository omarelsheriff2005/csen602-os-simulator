#ifndef GUI_H
#define GUI_H

#include "raylib.h"

/* Window */
#define WIN_W  1440
#define WIN_H   860

/* Spacing system */
#define PAD      12
#define GAP       8
#define HDR_H    52      /* Title bar */
#define CTRL_H   56      /* Controls strip */
#define LOG_H   180      /* Log panel height */
#define PANEL_HDR_H 30   /* Panel header height */

/* Typography (all downscaled from 64px atlas for crispness) */
#define FONT_TITLE   22
#define FONT_H1      18
#define FONT_BODY    15
#define FONT_SMALL   13
#define FONT_MONO    13
#define FONT_SZ      FONT_BODY   /* default */

/* Modern dark palette */
#define CLR_BG         (Color){ 18,  20,  28, 255 }
#define CLR_PANEL      (Color){ 32,  35,  48, 255 }
#define CLR_PANEL_HDR  (Color){ 44,  48,  66, 255 }
#define CLR_PANEL_BDR  (Color){ 58,  62,  82, 255 }
#define CLR_ROW_ALT    (Color){ 37,  40,  54, 255 }

#define CLR_TEXT       (Color){ 232, 234, 242, 255 }
#define CLR_TEXT_DIM   (Color){ 140, 146, 166, 255 }
#define CLR_TEXT_MUTED (Color){  90,  96, 114, 255 }

#define CLR_ACCENT     (Color){ 110, 150, 255, 255 }   /* blue */
#define CLR_ACCENT_HI  (Color){ 150, 180, 255, 255 }

/* State colors */
#define CLR_READY      (Color){  92, 200, 130, 255 }   /* green */
#define CLR_RUNNING    (Color){ 255, 190,  80, 255 }   /* amber */
#define CLR_BLOCKED    (Color){ 230,  90,  95, 255 }   /* red */
#define CLR_FINISHED   (Color){ 120, 128, 148, 255 }   /* gray */

/* Event highlights */
#define CLR_SWAP_FLASH (Color){ 255, 140,  60, 255 }   /* orange */
#define CLR_BTN_BG     (Color){  55,  60,  82, 255 }
#define CLR_BTN_HOVER  (Color){  72,  80, 110, 255 }
#define CLR_BTN_DANGER (Color){ 140,  55,  65, 255 }

void guiRun(void);

#endif
