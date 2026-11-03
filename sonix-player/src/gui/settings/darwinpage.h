#ifndef DARWINPAGE_H
#define DARWINPAGE_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// Darwin, the RS2 II's converter (src/system/audio/darwin.h): its digital
// filter, NOS and the harmonic stage, on one page the way the stock player
// groups them. Only built on the Darwin board; the music page shows its row
// in place of the CS43198's filters, NOS and DRE.
// The page's screen, created in gui.c with the others so that the main
// menu's Darwin tile can point at it before the page is built.
extern lv_obj_t *darwin_screen;

void darwinpage_init(gui_config_t *cfg);

lv_obj_t *darwinpage_screen(void);

// The filter's short name ("Darwin 3"), for a row that opens this page.
const char *darwinpage_filter_short(void);

#endif /* DARWINPAGE_H */
