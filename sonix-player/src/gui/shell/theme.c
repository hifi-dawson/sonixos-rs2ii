#include "theme.h"

#include "src/system/device/bootlogo.h"
#include "src/system/core/config.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

lv_style_t theme_style_screen;
lv_style_t theme_style_panel;
lv_style_t theme_style_card;
lv_style_t theme_style_card_pressed;
lv_style_t theme_style_text;
lv_style_t theme_style_text_dim;
lv_style_t theme_style_icon;
lv_style_t theme_style_switch;		   // switch track, off state
lv_style_t theme_style_switch_checked; // switch indicator, on state
lv_style_t theme_style_accent_bg;	   // anything that must follow the accent
lv_style_t theme_style_slider_track;   // a slider's unfilled part

// The built-in packs (theme.h).
//
// Classic is GNOME Adwaita (libadwaita), as upstream Sonix has it. Dark:
// window #242424, cards #303030 (white at 8%% over the window), accent blue
// #3584e4, secondary text white at 55%% flattened against the card. Light:
// cards white, pressed = black at 8%%, text black at 80%%; the window is
// pushed further from white than stock Adwaita's #fafafa, because on this
// panel white cards on a #fafafa window are indistinguishable.
//
// Midnight is a blue one: a night of deep blue-black with cool
// white text and a clear sky-blue accent, and by day a pale blue-white page,
// white cards and the same blue a shade deeper.
static const theme_pack_t BUILTIN_PACKS[] = {
	{
		.name = "classic",
		.display = "theme_pack_classic",
		.builtin = true,
		.dark =
			{
				.screen_bg = LV_COLOR_MAKE(36, 36, 36),
				.surface = LV_COLOR_MAKE(48, 48, 48),
				.surface_pressed = LV_COLOR_MAKE(64, 64, 64),
				.panel = LV_COLOR_MAKE(30, 30, 30),
				.cover_bg = LV_COLOR_MAKE(30, 30, 30),
				.text_primary = LV_COLOR_MAKE(255, 255, 255),
				.text_secondary = LV_COLOR_MAKE(160, 160, 160),
				.accent = LV_COLOR_MAKE(53, 132, 228),
				.dark = true,
			},
		.light =
			{
				.screen_bg = LV_COLOR_MAKE(236, 237, 240),
				.surface = LV_COLOR_MAKE(255, 255, 255),
				.surface_pressed = LV_COLOR_MAKE(221, 223, 228),
				.panel = LV_COLOR_MAKE(255, 255, 255),
				.cover_bg = LV_COLOR_MAKE(224, 226, 230),
				.text_primary = LV_COLOR_MAKE(50, 50, 50),
				.text_secondary = LV_COLOR_MAKE(115, 115, 115),
				.accent = LV_COLOR_MAKE(53, 132, 228),
				.dark = false,
			},
	},
	{
		.name = "midnight",
		.display = "theme_pack_midnight",
		.builtin = true,
		.dark =
			{
				.screen_bg = LV_COLOR_MAKE(17, 24, 32),
				.surface = LV_COLOR_MAKE(27, 37, 49),
				.surface_pressed = LV_COLOR_MAKE(39, 53, 68),
				.panel = LV_COLOR_MAKE(13, 18, 24),
				.cover_bg = LV_COLOR_MAKE(13, 18, 24),
				.text_primary = LV_COLOR_MAKE(238, 242, 247),
				.text_secondary = LV_COLOR_MAKE(159, 176, 194),
				.accent = LV_COLOR_MAKE(79, 156, 240),
				.dark = true,
			},
		.light =
			{
				.screen_bg = LV_COLOR_MAKE(238, 243, 248),
				.surface = LV_COLOR_MAKE(255, 255, 255),
				.surface_pressed = LV_COLOR_MAKE(219, 229, 240),
				.panel = LV_COLOR_MAKE(255, 255, 255),
				.cover_bg = LV_COLOR_MAKE(223, 231, 239),
				.text_primary = LV_COLOR_MAKE(31, 42, 54),
				.text_secondary = LV_COLOR_MAKE(95, 111, 128),
				.accent = LV_COLOR_MAKE(47, 127, 214),
				.dark = false,
			},
	},
};
#define BUILTIN_PACK_COUNT ((int)(sizeof(BUILTIN_PACKS) / sizeof(BUILTIN_PACKS[0])))
#define DEFAULT_PACK "classic"

