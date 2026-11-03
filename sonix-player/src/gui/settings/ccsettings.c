#include "ccsettings.h"

#include "lvgl/lvgl.h"

#include "src/gui/shell/draglist.h"
#include "src/gui/shell/quickpanel.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"

lv_obj_t *ccsettings_screen;

// The control centre's buttons on the two-list page (draglist.h): the ones in
// the panel on top, the ones left out underneath.

static int in_count(void) { return quickpanel_in_use_count(); }
static int in_at(int position) { return (int)quickpanel_in_use_at(position); }
static int out_count(void) { return quickpanel_hidden_count(); }
static int out_at(int position) { return (int)quickpanel_hidden_at(position); }
static const char *tag(int item) { return quickpanel_button_tag((quickpanel_button_t)item); }
static const lv_image_dsc_t *icon(int item) { return quickpanel_button_icon((quickpanel_button_t)item); }
static void move_in(int item, int position) { quickpanel_move_in_use((quickpanel_button_t)item, position); }
static void move_out(int item, int position) { quickpanel_move_hidden((quickpanel_button_t)item, position); }

static const draglist_model_t MODEL = {
	.item_count = QP_BTN_COUNT,
	.capacity = QP_SLOT_COUNT,
	.in_heading = "in_use",
	.out_heading = "controlcentre_not_in_use",
	.recolor_icons = true,
	.in_count = in_count,
	.in_at = in_at,
	.out_count = out_count,
	.out_at = out_at,
	.tag = tag,
	.icon = icon,
	.move_in = move_in,
	.move_out = move_out,
};

void ccsettings_init(gui_config_t *cfg) {
	ccsettings_screen = lv_obj_create(NULL);
	lv_obj_add_style(ccsettings_screen, &theme_style_screen, 0);

	lv_obj_t *page = settingsrow_page(ccsettings_screen, cfg, "control_centre");
	draglist_create(ccsettings_screen, page, cfg, &MODEL);
	switcher_attach_back_gesture(ccsettings_screen);
}
