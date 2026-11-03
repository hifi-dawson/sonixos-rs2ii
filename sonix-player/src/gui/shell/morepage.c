#include "morepage.h"

#include "lvgl/lvgl.h"

#include "src/gui/shell/gridpage.h"
#include "src/gui/shell/hometiles.h"
#include "src/gui/shell/settingsrow.h"

lv_obj_t *morepage_screen;

static lv_obj_t *grid;
static gui_config_t *config;

// Every tile that is not on the home screen (hometiles.h), in the user's
// order.
static int entries_from_layout(grid_entry_t *entries) {
	int n = 0;
	for (int i = 0; i < hometiles_more_count() && n < HOME_TILE_COUNT; i++) {
		entries[n++] = hometiles_entry(hometiles_more_at(i));
	}
	return n;
}

static void layout_changed(void) {
	grid_entry_t entries[HOME_TILE_COUNT];
	int n = entries_from_layout(entries);
	gridpage_set_entries(grid, config, entries, n, 2, 3, true);
	if (n > 6) {
		gridpage_set_scrolling(grid);
	}
}

void morepage_init(gui_config_t *cfg) {
	config = cfg;
	grid_entry_t entries[HOME_TILE_COUNT];
	int n = entries_from_layout(entries);

	// Two columns by three rows, like the Music and Wireless pages, so a tile
	// here is exactly the size of a tile there. The grid is laid out as full
	// even with fewer entries, so the only tile sits where it would with six --
	// top left -- instead of floating in the middle of a page of its own. With
	// more than six it scrolls.
	grid = gridpage_build(morepage_screen, cfg, entries, n, 2, 3, true);
	if (n > 6) {
		gridpage_set_scrolling(grid);
	}

	settingsrow_title(morepage_screen, cfg, "more");
	hometiles_on_change(layout_changed);
}