// The packs on offer: the built-in ones, then the card's. Rebuilt by
// theme_packs_scan().
static theme_pack_t packs[THEME_PACK_MAX];
static int pack_count;
static int pack_index;

// Working copies of the chosen pack's two palettes, with an accent preset
// laid over both when one is chosen rather than the pack's own.
static theme_palette_t palette_dark;
static theme_palette_t palette_light;

// Whether the dark preset is the chosen one, kept apart from `active` because
// with the dynamic tint on `active` points at a derived copy rather than at
// either preset.
static bool dark_selected = true;

// The dynamic tint: the neutral colours -- backgrounds, cards, the status bar
// -- borrow the accent's hue, the way Material You colours a system from one
// seed. Off by default, remembered in the config.
static bool dynamic_tint;

// The tinted copy of whichever preset is chosen. Rebuilt on every theme,
// accent or tint change; `active` points here while the tint is on.
static theme_palette_t tinted;

static theme_palette_t *active = &palette_dark;

// The accent presets, Adwaita's named accents, ordered as requested: blue
// (the default), then warm to cool -- orange, yellow, green, purple, pink.
static const lv_color_t ACCENT_COLORS[THEME_ACCENT_COUNT] = {
	LV_COLOR_MAKE(53, 132, 228), // blue (#3584e4, the default)
	LV_COLOR_MAKE(255, 120, 0),  // orange (#ff7800)
	LV_COLOR_MAKE(229, 165, 10), // yellow (#e5a50a)
	LV_COLOR_MAKE(46, 194, 126), // green (#2ec27e)
	LV_COLOR_MAKE(145, 65, 172), // purple (#9141ac)
	LV_COLOR_MAKE(213, 97, 153), // pink (#d56199)
};
static int accent_index = THEME_ACCENT_PACK;

static void refresh_all(void);

lv_color_t theme_accent_preset(int index) {
	if (index == THEME_ACCENT_PACK) {
		const theme_pack_t *pack = &packs[pack_index];
		return dark_selected ? pack->dark.accent : pack->light.accent;
	}
	if (index < 0 || index >= THEME_ACCENT_COUNT) {
		index = 0;
	}
	return ACCENT_COLORS[index];
}

int theme_get_accent(void) { return accent_index; }

// The chosen pack's palettes into the working copies, with the accent preset
// over them when one is chosen.
static void load_pack_palettes(void) {
	palette_dark = packs[pack_index].dark;
	palette_light = packs[pack_index].light;
	if (accent_index >= 0 && accent_index < THEME_ACCENT_COUNT) {
		palette_dark.accent = ACCENT_COLORS[accent_index];
		palette_light.accent = ACCENT_COLORS[accent_index];
	}
}

// Which of the boot logos goes with the accent: the preset's own, or the blue
// one when the pack's accent is in force.
static int logo_accent(void) { return accent_index >= 0 ? accent_index : 0; }

// ---------------------------------------------------------------------------
// the packs
// ---------------------------------------------------------------------------

static int pack_by_name(const char *name) {
	for (int i = 0; i < pack_count; i++) {
		if (strcmp(packs[i].name, name) == 0) {
			return i;
		}
	}
	return -1;
}

// "#rrggbb" or "rrggbb" into a colour; false and untouched otherwise.
static bool parse_colour(const char *text, lv_color_t *out) {
	if (*text == '#') {
		text++;
	}
	if (strlen(text) != 6) {
		return false;
	}
	char *end;
	unsigned long v = strtoul(text, &end, 16);
	if (*end != '\0') {
		return false;
	}
	*out = lv_color_make((uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
	return true;
}

static void trim(char *s) {
	char *start = s;
	while (*start == ' ' || *start == '\t') {
		start++;
	}
	size_t len = strlen(start);
	while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\r' ||
					   start[len - 1] == '\n')) {
		start[--len] = '\0';
	}
	memmove(s, start, len + 1);
}

static bool set_palette_key(theme_palette_t *p, const char *key, const char *value) {
	lv_color_t *slot = NULL;
	if (strcmp(key, "screen_bg") == 0) {
		slot = &p->screen_bg;
	} else if (strcmp(key, "surface") == 0) {
		slot = &p->surface;
	} else if (strcmp(key, "surface_pressed") == 0) {
		slot = &p->surface_pressed;
	} else if (strcmp(key, "panel") == 0) {
		slot = &p->panel;
	} else if (strcmp(key, "cover_bg") == 0) {
		slot = &p->cover_bg;
	} else if (strcmp(key, "text_primary") == 0) {
		slot = &p->text_primary;
	} else if (strcmp(key, "text_secondary") == 0) {
		slot = &p->text_secondary;
	} else if (strcmp(key, "accent") == 0) {
		slot = &p->accent;
	}
	return slot && parse_colour(value, slot);
}

