#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// The page that arranges the home screen: which tiles are on it, in what
// order, and which sit under More (hometiles.h). Reached from Appearance.
extern lv_obj_t *homescreen_screen;

void homescreen_init(gui_config_t *cfg);

#endif /* HOMESCREEN_H */
