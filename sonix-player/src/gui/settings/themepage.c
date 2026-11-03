#include "themepage.h"

#include <stdint.h>

#include "lvgl/lvgl.h"

#include "src/gui/fonts/fonts.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/lang.h"

lv_obj_t *themepage_screen;

// The same green the language and boot logo pages mark their choice with.
#define CHECK_GREEN lv_color_make(46, 194, 126)

static const char *card_root;
static lv_obj_t *rows[THEME_PACK_MAX];
static lv_obj_t *checks[THEME_PACK_MAX];
static lv_obj_t *swatches[THEME_PACK_MAX];

static void paint(void) {
	int chosen = theme_get_pack();
	int count = theme_pack_count();
	for (int i = 0; i < THEME_PACK_MAX; i++) {
		if (i >= count) {
			lv_obj_set_hidden(rows[i], true);
			continue;
		}
		const theme_pack_t *pack = theme_pack(i);
		lv_label_set_text(settingsrow_name_label(rows[i]), pack->builtin ? tr(pack->display) : pack->display);
		// The pack's accent, on its dark background: enough to tell the packs
		// apart before trying one.
		lv_obj_set_style_bg_color(swatches[i], pack->dark.accent, 0);
		lv_obj_set_style_border_color(swatches[i], pack->dark.screen_bg, 0);
		lv_obj_set_hidden(checks[i], i != chosen);
		lv_obj_set_hidden(rows[i], false);
	}
}

static void pick_cb(lv_event_t *e) {
	if (switcher_back_drag_active()) {
		return;
	}
	int index = (int)(intptr_t)lv_event_get_user_data(e);
	theme_set_pack(index);
	paint();
}

static void loaded_cb(lv_event_t *e) {
	(void)e;
	// The card may have gained or lost a pack since the page was last open.
	theme_packs_scan(card_root);
	paint();
}

static void refresh_theme(void) { paint(); }

void themepage_init(gui_config_t *cfg) {
	card_root = cfg->sd_root_path;

	themepage_screen = lv_obj_create(NULL);
	lv_obj_add_style(themepage_screen, &theme_style_screen, 0);

	lv_obj_t *container = settingsrow_page(themepage_screen, cfg, "appearance_theme_pack");

	for (int i = 0; i < THEME_PACK_MAX; i++) {
		rows[i] = settingsrow_action(container, "", pick_cb, (void *)(intptr_t)i);
		settingsrow_name_lines(rows[i], 1);

		// A swatch and the check share the row's right end: the swatch
		// inboard of where the check goes.
		lv_obj_t *swatch = lv_obj_create(rows[i]);
		swatches[i] = swatch;
		lv_obj_remove_style_all(swatch);
		lv_obj_set_size(swatch, 28, 28);
		lv_obj_set_style_radius(swatch, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
		lv_obj_set_style_border_width(swatch, 3, 0);
		lv_obj_set_style_border_opa(swatch, LV_OPA_COVER, 0);
		lv_obj_set_clickable(swatch, false);
		lv_obj_align(swatch, LV_ALIGN_RIGHT_MID, -44, 0);

		lv_obj_t *mark = lv_image_create(rows[i]);
		checks[i] = mark;
		lv_image_set_src(mark, &icon_check);
		lv_obj_set_style_image_recolor(mark, CHECK_GREEN, 0);
		lv_obj_set_style_image_recolor_opa(mark, LV_OPA_COVER, 0);
		lv_obj_align(mark, LV_ALIGN_RIGHT_MID, 0, 0);
		lv_obj_set_hidden(mark, true);

		lv_obj_set_hidden(rows[i], true);
	}

	lv_obj_t *note = lv_label_create(container);
	lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(note, lv_pct(100));
	lv_obj_add_style(note, &theme_style_text_dim, 0);
	lv_obj_set_style_text_font(note, &font_ui_22, 0);
	lv_label_set_text(note, tr("theme_pack_note"));

	theme_packs_scan(card_root);
	paint();

	lv_obj_add_event_cb(themepage_screen, loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
	switcher_attach_back_gesture(themepage_screen);
	theme_register_refresh(refresh_theme);
}
