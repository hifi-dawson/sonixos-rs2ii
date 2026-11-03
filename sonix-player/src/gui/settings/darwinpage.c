#include "darwinpage.h"

#include <stdint.h>
#include <stdio.h>

#include "src/gui/fonts/fonts.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/system/audio/darwin.h"
#include "src/system/core/lang.h"

// Made in gui.c with the other screens: the main menu's Darwin tile points
// at it before this page is built.
lv_obj_t *darwin_screen;
static lv_obj_t *filter_screen;
static lv_obj_t *filter_checks[DARWIN_FILTER_COUNT];
static lv_obj_t *filter_value;
static lv_obj_t *nos_switch;
static lv_obj_t *harmonic_switch, *harmonic_card, *harmonic_value, *harmonic_slider;
static lv_obj_t *gain_pills, *gain_pill[DARWIN_GAIN_COUNT];
static lv_obj_t *dsd_gain_pills, *dsd_gain_pill[DARWIN_DSD_GAIN_COUNT];

static const char *const GAIN_KEYS[DARWIN_GAIN_COUNT] = {"darwin_gain_low", "darwin_gain_mid", "darwin_gain_high"};
// The stock player's "0 dB" and three steps whose size is not known yet.
static const char *const DSD_GAIN_TEXT[DARWIN_DSD_GAIN_COUNT] = {"0 dB", "1", "2", "3"};

// The level 0..255 in single steps, as the stock player has it: 0 is off.
#define HARMONIC_STEPS (DARWIN_HARMONIC_MAX + 1)

const char *darwinpage_filter_short(void) {
	static char text[24];
	int index = darwin_filter();
	if (index == 0) {
		return tr("darwin_filter_default");
	}
	snprintf(text, sizeof(text), "Darwin %d", index);
	return text;
}

// ---------------------------------------------------------------------------
// The filters: eleven rows and a check mark, as on the CS43198's own page.
// ---------------------------------------------------------------------------

static void refresh_filter(void) {
	int active = darwin_filter();
	for (int i = 0; i < DARWIN_FILTER_COUNT; i++) {
		if (filter_checks[i]) {
			lv_label_set_text(filter_checks[i], i == active ? "\xE2\x9C\x93" : "");
			lv_obj_set_style_text_color(filter_checks[i], theme()->accent, 0);
		}
	}
	if (filter_value) {
		lv_label_set_text(filter_value, darwinpage_filter_short());
	}
}

static void filter_row_cb(lv_event_t *e) {
	darwin_set_filter((int)(intptr_t)lv_event_get_user_data(e));
	refresh_filter();
}

static void build_filter_page(gui_config_t *cfg) {
	filter_screen = lv_obj_create(NULL);
	lv_obj_t *container = settingsrow_page(filter_screen, cfg, "darwin_digital_filter");

	for (int i = 0; i < DARWIN_FILTER_COUNT; i++) {
		lv_obj_t *row = settingsrow_add(container, darwin_filter_key(i), &filter_checks[i], filter_row_cb,
										(void *)(intptr_t)i);
		settingsrow_name_lines(row, 2);
		lv_obj_set_style_text_font(filter_checks[i], &font_ui_26, 0);
	}

	lv_obj_t *note = lv_label_create(container);
	lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(note, lv_pct(100));
	lv_obj_add_style(note, &theme_style_text_dim, 0);
	lv_obj_set_style_text_font(note, &font_ui_22, 0);
	lv_label_set_text(note, tr("darwin_filter_note"));

	switcher_attach_back_gesture(filter_screen);
}

// ---------------------------------------------------------------------------
// NOS and the harmonics
// ---------------------------------------------------------------------------

static void nos_toggle_cb(lv_event_t *e) {
	(void)e;
	darwin_set_nos(lv_obj_has_state(nos_switch, LV_STATE_CHECKED));
}

// ---------------------------------------------------------------------------
// The two gains on the card: three positions and four, each a row of pills.
// ---------------------------------------------------------------------------

static void refresh_gains(void) {
	for (int i = 0; i < DARWIN_GAIN_COUNT; i++) {
		settingsrow_pill_active(gain_pill[i], i == darwin_gain());
	}
	for (int i = 0; i < DARWIN_DSD_GAIN_COUNT; i++) {
		settingsrow_pill_active(dsd_gain_pill[i], i == darwin_dsd_gain());
	}
}

