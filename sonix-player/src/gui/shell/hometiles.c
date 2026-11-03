#include "hometiles.h"

#include <stdio.h>
#include <string.h>

#include "src/gui/audio/dacpage.h"
#include "src/gui/audio/metronomepage.h"
#include "src/gui/ebook/ebookpage.h"
#include "src/gui/flappybird/flappybird.h"
#include "src/gui/gearboy/gearboypage.h"
#include "src/gui/library/audiobooks.h"
#include "src/gui/library/filespage.h"
#include "src/gui/library/music.h"
#include "src/gui/settings/darwinpage.h"
#include "src/gui/settings/settings.h"
#include "src/gui/shell/icons.h"
#include "src/gui/streaming/podcastpage.h"
#include "src/gui/streaming/streaming.h"
#include "src/gui/wireless/wireless.h"
#include "src/system/core/config.h"
#include "src/system/device/sysinfo.h"

// Music and Audiobooks both open with the card out or at a computer, and say
// so on their own page. Books and the file explorer are actions rather than
// screens: the shelf reads the Ebook folder again every time it is opened,
// and the listing is read when the page is entered, so what changed on the
// card turns up without a restart.
typedef struct {
	const char *name; // in the config file
	const char *tag;  // the caption, a translation tag
	const lv_image_dsc_t *icon;
	lv_obj_t **target;
	void (*action)(void);
} tile_t;

static const tile_t TILES[HOME_TILE_COUNT] = {
	[HOME_TILE_MUSIC] = {"music", "music", &icon_menu_music, &music_screen, NULL},
	[HOME_TILE_STREAMING] = {"streaming", "streaming", &icon_menu_streaming, &streaming_screen, NULL},
	[HOME_TILE_WIRELESS] = {"wireless", "wireless", &icon_menu_wireless, &wireless_screen, NULL},
	[HOME_TILE_DARWIN] = {"darwin", "darwin", &icon_menu_darwin, &darwin_screen, NULL},
	[HOME_TILE_PODCASTS] = {"podcasts", "podcasts", &icon_menu_podcast, &podcast_screen, NULL},
	[HOME_TILE_AUDIOBOOKS] = {"audiobooks", "audiobooks", &icon_menu_audiobooks, &audiobooks_screen, NULL},
	[HOME_TILE_DAC] = {"dac", "dac", &icon_menu_dac, &dacpage_screen, NULL},
	[HOME_TILE_BOOKS] = {"books", "books", &icon_menu_books, NULL, ebookpage_open},
	[HOME_TILE_FILES] = {"files", "file_explorer", &icon_menu_file_explorer, NULL, filespage_open},
	[HOME_TILE_GEARBOY] = {"gearboy", "gearboy", &icon_menu_gearboy, &gearboypage_screen, NULL},
	// An action: the artwork is read from the resource tree on the way in.
	[HOME_TILE_FLAPPY_BIRD] = {"flappy_bird", "flappy_bird", &icon_menu_flappy_bird, NULL, flappybird_open},
	[HOME_TILE_METRONOME] = {"metronome", "metronome", &icon_menu_metronome, &metronomepage_screen, NULL},
	[HOME_TILE_SETTINGS] = {"settings", "settings", &icon_menu_settings, &settings_screen, NULL},
};

// What the main menu and the More page showed before the layout could be
// changed, which is still what a player with no saved layout shows.
static const home_tile_t DEFAULT_HOME[] = {
	HOME_TILE_MUSIC,	  HOME_TILE_STREAMING,	HOME_TILE_WIRELESS, HOME_TILE_DARWIN,
	HOME_TILE_PODCASTS, HOME_TILE_AUDIOBOOKS, HOME_TILE_SETTINGS,
};
static const home_tile_t DEFAULT_MORE[] = {
	HOME_TILE_DAC, HOME_TILE_GEARBOY, HOME_TILE_BOOKS, HOME_TILE_FILES, HOME_TILE_FLAPPY_BIRD, HOME_TILE_METRONOME,
};

static bool available[HOME_TILE_COUNT];
static home_tile_t home[HOME_TILE_COUNT];
static int home_n;
static home_tile_t more[HOME_TILE_COUNT];
static int more_n;

static void (*listeners[2])(void);

static bool listed(const home_tile_t *list, int n, home_tile_t tile) {
	for (int i = 0; i < n; i++) {
		if (list[i] == tile) {
			return true;
		}
	}
	return false;
}

static home_tile_t tile_by_name(const char *name) {
	for (int i = 0; i < HOME_TILE_COUNT; i++) {
		if (strcmp(TILES[i].name, name) == 0) {
			return (home_tile_t)i;
		}
	}
	return HOME_TILE_COUNT;
}

// "music,darwin,podcasts" -> the list, skipping names this build does not
// know, tiles the model has not got, and repeats.
static void parse(const char *text, home_tile_t *list, int *n, int limit) {
	*n = 0;
	char copy[256];
	snprintf(copy, sizeof(copy), "%s", text ? text : "");
	for (char *name = strtok(copy, ", "); name; name = strtok(NULL, ", ")) {
		home_tile_t tile = tile_by_name(name);
		if (tile == HOME_TILE_COUNT || !available[tile] || listed(list, *n, tile) || *n >= limit) {
			continue;
		}
		list[(*n)++] = tile;
	}
}

static void format(const home_tile_t *list, int n, char *out, size_t size) {
	out[0] = '\0';
	for (int i = 0; i < n; i++) {
		size_t len = strlen(out);
		snprintf(out + len, size - len, "%s%s", i ? "," : "", TILES[list[i]].name);
	}
}

