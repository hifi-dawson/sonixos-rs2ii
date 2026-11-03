#ifndef HOMETILES_H
#define HOMETILES_H

#include <stdbool.h>

#include "src/gui/shell/gridpage.h"

// ---------------------------------------------------------------------------
// Which tiles are on the home screen, and which under More
//
// The main menu has six places: up to HOMETILES_HOME_SLOTS tiles the user
// chose, in the user's order, and the More tile last, which opens a page
// holding every other tile. The two lists are saved under [home] as
// comma-separated tile names, so a tile that this build knows and a saved
// layout does not (a new one) turns up under More rather than nowhere.
//
// Only the tiles the model has are offered: Streaming and Wireless need a
// radio, Darwin its board, Podcasts the card's folder (on a player without a
// radio, where Streaming is not there to hold them).
// ---------------------------------------------------------------------------

typedef enum {
	HOME_TILE_MUSIC,
	HOME_TILE_STREAMING,
	HOME_TILE_WIRELESS,
	HOME_TILE_DARWIN,
	HOME_TILE_PODCASTS,
	HOME_TILE_AUDIOBOOKS,
	HOME_TILE_DAC,
	HOME_TILE_BOOKS,
	HOME_TILE_FILES,
	HOME_TILE_GEARBOY,
	HOME_TILE_FLAPPY_BIRD,
	HOME_TILE_METRONOME,
	HOME_TILE_SETTINGS,
	HOME_TILE_COUNT,
} home_tile_t;

#define HOMETILES_HOME_SLOTS 5

// Reads the saved layout, or lays the model's default out. Before
// main_menu_init() and morepage_init(), which build from it.
void hometiles_init(void);

// The two lists, the way the editing page shows them.
int hometiles_home_count(void);
home_tile_t hometiles_home_at(int position);
int hometiles_more_count(void);
home_tile_t hometiles_more_at(int position);

// Moving a tile to a position in either list saves the layout and tells the
// pages. A tile past the last home place pushes the last one out, to the top
// of More.
void hometiles_move_home(home_tile_t tile, int position);
void hometiles_move_more(home_tile_t tile, int position);

// The tile's name (a translation tag) and picture.
const char *hometiles_tag(home_tile_t tile);
const lv_image_dsc_t *hometiles_icon(home_tile_t tile);

// The grid entry for a tile: its label, picture and where it goes.
grid_entry_t hometiles_entry(home_tile_t tile);

// Called on the GUI thread after every move: the main menu and the More page
// lay their tiles out again. Up to two listeners.
void hometiles_on_change(void (*cb)(void));

#endif /* HOMETILES_H */