// One card pack from its file. Starts from the classic colours, so a section
// or a key left out keeps a usable colour. false when the file does not open
// or names no colour at all.
static bool read_pack_file(const char *path, const char *stem, theme_pack_t *out) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	snprintf(out->name, sizeof(out->name), "%s", stem);
	snprintf(out->display, sizeof(out->display), "%s", stem);
	out->dark = BUILTIN_PACKS[0].dark;
	out->light = BUILTIN_PACKS[0].light;

	int colours = 0;
	char section[16] = "";
	char line[160];
	while (fgets(line, sizeof(line), f)) {
		trim(line);
		if (line[0] == '\0' || line[0] == ';' || line[0] == '#') {
			continue;
		}
		if (line[0] == '[') {
			char *close = strchr(line, ']');
			if (close) {
				*close = '\0';
				snprintf(section, sizeof(section), "%s", line + 1);
			}
			continue;
		}
		char *eq = strchr(line, '=');
		if (!eq) {
			continue;
		}
		*eq = '\0';
		char *key = line;
		char *value = eq + 1;
		trim(key);
		trim(value);
		if (strcmp(section, "theme") == 0) {
			if (strcmp(key, "name") == 0 && value[0]) {
				snprintf(out->display, sizeof(out->display), "%s", value);
			}
		} else if (strcmp(section, "dark") == 0) {
			colours += set_palette_key(&out->dark, key, value);
		} else if (strcmp(section, "light") == 0) {
			colours += set_palette_key(&out->light, key, value);
		}
	}
	fclose(f);
	out->dark.dark = true;
	out->light.dark = false;
	return colours > 0;
}

static int compare_packs(const void *a, const void *b) {
	return strcasecmp(((const theme_pack_t *)a)->display, ((const theme_pack_t *)b)->display);
}

static void packs_reset(void) {
	pack_count = 0;
	for (int i = 0; i < BUILTIN_PACK_COUNT && pack_count < THEME_PACK_MAX; i++) {
		packs[pack_count++] = BUILTIN_PACKS[i];
	}
}

int theme_pack_count(void) { return pack_count; }

const theme_pack_t *theme_pack(int index) {
	return index >= 0 && index < pack_count ? &packs[index] : &packs[0];
}

int theme_get_pack(void) { return pack_index; }

// Callers asking to be called back after a theme or accent change. The list
// grows on demand and has no fixed limit: a dropped registration shows up only
// as a page that keeps the colours it was built with, which is invisible until
// somebody changes the accent. A failed allocation is logged for the same
// reason.
static theme_refresh_cb_t *refresh_cbs;
static int refresh_cb_count;
static int refresh_cb_capacity;

// How much of the accent's own saturation a neutral borrows. Dark surfaces
// take more before the hue stops reading as a shade and starts reading as a
// colour; the light ones are nearly white, where the same amount would turn
// the page pastel.
#define TINT_STRENGTH_DARK 0.30f
#define TINT_STRENGTH_LIGHT 0.15f

// Gives a neutral the accent's hue while keeping exactly how light it is: the
// brightest channel of the result is the brightest channel of the input. The
// colour is never pulled towards the accent's own brightness, which is what
// would turn a dark background light under a pale accent; the two channels
// below the brightest do come down, so a tinted surface is fractionally darker
// than the plain one, and that is what the two strengths above are set from.
static lv_color_t tint_with(lv_color_t c, lv_color_t accent, float strength) {
	uint8_t amax = accent.red > accent.green ? accent.red : accent.green;
	if (accent.blue > amax) {
		amax = accent.blue;
	}
	uint8_t amin = accent.red < accent.green ? accent.red : accent.green;
	if (accent.blue < amin) {
		amin = accent.blue;
	}
	uint8_t cmax = c.red > c.green ? c.red : c.green;
	if (c.blue > cmax) {
		cmax = c.blue;
	}
	if (amax == amin || cmax == 0) {
		return c; // a grey accent has no hue to lend, and black takes none
	}

	// The accent's shape: 1 on its brightest channel, 0 on its darkest. Applied
	// to the neutral's own brightness it reproduces that hue at the neutral's
	// level, at a fraction of the accent's saturation.
	float span = (float)(amax - amin);
	float sat = span / (float)amax * strength;
	float shape[3] = {
		((float)accent.red - (float)amin) / span,
		((float)accent.green - (float)amin) / span,
		((float)accent.blue - (float)amin) / span,
	};

	uint8_t out[3];
	for (int i = 0; i < 3; i++) {
		out[i] = (uint8_t)((float)cmax * (1.0f - sat * (1.0f - shape[i])) + 0.5f);
	}
	return lv_color_make(out[0], out[1], out[2]);
}

