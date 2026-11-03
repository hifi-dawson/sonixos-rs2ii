#ifndef THEMEPAGE_H
#define THEMEPAGE_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// Appearance > Theme: the theme packs (theme.h), the built-in two and the
// card's, one row each with a check on the one in force.
extern lv_obj_t *themepage_screen;

void themepage_init(gui_config_t *cfg);

#endif /* THEMEPAGE_H */