static void gain_pick_cb(lv_event_t *e) {
	if (player_sheet_drag_active() || switcher_back_drag_active()) {
		return;
	}
	darwin_set_gain((int)(intptr_t)lv_event_get_user_data(e));
	refresh_gains();
}

static void dsd_gain_pick_cb(lv_event_t *e) {
	if (player_sheet_drag_active() || switcher_back_drag_active()) {
		return;
	}
	darwin_set_dsd_gain((int)(intptr_t)lv_event_get_user_data(e));
	refresh_gains();
}

static void refresh_harmonic(void) {
	bool on = darwin_harmonic_enabled();
	lv_label_set_text_fmt(harmonic_value, "%d", darwin_harmonic_level());
	if (on) {
		lv_obj_add_state(harmonic_switch, LV_STATE_CHECKED);
	} else {
		lv_obj_remove_state(harmonic_switch, LV_STATE_CHECKED);
	}
	settingsrow_toggle_slider_expanded(harmonic_card, on);
}

static void harmonic_toggle_cb(lv_event_t *e) {
	(void)e;
	darwin_harmonic_set_enabled(lv_obj_has_state(harmonic_switch, LV_STATE_CHECKED));
	refresh_harmonic();
}

static void harmonic_slider_cb(lv_event_t *e) {
	(void)e;
	darwin_harmonic_set_level((int)lv_slider_get_value(harmonic_slider));
	refresh_harmonic();
}

static void darwin_loaded_cb(lv_event_t *e) {
	(void)e;
	refresh_filter();
	refresh_gains(); // the control centre moves the gain from any page
}

// The control centre moved it with this page underneath.
static void darwin_refresh_cb(lv_event_t *e) {
	(void)e;
	refresh_gains();
}

void darwinpage_init(gui_config_t *cfg) {
	lv_obj_t *container = settingsrow_page(darwin_screen, cfg, "darwin");

	build_filter_page(cfg);
	settingsrow_add(container, "darwin_digital_filter", &filter_value, switch_screen_cb, filter_screen);

	settingsrow_toggle(container, "darwin_nos", &nos_switch, nos_toggle_cb);
	if (darwin_nos()) {
		lv_obj_add_state(nos_switch, LV_STATE_CHECKED);
	}

	settingsrow_pills(container, "darwin_gain", &gain_pills);
	for (int i = 0; i < DARWIN_GAIN_COUNT; i++) {
		gain_pill[i] = settingsrow_pill(gain_pills, GAIN_KEYS[i], i, gain_pick_cb);
	}

	settingsrow_pills(container, "darwin_dsd_gain", &dsd_gain_pills);
	for (int i = 0; i < DARWIN_DSD_GAIN_COUNT; i++) {
		dsd_gain_pill[i] = settingsrow_pill_text(dsd_gain_pills, DSD_GAIN_TEXT[i], i, dsd_gain_pick_cb);
	}

	harmonic_card = settingsrow_toggle_slider(container, "darwin_harmonic", HARMONIC_STEPS, &harmonic_switch,
											  &harmonic_value, &harmonic_slider, harmonic_toggle_cb,
											  harmonic_slider_cb);
	lv_slider_set_value(harmonic_slider, darwin_harmonic_level(), LV_ANIM_OFF);

	lv_obj_t *note = lv_label_create(container);
	lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(note, lv_pct(100));
	lv_obj_add_style(note, &theme_style_text_dim, 0);
	lv_obj_set_style_text_font(note, &font_ui_22, 0);
	lv_label_set_text(note, tr("darwin_harmonic_note"));

	refresh_filter();
	refresh_harmonic();
	refresh_gains();
	theme_register_refresh(refresh_filter);
	theme_register_refresh(refresh_gains);
	lv_obj_add_event_cb(darwin_screen, darwin_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
	lv_obj_add_event_cb(darwin_screen, darwin_refresh_cb, LV_EVENT_REFRESH, NULL);
	switcher_attach_back_gesture(darwin_screen);
}

lv_obj_t *darwinpage_screen(void) { return darwin_screen; }
