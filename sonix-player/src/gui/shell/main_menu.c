#include "main_menu.h"

#include <stdint.h>

#include "src/gui/shell/gridpage.h"
#include "src/gui/shell/gui.h"
#include "src/gui/shell/hometiles.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/morepage.h"

#include "lvgl/lvgl.h"

lv_obj_t *main_menu_screen;

static lv_obj_t *grid;
static gui_config_t *config;

// The tiles the user put on the home screen (hometiles.h), then the door to
// the More page, which holds every other tile. The main menu has six places,
// and the last is always that door, so the page can keep growing.
static int entries_from_layout(grid_entry_t *entries) {
	int n = 0;
	for (int i = 0; i < hometiles_home_count() && n < HOMETILES_HOME_SLOTS; i++) {
		entries[n++] = hometiles_entry(hometiles_home_at(i));
	}
	entries[n++] = (grid_entry_t){"more", &icon_menu_more, &morepage_screen, NULL};
	return n;
}

static void layout_changed(void) {
	grid_entry_t entries[HOMETILES_HOME_SLOTS + 1];
	int n = entries_from_layout(entries);
	gridpage_set_entries(grid, config, entries, n, 2, 3, false);
}

void main_menu_init(gui_config_t *cfg) {
	config = cfg;
	grid_entry_t entries[HOMETILES_HOME_SLOTS + 1];
	int n = entries_from_layout(entries);
	grid = gridpage_build(main_menu_screen, cfg, entries, n, 2, 3, false);
	hometiles_on_change(layout_changed);
}