// Points `active` at the chosen preset, or at a tinted copy of it.
static void rebuild_active(void) {
	theme_palette_t *base = dark_selected ? &palette_dark : &palette_light;

	if (!dynamic_tint) {
		active = base;
		return;
	}

	float strength = base->dark ? TINT_STRENGTH_DARK : TINT_STRENGTH_LIGHT;
	tinted = *base;
	tinted.screen_bg = tint_with(base->screen_bg, base->accent, strength);
	tinted.surface = tint_with(base->surface, base->accent, strength);
	tinted.surface_pressed = tint_with(base->surface_pressed, base->accent, strength);
	tinted.panel = tint_with(base->panel, base->accent, strength);
	tinted.cover_bg = tint_with(base->cover_bg, base->accent, strength);
	// Text keeps the plain colours: the monochrome icons are recoloured with
	// text_primary, and they are meant to come through the tint untouched.
	active = &tinted;
}

// Pushes the active palette into the shared styles.
static void apply_palette(void) {
	lv_style_set_bg_color(&theme_style_screen, active->screen_bg);

	lv_style_set_bg_color(&theme_style_panel, active->panel);

	lv_style_set_bg_color(&theme_style_card, active->surface);
	lv_style_set_text_color(&theme_style_card, active->text_primary);

	lv_style_set_bg_color(&theme_style_card_pressed, active->surface_pressed);

	lv_style_set_text_color(&theme_style_text, active->text_primary);
	lv_style_set_text_color(&theme_style_text_dim, active->text_secondary);

	lv_style_set_image_recolor(&theme_style_icon, active->text_primary);

	lv_style_set_bg_color(&theme_style_switch, active->surface_pressed);
	lv_style_set_bg_color(&theme_style_switch_checked, active->accent);

	lv_style_set_bg_color(&theme_style_accent_bg, active->accent);
	lv_style_set_bg_color(&theme_style_slider_track, active->text_secondary);
}

void theme_init(void) {
	lv_style_init(&theme_style_screen);
	lv_style_set_bg_opa(&theme_style_screen, LV_OPA_COVER);

	lv_style_init(&theme_style_panel);
	lv_style_set_bg_opa(&theme_style_panel, LV_OPA_COVER);

	lv_style_init(&theme_style_card);
	lv_style_set_bg_opa(&theme_style_card, LV_OPA_COVER);

	lv_style_init(&theme_style_card_pressed);
	lv_style_set_bg_opa(&theme_style_card_pressed, LV_OPA_COVER);
	// The default theme grows a pressed button by a few pixels; on a list of
	// cards that reads as the row jumping out of the list, so pin it flat.
	lv_style_set_transform_width(&theme_style_card_pressed, 0);
	lv_style_set_transform_height(&theme_style_card_pressed, 0);

	lv_style_init(&theme_style_text);
	lv_style_init(&theme_style_text_dim);

	lv_style_init(&theme_style_icon);
	lv_style_set_image_recolor_opa(&theme_style_icon, LV_OPA_COVER);

	lv_style_init(&theme_style_switch);
	lv_style_set_bg_opa(&theme_style_switch, LV_OPA_COVER);
	lv_style_init(&theme_style_switch_checked);
	lv_style_set_bg_opa(&theme_style_switch_checked, LV_OPA_COVER);

	lv_style_init(&theme_style_accent_bg);
	lv_style_set_bg_opa(&theme_style_accent_bg, LV_OPA_COVER);
	lv_style_init(&theme_style_slider_track);
	lv_style_set_bg_opa(&theme_style_slider_track, LV_OPA_40);
	lv_style_set_radius(&theme_style_slider_track, LV_RADIUS_CIRCLE);

	// Whatever was chosen last time. Dark is the default for a first boot.
	dark_selected = config_get_bool("ui", "dark_theme", true);

	// The pack as it was left, among the built-in ones for now: the card's
	// are read once it is mounted (theme_packs_scan), and the choice is kept
	// by name until then.
	packs_reset();
	pack_index = pack_by_name(config_get("ui", "theme_pack", DEFAULT_PACK));
	if (pack_index < 0) {
		pack_index = pack_by_name(DEFAULT_PACK);
	}

	// The accent as it was left: a preset, or the pack's own.
	accent_index = (int)config_get_int("ui", "accent", THEME_ACCENT_PACK);
	if (accent_index < THEME_ACCENT_PACK || accent_index >= THEME_ACCENT_COUNT) {
		accent_index = THEME_ACCENT_PACK;
	}
	load_pack_palettes();

	dynamic_tint = config_get_bool("ui", "dynamic_tint", false);

	rebuild_active();
	apply_palette();

	// The marker on the flash may be missing or older than these settings.
	bootlogo_follow(active->dark, logo_accent());
}