static void save(void) {
	char text[256];
	format(home, home_n, text, sizeof(text));
	config_set("home", "tiles", text);
	format(more, more_n, text, sizeof(text));
	config_set("home", "more", text);
	config_save();
}

static void notify(void) {
	for (size_t i = 0; i < sizeof(listeners) / sizeof(listeners[0]); i++) {
		if (listeners[i]) {
			listeners[i]();
		}
	}
}

void hometiles_init(void) {
	const sysinfo_model_t *model = sysinfo_model();
	bool wireless = !(model && model->no_wireless);
	for (int i = 0; i < HOME_TILE_COUNT; i++) {
		available[i] = true;
	}
	available[HOME_TILE_STREAMING] = wireless;
	available[HOME_TILE_WIRELESS] = wireless;
	available[HOME_TILE_DARWIN] = model && model->darwin;
	available[HOME_TILE_PODCASTS] = !wireless;
#if !GB_CORE
	available[HOME_TILE_GEARBOY] = false; // the emulator is only there when the binary was built with it
#endif

	const char *saved_home = config_get("home", "tiles", NULL);
	const char *saved_more = config_get("home", "more", NULL);
	if (saved_home || saved_more) {
		parse(saved_home, home, &home_n, HOMETILES_HOME_SLOTS);
		parse(saved_more, more, &more_n, HOME_TILE_COUNT);
	} else {
		home_n = 0;
		for (size_t i = 0; i < sizeof(DEFAULT_HOME) / sizeof(DEFAULT_HOME[0]); i++) {
			if (available[DEFAULT_HOME[i]] && home_n < HOMETILES_HOME_SLOTS) {
				home[home_n++] = DEFAULT_HOME[i];
			}
		}
		more_n = 0;
		for (size_t i = 0; i < sizeof(DEFAULT_MORE) / sizeof(DEFAULT_MORE[0]); i++) {
			if (available[DEFAULT_MORE[i]]) {
				more[more_n++] = DEFAULT_MORE[i];
			}
		}
	}

	// Whatever is left -- a tile a saved layout does not name, a default that
	// fell off a full home screen -- goes under More, so every tile is
	// somewhere.
	for (int i = 0; i < HOME_TILE_COUNT; i++) {
		if (available[i] && !listed(home, home_n, (home_tile_t)i) && !listed(more, more_n, (home_tile_t)i)) {
			more[more_n++] = (home_tile_t)i;
		}
	}
}

int hometiles_home_count(void) { return home_n; }
home_tile_t hometiles_home_at(int position) { return position >= 0 && position < home_n ? home[position] : HOME_TILE_COUNT; }
int hometiles_more_count(void) { return more_n; }
home_tile_t hometiles_more_at(int position) { return position >= 0 && position < more_n ? more[position] : HOME_TILE_COUNT; }

static void remove_from(home_tile_t *list, int *n, home_tile_t tile) {
	for (int i = 0; i < *n; i++) {
		if (list[i] == tile) {
			memmove(&list[i], &list[i + 1], (size_t)(*n - i - 1) * sizeof(list[0]));
			(*n)--;
			return;
		}
	}
}

static void insert_at(home_tile_t *list, int *n, home_tile_t tile, int position) {
	if (position < 0) {
		position = 0;
	}
	if (position > *n) {
		position = *n;
	}
	memmove(&list[position + 1], &list[position], (size_t)(*n - position) * sizeof(list[0]));
	list[position] = tile;
	(*n)++;
}

void hometiles_move_home(home_tile_t tile, int position) {
	if (tile < 0 || tile >= HOME_TILE_COUNT || !available[tile]) {
		return;
	}
	remove_from(home, &home_n, tile);
	remove_from(more, &more_n, tile);
	insert_at(home, &home_n, tile, position);
	// One too many: the last one goes to the top of More.
	while (home_n > HOMETILES_HOME_SLOTS) {
		home_tile_t out = home[home_n - 1];
		home_n--;
		insert_at(more, &more_n, out, 0);
	}
	save();
	notify();
}

void hometiles_move_more(home_tile_t tile, int position) {
	if (tile < 0 || tile >= HOME_TILE_COUNT || !available[tile]) {
		return;
	}
	remove_from(home, &home_n, tile);
	remove_from(more, &more_n, tile);
	insert_at(more, &more_n, tile, position);
	save();
	notify();
}

const char *hometiles_tag(home_tile_t tile) { return tile >= 0 && tile < HOME_TILE_COUNT ? TILES[tile].tag : ""; }

const lv_image_dsc_t *hometiles_icon(home_tile_t tile) {
	return tile >= 0 && tile < HOME_TILE_COUNT ? TILES[tile].icon : NULL;
}

grid_entry_t hometiles_entry(home_tile_t tile) {
	grid_entry_t entry = {"", NULL, NULL, NULL};
	if (tile >= 0 && tile < HOME_TILE_COUNT) {
		entry.label = TILES[tile].tag;
		entry.icon = TILES[tile].icon;
		entry.target = TILES[tile].target;
		entry.action = TILES[tile].action;
	}
	return entry;
}

void hometiles_on_change(void (*cb)(void)) {
	for (size_t i = 0; i < sizeof(listeners) / sizeof(listeners[0]); i++) {
		if (!listeners[i]) {
			listeners[i] = cb;
			return;
		}
	}
}
