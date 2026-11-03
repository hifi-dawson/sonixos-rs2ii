#include "draglist.h"

#include <stdlib.h>

#include "src/gui/fonts/fonts.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/lang.h"

#define ROW_HEIGHT 72
#define ROW_GAP 8
#define ROW_PITCH (ROW_HEIGHT + ROW_GAP)
#define ROW_RADIUS 12
#define ROW_PAD 20
#define ICON_BOX 56
#define HEADING_H 40

// The grip is only as large as its glyph and a margin round it; the room kept
// for it at the end of the row is wider.
#define GRIP_SIZE 48
#define GRIP_SPACE 64

// Either list is a drop target even when it is empty, so neither is ever
// shorter than one row.
#define LIST_MIN_ROWS 1

// How far the finger has to travel before a release counts as a drop.
#define DROP_MIN_TRAVEL 8

// How close to the top or bottom of the page a held row has to be before the
// page moves under it, and how far it moves each time the event comes round.
#define EDGE_PX 70
#define EDGE_STEP 18

typedef struct {
	draglist_t *list; // the grip's event handler finds its list through the row
	int index;
	lv_obj_t *row;
	lv_obj_t *icon;
	lv_obj_t *name;
	lv_obj_t *grip;
} entry_t;

struct draglist {
	const draglist_model_t *model;
	lv_obj_t *page;
	lv_obj_t *body;
	lv_obj_t *in_use_count;
	lv_obj_t *others_heading;
	lv_obj_t *drop_line;
	entry_t *entries; // item_count of them

	// Which item each row is showing. The first in_count() rows are the first
	// list's, the rest the second's. Rebuilt at every repaint, so the drag
	// reads them rather than a number baked in at build time.
	int *row_item;
	int row_count;

	// The row being carried, or -1.
	int drag_row;
	bool drag_moved;
	lv_point_t drag_from;
};

// Where each part of the canvas starts. The second heading follows the first
// list, so both move as items cross between them.
static int in_use_top(void) { return HEADING_H; }

static int others_heading_y(const draglist_t *l) {
	int n = l->model->in_count();
	return in_use_top() + (n < LIST_MIN_ROWS ? LIST_MIN_ROWS : n) * ROW_PITCH + 8;
}

static int others_top(const draglist_t *l) { return others_heading_y(l) + HEADING_H; }

