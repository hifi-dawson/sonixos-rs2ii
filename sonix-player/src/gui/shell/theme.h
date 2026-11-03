#ifndef THEME_H
#define THEME_H

#include <stdbool.h>

#include "lvgl/lvgl.h"

// The handful of colours the whole UI is built from. Two presets (dark and
// light) live in theme.c; everything on screen pulls from the active one.
typedef struct {
	lv_color_t screen_bg;	   // page background
	lv_color_t surface;		   // list rows, cards
	lv_color_t surface_pressed; // the same, while a finger is on it
	lv_color_t panel;		   // status bar and the player's control block
	lv_color_t cover_bg;	   // behind the artwork, when a track has none
	lv_color_t text_primary;
	lv_color_t text_secondary;
	lv_color_t accent;		   // the blue used for active things
	bool dark;
} theme_palette_t;

// Shared styles. Widgets add these instead of setting colours themselves,
// which is what makes switching themes a one-liner: update the style, LVGL
// repaints everything using it.
extern lv_style_t theme_style_screen;
extern lv_style_t theme_style_panel;
extern lv_style_t theme_style_card;
extern lv_style_t theme_style_card_pressed;
extern lv_style_t theme_style_text;
extern lv_style_t theme_style_text_dim;
extern lv_style_t theme_style_icon;
extern lv_style_t theme_style_switch;
extern lv_style_t theme_style_switch_checked; // image_recolor for monochrome icons

// A plain accent-coloured fill. Anything that has to follow the accent colour
// without the page repainting it adds this instead of setting the colour by
// hand: the sliders' filled part, the pill buttons. A hand-set colour is a
// snapshot -- it keeps whichever accent was live when the page was built, and
// does not follow a later change.
extern lv_style_t theme_style_accent_bg;

// The unfilled part of a slider's track: the secondary text colour at low
// opacity, which reads as a quiet groove on both palettes.
extern lv_style_t theme_style_slider_track;

// Anything that can't be expressed as a style -- an icon recoloured at runtime,
// the blurred artwork behind the controls -- registers a callback here and gets
// called after every switch.
typedef void (*theme_refresh_cb_t)(void);
void theme_register_refresh(theme_refresh_cb_t cb);

// The ring that tells a slider's knob from what is behind it. In the dark
// theme a hairline of black is plenty; in the light one the cards are nearly
// white too, so a white knob needs a real grey outline not to vanish into
// them. Call it when the slider is built and from the page's theme-refresh
// callback -- the colour depends on the theme in force.
void theme_apply_slider_knob(lv_obj_t *slider);

// Builds the shared styles. Call once, before any screen is created.
void theme_init(void);

// Call with the page about to be put on screen. A theme, accent or tint change
// walks only the page the user is looking at; this is what walks a page that
// was hidden when the palette moved, and does nothing when it was not.
void theme_notify_screen_shown(lv_obj_t *screen);

const theme_palette_t *theme(void);
bool theme_is_dark(void);

// Switches between the two presets and repaints the UI.
void theme_toggle(void);

// The accent colour, chosen from a fixed set of presets (Appearance > Accent
// colour): 0 blue, 1 orange, 2 yellow, 3 green, 4 purple, 5 pink -- or
// THEME_ACCENT_PACK, the theme pack's own accent, which is the default.
// Applies to both dark and light palettes, persists in the config.
#define THEME_ACCENT_COUNT 6
#define THEME_ACCENT_PACK (-1)
lv_color_t theme_accent_preset(int index); // THEME_ACCENT_PACK gives the pack's
int theme_get_accent(void);
void theme_set_accent(int index);

// ---------------------------------------------------------------------------
// Theme packs
//
// A pack is a dark palette and a light palette with a name. Two are built in:
// "classic", GNOME Adwaita's colours as upstream Sonix has them, and "midnight",
// a blue one. More come from the card, one file each in
// <card>/Themes/<name>.ini (assets/themes/midnight.ini is the example):
//
//     [theme]
//     name = Desert Night
//     [dark]
//     screen_bg = #111820
//     surface = #1b2531
//     surface_pressed = #273544
//     panel = #0d1218
//     cover_bg = #0d1218
//     text_primary = #eef2f7
//     text_secondary = #9fb0c2
//     accent = #4f9cf0
//     [light]
//     ...the same keys...
//
// A section or key left out keeps the classic pack's colour. The dark or
// light switch, the accent presets and the dynamic tint all work on top of
// whichever pack is chosen. The choice persists in the config as the pack's
// name, so a card pack that is missing at boot falls back to Classic until the
// card with it is back.
// ---------------------------------------------------------------------------

#define THEME_PACK_MAX 16
#define THEME_PACKS_FOLDER "Themes"

typedef struct {
	char name[40];	  // "classic", "midnight", or the file's name without .ini
	char display[64]; // a translation tag for a built-in pack, the [theme] name for a card one
	bool builtin;
	theme_palette_t dark;
	theme_palette_t light;
} theme_pack_t;

int theme_pack_count(void);
const theme_pack_t *theme_pack(int index);
int theme_get_pack(void);
void theme_set_pack(int index);

// Reads the card's Themes folder again: the built-in packs, then the card's
// in name order. The chosen pack is kept by name; a card pack whose file
// changed is applied afresh.
void theme_packs_scan(const char *card_root);

// Picks a specific theme rather than flipping; no-op when already active.
void theme_set_dark(bool dark);

// The dynamic tint (Appearance > Dynamic tint): the neutral colours --
// backgrounds, cards, the status bar -- borrow the accent's hue, the way
// Material You colours a system from one seed. Each colour keeps its brightest
// channel, so a tinted surface stays at the level the preset put it at instead
// of drifting towards the accent's own; text and the monochrome icons are left
// alone entirely. Off by default, persists in the config.
bool theme_dynamic_tint(void);
void theme_set_dynamic_tint(bool on);

#endif // THEME_H
