#ifndef DRAGLIST_H
#define DRAGLIST_H

#include <stdbool.h>

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// ---------------------------------------------------------------------------
// Two lists on one canvas
//
// The page the control centre and the home screen are arranged on: the items
// in use on top, the ones left out underneath, a heading each. Each row
// carries the item's picture and its name, so what a glyph is for can be read
// here rather than guessed.
//
// A row moves the way a playlist entry does while it is being reordered: it is
// taken by its grip and nowhere else, it wears the accent border while it is
// carried, and an accent rule shows where it would land. Both lists live on one
// canvas, so a row crossing from one to the other is one coordinate and not a
// change of parent. The page scrolls when the rows run past the screen, and it
// scrolls by itself while a row is held near its top or bottom edge.
//
// What the lists hold is the model's business: the widget asks it how many
// items are in each list and which, and tells it where a dropped item went.
// Items are small integers, the model's own enum.
// ---------------------------------------------------------------------------

typedef struct {
	int item_count; // every item has a row, in either list
	int capacity;	// places in the first list, for the "n/N" beside its heading; 0 for none
	const char *in_heading; // translation tags
	const char *out_heading;
	bool recolor_icons; // glyphs painted in the theme's icon colour, or pictures left as they are

	int (*in_count)(void);
	int (*in_at)(int position);
	int (*out_count)(void);
	int (*out_at)(int position);
	const char *(*tag)(int item);
	const lv_image_dsc_t *(*icon)(int item);
	// Moving an item to a position in either list: the model saves the
	// layout and keeps its lists consistent (an item past the last place of
	// a full first list pushes the last one out, say).
	void (*move_in)(int item, int position);
	void (*move_out)(int item, int position);
} draglist_model_t;

typedef struct draglist draglist_t;

// Builds the canvas in `page` (a settingsrow_page() container) and refreshes
// it whenever `screen` is shown. The model must outlive the list.
draglist_t *draglist_create(lv_obj_t *screen, lv_obj_t *page, gui_config_t *cfg, const draglist_model_t *model);

// Lays the rows out again from the model: after the layout changed elsewhere.
void draglist_refresh(draglist_t *list);

#endif /* DRAGLIST_H */