void theme_set_pack(int index) {
	if (index < 0 || index >= pack_count) {
		return;
	}
	pack_index = index;
	load_pack_palettes();
	rebuild_active();
	apply_palette();

	config_set("ui", "theme_pack", packs[index].name);
	config_save();

	bootlogo_follow(active->dark, logo_accent());
	refresh_all();
}

void theme_packs_scan(const char *card_root) {
	char chosen[sizeof(packs[0].name)];
	snprintf(chosen, sizeof(chosen), "%s", packs[pack_index].name);

	packs_reset();
	int first_card = pack_count;
	char dir[512];
	DIR *d = NULL;
	if (card_root && card_root[0] && (size_t)snprintf(dir, sizeof(dir), "%s/" THEME_PACKS_FOLDER, card_root) < sizeof(dir)) {
		d = opendir(dir);
	}
	if (d) {
		struct dirent *e;
		while (pack_count < THEME_PACK_MAX && (e = readdir(d)) != NULL) {
			size_t len = strlen(e->d_name);
			if (e->d_name[0] == '.' || len < 5 || strcasecmp(e->d_name + len - 4, ".ini") != 0 ||
				len - 4 >= sizeof(packs[0].name)) {
				continue;
			}
			char stem[sizeof(packs[0].name)];
			snprintf(stem, sizeof(stem), "%.*s", (int)(len - 4), e->d_name);
			if (pack_by_name(stem) >= 0) {
				continue; // a card pack cannot shadow a built-in one
			}
			char path[768];
			snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
			if (read_pack_file(path, stem, &packs[pack_count])) {
				pack_count++;
			}
		}
		closedir(d);
		qsort(&packs[first_card], (size_t)(pack_count - first_card), sizeof(packs[0]), compare_packs);
	}

	// The same pack as before, by name: the one saved in the config when the
	// card was not there at startup, else the one in force, else the default.
	int wanted = pack_by_name(config_get("ui", "theme_pack", chosen));
	if (wanted < 0) {
		wanted = pack_by_name(chosen);
	}
	if (wanted < 0) {
		wanted = pack_by_name(DEFAULT_PACK);
	}
	bool changed = wanted != pack_index || !packs[wanted].builtin;
	pack_index = wanted;
	if (changed) {
		// A card pack's colours may have been edited since it was last read.
		load_pack_palettes();
		rebuild_active();
		apply_palette();
		bootlogo_follow(active->dark, logo_accent());
		refresh_all();
	}
}

void theme_apply_slider_knob(lv_obj_t *slider) {
	if (!slider) {
		return;
	}
	bool dark = theme()->dark;
	lv_obj_set_style_border_color(slider, dark ? lv_color_black() : lv_color_make(120, 120, 120), LV_PART_KNOB);
	lv_obj_set_style_border_opa(slider, dark ? LV_OPA_10 : LV_OPA_COVER, LV_PART_KNOB);
	lv_obj_set_style_border_width(slider, dark ? 1 : 2, LV_PART_KNOB);
}

void theme_register_refresh(theme_refresh_cb_t cb) {
	if (!cb) {
		return;
	}

	if (refresh_cb_count == refresh_cb_capacity) {
		int wanted = refresh_cb_capacity ? refresh_cb_capacity * 2 : 16;
		theme_refresh_cb_t *grown = realloc(refresh_cbs, (size_t)wanted * sizeof(*grown));
		if (!grown) {
			// A device that cannot find a few dozen bytes has bigger problems,
			// but a registration must never be lost unnoticed.
			fprintf(stderr, "theme: out of memory for refresh callback number %d\n",
					refresh_cb_count + 1);
			return;
		}
		refresh_cbs = grown;
		refresh_cb_capacity = wanted;
	}

	refresh_cbs[refresh_cb_count++] = cb;
}

