#include "homescreen.h"

#include "lvgl/lvgl.h"

#include "src/gui/fonts/fonts.h"
#include "src/gui/shell/draglist.h"
#include "src/gui/shell/hometiles.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/lang.h"

lv_obj_t *homescreen_screen;

// The home screen's tiles on the two-list page (draglist.h): the ones on the
// home screen on top, the ones under More underneath.

static int in_count(void) { return hometiles_home_count(); }
static int in_at(int position) { return (int)hometiles_home_at(position); }
static int out_count(void) { return hometiles_more_count(); }
static int out_at(int position) { return (int)hometiles_more_at(position); }
static const char *tag(int item) { return hometiles_tag((home_tile_t)item); }
static const lv_image_dsc_t *icon(int item) { return hometiles_icon((home_tile_t)item); }
static void move_in(int item, int position) { hometiles_move_home((home_tile_t)item, position); }
static void move_out(int item, int position) { hometiles_move_more((home_tile_t)item, position); }

static const draglist_model_t MODEL = {
	.item_count = HOME_TILE_COUNT,
	.capacity = HOMETILES_HOME_SLOTS,
	.in_heading = "home_screen_on_home",
	.out_heading = "home_screen_under_more",
	.recolor_icons = false,
	.in_count = in_count,
	.in_at = in_at,
	.out_count = out_count,
	.out_at = out_at,
	.tag = tag,
	.icon = icon,
	.move_in = move_in,
	.move_out = move_out,
};

void homescreen_init(gui_config_t *cfg) {
	homescreen_screen = lv_obj_create(NULL);
	lv_obj_add_style(homescreen_screen, &theme_style_screen, 0);

	lv_obj_t *page = settingsrow_page(homescreen_screen, cfg, "home_screen");

	lv_obj_t *note = lv_label_create(page);
	lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(note, lv_pct(100));
	lv_obj_add_style(note, &theme_style_text_dim, 0);
	lv_obj_set_style_text_font(note, &font_ui_22, 0);
	lv_label_set_text(note, tr("home_screen_note"));

	draglist_create(homescreen_screen, page, cfg, &MODEL);
	switcher_attach_back_gesture(homescreen_screen);
}