void draglist_refresh(draglist_t *l) {
	const draglist_model_t *m = l->model;
	int in_n = m->in_count();
	int out_n = m->out_count();
	l->row_count = 0;

	for (int i = 0; i < in_n && l->row_count < m->item_count; i++, l->row_count++) {
		l->row_item[l->row_count] = m->in_at(i);
		lv_obj_set_y(l->entries[l->row_count].row, in_use_top() + i * ROW_PITCH);
	}
	for (int i = 0; i < out_n && l->row_count < m->item_count; i++, l->row_count++) {
		l->row_item[l->row_count] = m->out_at(i);
		lv_obj_set_y(l->entries[l->row_count].row, others_top(l) + i * ROW_PITCH);
	}

	for (int i = 0; i < m->item_count; i++) {
		entry_t *e = &l->entries[i];
		if (i >= l->row_count) {
			lv_obj_set_hidden(e->row, true);
			continue;
		}
		lv_obj_set_hidden(e->row, false);
		const lv_image_dsc_t *icon = m->icon(l->row_item[i]);
		lv_image_set_src(e->icon, icon);
		// A picture larger than its box (a main-menu tile's) is shrunk to
		// fit; a glyph that fits is shown as drawn.
		if (icon && (icon->header.w > ICON_BOX || icon->header.h > ICON_BOX)) {
			lv_obj_set_size(e->icon, ICON_BOX, ICON_BOX);
			lv_image_set_inner_align(e->icon, LV_IMAGE_ALIGN_CONTAIN);
		} else {
			lv_obj_set_size(e->icon, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
			lv_image_set_inner_align(e->icon, LV_IMAGE_ALIGN_CENTER);
		}
		lv_label_set_text(e->name, tr(m->tag(l->row_item[i])));
	}

	if (l->in_use_count) {
		lv_label_set_text_fmt(l->in_use_count, "%d/%d", in_n, m->capacity);
	}
	lv_obj_set_y(l->others_heading, others_heading_y(l) + 10);

	int rows_below = out_n < LIST_MIN_ROWS ? LIST_MIN_ROWS : out_n;
	lv_obj_set_height(l->body, others_top(l) + rows_below * ROW_PITCH + 8);
}

static void refresh_async(void *user) { draglist_refresh(user); }

// ---------------------------------------------------------------------------
// Carrying a row
// ---------------------------------------------------------------------------

// Where a row would land if the finger lifted at `point`: which list, and the
// gap it would go into, counted with the carried row still in its place.
typedef struct {
	bool in_use;
	int at;
} target_t;

static target_t target_at(const draglist_t *l, lv_point_t point) {
	lv_area_t area;
	lv_obj_get_coords(l->body, &area);
	int y = point.y - area.y1;

	target_t t;
	t.in_use = y < others_heading_y(l);
	int top = t.in_use ? in_use_top() : others_top(l);
	int count = t.in_use ? l->model->in_count() : l->model->out_count();
	t.at = (y - top + ROW_PITCH / 2) / ROW_PITCH;
	if (t.at < 0) {
		t.at = 0;
	}
	if (t.at > count) {
		t.at = count;
	}
	return t;
}

// The accent rule in the gap a drop would go into. On the canvas, so it
// scrolls with the rows.
static void drop_line_show(draglist_t *l, target_t t) {
	int top = t.in_use ? in_use_top() : others_top(l);
	lv_obj_set_pos(l->drop_line, 0, top + t.at * ROW_PITCH - ROW_GAP / 2 - 2);
	lv_obj_set_hidden(l->drop_line, false);
	lv_obj_move_foreground(l->drop_line);
}

static void carried_look(draglist_t *l, int index, bool on) {
	lv_obj_t *row = l->entries[index].row;
	lv_obj_set_style_border_width(row, on ? 2 : 0, 0);
	if (on) {
		lv_obj_set_style_border_color(row, theme()->accent, 0);
		lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
	}
}

// The gap is counted with the carried row in place; once it is lifted out,
// every gap below it in its own list is one earlier.
static void drop(draglist_t *l, int index, target_t t) {
	int in_n = l->model->in_count();
	bool from_in_use = index < in_n;
	int from = from_in_use ? index : index - in_n;
	int at = t.at;
	if (t.in_use == from_in_use && from < at) {
		at--;
	}
	if (t.in_use == from_in_use && at == from) {
		return;
	}
	if (t.in_use) {
		l->model->move_in(l->row_item[index], at);
	} else {
		l->model->move_out(l->row_item[index], at);
	}
}

// The page follows the finger when a held row reaches either end of it.
static void edge_scroll(draglist_t *l, lv_point_t point) {
	lv_area_t view;
	lv_obj_get_coords(l->page, &view);
	int step = 0;
	if (point.y < view.y1 + EDGE_PX) {
		step = -EDGE_STEP;
	} else if (point.y > view.y2 - EDGE_PX) {
		step = EDGE_STEP;
	}
	if (step == 0) {
		return;
	}
	int scroll = lv_obj_get_scroll_y(l->page);
	int limit = scroll + lv_obj_get_scroll_bottom(l->page);
	int wanted = scroll + step;
	wanted = wanted < 0 ? 0 : (wanted > limit ? limit : wanted);
	if (wanted != scroll) {
		lv_obj_scroll_to_y(l->page, wanted, LV_ANIM_OFF);
	}
}

static void drag_cb(lv_event_t *e) {
	lv_event_code_t code = lv_event_get_code(e);
	entry_t *entry = lv_event_get_user_data(e);
	draglist_t *l = entry->list;

	lv_indev_t *indev = lv_indev_active();
	lv_point_t point = {0, 0};
	if (indev) {
		lv_indev_get_point(indev, &point);
	}

	if (code == LV_EVENT_PRESSED) {
		l->drag_row = entry->index < l->row_count ? entry->index : -1;
		if (l->drag_row < 0) {
			return;
		}
		l->drag_moved = false;
		l->drag_from = point;
		carried_look(l, l->drag_row, true);
		return;
	}

	if (l->drag_row < 0) {
		return;
	}

	if (code == LV_EVENT_PRESSING) {
		edge_scroll(l, point);
		if (LV_ABS(point.y - l->drag_from.y) > DROP_MIN_TRAVEL || LV_ABS(point.x - l->drag_from.x) > DROP_MIN_TRAVEL) {
			l->drag_moved = true;
		}
		if (l->drag_moved) {
			drop_line_show(l, target_at(l, point));
		}
		return;
	}

	if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		int index = l->drag_row;
		l->drag_row = -1;
		lv_obj_set_hidden(l->drop_line, true);
		carried_look(l, index, false);

		if (code == LV_EVENT_RELEASED && l->drag_moved) {
			drop(l, index, target_at(l, point));
		}
		// Not here: the repaint rebinds the very row whose release is being
		// handled. It waits for the event to unwind.
		lv_async_call(refresh_async, l);
	}
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

static void loaded_cb(lv_event_t *e) { draglist_refresh(lv_event_get_user_data(e)); }

static lv_obj_t *make_heading(lv_obj_t *parent, const char *tag, int y) {
	lv_obj_t *label = lv_label_create(parent);
	lv_label_set_text(label, tr(tag));
	lv_obj_add_style(label, &theme_style_text_dim, 0);
	lv_obj_set_style_text_font(label, &font_ui_22, 0);
	lv_obj_set_pos(label, 4, y + 10);
	return label;
}

static void make_row(draglist_t *l, entry_t *entry, int width) {
	lv_obj_t *row = lv_obj_create(l->body);
	lv_obj_set_size(row, width, ROW_HEIGHT);
	lv_obj_set_x(row, 0);
	lv_obj_add_style(row, &theme_style_card, 0);
	lv_obj_set_style_radius(row, ROW_RADIUS, 0);
	lv_obj_set_style_border_width(row, 0, 0);
	lv_obj_set_style_shadow_width(row, 0, 0);
	lv_obj_set_style_pad_hor(row, ROW_PAD, 0);
	lv_obj_set_style_pad_ver(row, 0, 0);
	lv_obj_set_scrollable(row, false);
	entry->row = row;

	lv_obj_t *box = lv_obj_create(row);
	lv_obj_remove_style_all(box);
	lv_obj_set_size(box, ICON_BOX, ROW_HEIGHT);
	lv_obj_align(box, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_set_clickable(box, false);

	entry->icon = lv_image_create(box);
	if (l->model->recolor_icons) {
		lv_obj_add_style(entry->icon, &theme_style_icon, 0);
		lv_obj_set_style_image_recolor_opa(entry->icon, LV_OPA_COVER, 0);
	}
	lv_obj_center(entry->icon);

	entry->name = lv_label_create(row);
	lv_obj_add_style(entry->name, &theme_style_text, 0);
	lv_obj_set_style_text_font(entry->name, &font_ui_24, 0);
	lv_label_set_long_mode(entry->name, LV_LABEL_LONG_DOT);
	lv_obj_set_width(entry->name, width - ROW_PAD - ICON_BOX - 16 - GRIP_SPACE);
	lv_obj_align(entry->name, LV_ALIGN_LEFT_MID, ICON_BOX + 16, 0);

	// The handle, and the only part of the row that can be taken hold of. Not
	// a button: a tap on it does nothing. A drag on it must not find the page
	// and scroll it, hence no scroll chain.
	entry->grip = lv_obj_create(row);
	lv_obj_set_size(entry->grip, GRIP_SIZE, GRIP_SIZE);
	// Centred in its room, which runs into the row's right padding.
	lv_obj_align(entry->grip, LV_ALIGN_RIGHT_MID, ROW_PAD - (GRIP_SPACE - GRIP_SIZE) / 2, 0);
	lv_obj_set_style_bg_opa(entry->grip, 0, 0);
	lv_obj_set_style_border_width(entry->grip, 0, 0);
	lv_obj_set_style_pad_all(entry->grip, 0, 0);
	lv_obj_set_scrollable(entry->grip, false);
	lv_obj_set_scroll_chain_ver(entry->grip, false);
	lv_obj_set_scroll_chain_hor(entry->grip, false);
	lv_obj_set_clickable(entry->grip, true);

	lv_obj_t *grip_icon = lv_image_create(entry->grip);
	lv_image_set_src(grip_icon, &icon_grip);
	lv_obj_add_style(grip_icon, &theme_style_icon, 0);
	lv_obj_set_style_image_recolor_opa(grip_icon, LV_OPA_COVER, 0);
	lv_obj_center(grip_icon);

	lv_obj_add_event_cb(entry->grip, drag_cb, LV_EVENT_PRESSED, entry);
	lv_obj_add_event_cb(entry->grip, drag_cb, LV_EVENT_PRESSING, entry);
	lv_obj_add_event_cb(entry->grip, drag_cb, LV_EVENT_RELEASED, entry);
	lv_obj_add_event_cb(entry->grip, drag_cb, LV_EVENT_PRESS_LOST, entry);
}

draglist_t *draglist_create(lv_obj_t *screen, lv_obj_t *page, gui_config_t *cfg, const draglist_model_t *model) {
	draglist_t *l = calloc(1, sizeof(*l));
	if (!l) {
		return NULL;
	}
	l->model = model;
	l->page = page;
	l->drag_row = -1;
	l->entries = calloc((size_t)model->item_count, sizeof(entry_t));
	l->row_item = calloc((size_t)model->item_count, sizeof(int));
	if (!l->entries || !l->row_item) {
		free(l->entries);
		free(l->row_item);
		free(l);
		return NULL;
	}

	int width = cfg->screen_width - 2 * cfg->padding;

	l->body = lv_obj_create(page);
	lv_obj_set_width(l->body, width);
	lv_obj_set_height(l->body, ROW_PITCH);
	lv_obj_set_style_bg_opa(l->body, 0, 0);
	lv_obj_set_style_border_width(l->body, 0, 0);
	lv_obj_set_style_pad_all(l->body, 0, 0);
	lv_obj_set_scrollable(l->body, false);

	make_heading(l->body, model->in_heading, 0);
	l->others_heading = make_heading(l->body, model->out_heading, 0);

	// How many of the places are taken, level with the first heading.
	if (model->capacity > 0) {
		l->in_use_count = lv_label_create(l->body);
		lv_obj_add_style(l->in_use_count, &theme_style_text_dim, 0);
		lv_obj_set_style_text_font(l->in_use_count, &font_ui_22, 0);
		lv_obj_align(l->in_use_count, LV_ALIGN_TOP_RIGHT, -4, 10);
	}

	for (int i = 0; i < model->item_count; i++) {
		l->entries[i].list = l;
		l->entries[i].index = i;
		make_row(l, &l->entries[i], width);
	}

	l->drop_line = lv_obj_create(l->body);
	lv_obj_set_size(l->drop_line, width, 4);
	lv_obj_set_scrollable(l->drop_line, false);
	lv_obj_set_clickable(l->drop_line, false);
	lv_obj_set_style_border_width(l->drop_line, 0, 0);
	lv_obj_set_style_shadow_width(l->drop_line, 0, 0);
	lv_obj_set_style_pad_all(l->drop_line, 0, 0);
	lv_obj_add_style(l->drop_line, &theme_style_accent_bg, 0);
	lv_obj_set_style_radius(l->drop_line, 2, 0);
	lv_obj_set_hidden(l->drop_line, true);

	lv_obj_add_event_cb(screen, loaded_cb, LV_EVENT_SCREEN_LOADED, l);

	draglist_refresh(l);
	return l;
}