const theme_palette_t *theme(void) { return active; }

bool theme_is_dark(void) { return active->dark; }

void theme_set_dark(bool dark) {
	if (active->dark == dark) {
		return;
	}
	theme_toggle();
}

// Which palette the pages on screen were last walked for.
//
// lv_obj_report_style_change(NULL) walks every object of every screen and
// refreshes each one against its whole subtree. The pages here are built at
// startup and stay built, so that is the entire interface -- thousands of
// objects -- for a change that moves nothing but colours, and it takes far
// longer than a frame: the finger is still on the switch when it starts.
//
// A colour is read out of the style at draw time, so the page in front of the
// user is the only one that has to be walked at the moment of the change. The
// pages behind it are walked the first time they are shown again, which is
// where a redraw was going to happen anyway.
static unsigned palette_generation = 1;

static struct screen_generation {
	lv_obj_t *screen;
	unsigned generation;
} *screen_gens;
static int screen_gen_count;
static int screen_gen_capacity;

// True when `screen` still carries an older palette than the one in force, and
// records it as walked for this one. A screen this has never seen is taken as
// current: it was built from the styles as they stand.
//
// An allocation that fails costs a walk per page change rather than a page
// with the wrong colours, which is why the failure is silent here.
static bool screen_is_stale(lv_obj_t *screen) {
	for (int i = 0; i < screen_gen_count; i++) {
		if (screen_gens[i].screen != screen) {
			continue;
		}
		if (screen_gens[i].generation == palette_generation) {
			return false;
		}
		screen_gens[i].generation = palette_generation;
		return true;
	}

	if (screen_gen_count == screen_gen_capacity) {
		int wanted = screen_gen_capacity ? screen_gen_capacity * 2 : 32;
		struct screen_generation *grown = realloc(screen_gens, (size_t)wanted * sizeof(*grown));
		if (!grown) {
			return true;
		}
		screen_gens = grown;
		screen_gen_capacity = wanted;
	}

	screen_gens[screen_gen_count].screen = screen;
	screen_gens[screen_gen_count].generation = palette_generation;
	screen_gen_count++;
	return false;
}

// One tree, refreshed against the styles it carries. LV_STYLE_PROP_ANY takes
// the object's own properties and recurses into its children.
static void refresh_tree(lv_obj_t *obj) {
	if (obj) {
		lv_obj_refresh_style(obj, LV_PART_ANY, LV_STYLE_PROP_ANY);
	}
}

void theme_notify_screen_shown(lv_obj_t *screen) {
	if (screen && screen_is_stale(screen)) {
		refresh_tree(screen);
	}
}

// Repaints after a palette change: the page on screen and the three layers the
// status bar, the player sheet and the popups live on, then the callbacks,
// which catch what cannot be expressed as a shared style -- an icon recoloured
// at runtime, the blurred artwork behind the controls. Those run for every
// page, on screen or not, and cost about as much as one frame in total.
static void refresh_all(void) {
	palette_generation++;

	lv_obj_t *screen = lv_screen_active();
	screen_is_stale(screen);
	refresh_tree(screen);
	refresh_tree(lv_layer_top());
	refresh_tree(lv_layer_sys());
	refresh_tree(lv_layer_bottom());

	for (int i = 0; i < refresh_cb_count; i++) {
		refresh_cbs[i]();
	}
}

void theme_toggle(void) {
	dark_selected = !dark_selected;
	rebuild_active();
	apply_palette();

	config_set_bool("ui", "dark_theme", active->dark);
	config_save();

	// The boot logo follows the theme and the accent; see bootlogo.h.
	bootlogo_follow(active->dark, logo_accent());

	refresh_all();
}

void theme_set_accent(int index) {
	if (index < THEME_ACCENT_PACK || index >= THEME_ACCENT_COUNT) {
		index = THEME_ACCENT_PACK;
	}
	accent_index = index;
	load_pack_palettes();
	rebuild_active(); // the tint is drawn from the accent, so it moves with it
	apply_palette();

	config_set_int("ui", "accent", index);
	config_save();

	bootlogo_follow(active->dark, logo_accent());

	refresh_all();
}

bool theme_dynamic_tint(void) { return dynamic_tint; }

void theme_set_dynamic_tint(bool on) {
	if (dynamic_tint == on) {
		return;
	}
	dynamic_tint = on;
	rebuild_active();
	apply_palette();

	config_set_bool("ui", "dynamic_tint", on);
	config_save();

	refresh_all();
}
