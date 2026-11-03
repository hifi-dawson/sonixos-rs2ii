#include "darwin.h"

#include "src/system/audio/alsa-controls.h"
#include "src/system/core/config.h"

#include <stdio.h>
#include <unistd.h>

// The FPGA's SPI device, where sa_sound_hm200.ko puts its attributes.
#define HM200_NODE "/sys/devices/platform/jz-spi0.0/spi_master/spi0/spi0.0/"
#define FILTER_PATH_NODE HM200_NODE "filter_path"
#define OSR_BYPASS_NODE HM200_NODE "osr_bypass"

// Where the filter files are looked for, in this order: the card's own copy
// first, so that a filter can be replaced without rebuilding the firmware,
// then the stock one. The same two places and the same order as the stock
// player (routine 0x47c5a0).
#define FILTER_DIR_CARD "/data/mnt/sd_0/filter/"
#define FILTER_DIR_STOCK "/usr/resource/filter/"

// Index to file, the stock player's own table (0x7f2470). The names on the
// screen are the stock ones too, as language keys: Darwin Default, then
// Darwin 1 to 10.
static const char *const FILTER_FILES[DARWIN_FILTER_COUNT] = {
	"default.bin",
	"linear_phase_sharp_rolloff.bin",
	"linear_phase_sharp_late_rolloff.bin",
	"linear_phase_short_ring.bin",
	"linear_phase_short_ring_late_rolloff.bin",
	"minimum_phase_sharp_rolloff.bin",
	"minimum_phase_sharp_late_rolloff.bin",
	"minimum_phase_short_ring.bin",
	"minimum_phase_short_ring_late_rolloff.bin",
	"0th_order_hold_nos.bin",
	"darwin_ultra.bin",
};

static const char *const FILTER_KEYS[DARWIN_FILTER_COUNT] = {
	"darwin_filter_0", "darwin_filter_1", "darwin_filter_2", "darwin_filter_3",
	"darwin_filter_4", "darwin_filter_5", "darwin_filter_6", "darwin_filter_7",
	"darwin_filter_8", "darwin_filter_9", "darwin_filter_10",
};

static int filter_index;
static bool nos_on;
static int gain_mode;
static int dsd_gain;
static bool harmonic_on;
static int harmonic_level;

// What the playback thread reads. The factor is stored before the level, so a
// reader that sees the new level sees the factor that goes with it; between
// the two it sees the old pair, which is the level of a moment ago and no worse.
static volatile int active_level;
static volatile float active_factor = 1.0f;

// One write to an attribute, the way the stock player makes it: through
// `echo`, so with the newline, which the driver is used to.
static bool write_node(const char *node, const char *value) {
	FILE *f = fopen(node, "w");
	if (!f) {
		fprintf(stderr, "darwin: %s cannot be opened\n", node);
		return false;
	}
	bool ok = fprintf(f, "%s\n", value) >= 0;
	ok = fclose(f) == 0 && ok;
	if (!ok) {
		fprintf(stderr, "darwin: writing \"%s\" to %s failed\n", value, node);
	}
	return ok;
}

static void write_filter(void) {
	if (!alsa_board_is_darwin()) {
		return;
	}
	char path[160];
	snprintf(path, sizeof(path), FILTER_DIR_CARD "%s", FILTER_FILES[filter_index]);
	if (access(path, F_OK) != 0) {
		snprintf(path, sizeof(path), FILTER_DIR_STOCK "%s", FILTER_FILES[filter_index]);
		if (access(path, F_OK) != 0) {
			// The stock player says this much and leaves the FPGA as it was.
			fprintf(stderr, "darwin: no filter file %s on the card or in the firmware\n", FILTER_FILES[filter_index]);
			return;
		}
	}
	if (write_node(FILTER_PATH_NODE, path)) {
		fprintf(stderr, "darwin: filter %d, %s\n", filter_index, path);
	}
}

static void write_nos(void) {
	if (!alsa_board_is_darwin()) {
		return;
	}
	if (write_node(OSR_BYPASS_NODE, nos_on ? "1" : "0")) {
		fprintf(stderr, "darwin: NOS %s\n", nos_on ? "on" : "off");
	}
}

static void write_gain(void) {
	if (alsa_board_is_darwin() && alsa_set_control("Gain Mode", gain_mode) >= 0) {
		fprintf(stderr, "darwin: gain mode %d\n", gain_mode);
	}
}

static void write_dsd_gain(void) {
	if (alsa_board_is_darwin() && alsa_set_control("DSD Gain", dsd_gain) >= 0) {
		fprintf(stderr, "darwin: DSD gain %d\n", dsd_gain);
	}
}

// The stock setter (0x737760) keeps the level and (65536 - level) / 65536 as a
// float; the write routine (0x73b040) multiplies by the float. The same here,
// in the same order, so the arithmetic is the stock one to the last bit.
static void publish_harmonic(void) {
	int level = harmonic_on && alsa_board_is_darwin() ? harmonic_level : 0;
	active_factor = (float)(65536 - level) * (1.0f / 65536.0f);
	active_level = level;
}

void darwin_init(void) {
	int f = (int)config_get_int("darwin", "filter", 0);
	filter_index = f >= 0 && f < DARWIN_FILTER_COUNT ? f : 0;
	nos_on = config_get_int("darwin", "nos", 0) != 0;
	harmonic_on = config_get_int("darwin", "harmonic", 0) != 0;
	int level = (int)config_get_int("darwin", "harmonic_level", 0);
	harmonic_level = level < 0 ? 0 : (level > DARWIN_HARMONIC_MAX ? DARWIN_HARMONIC_MAX : level);
	int g = (int)config_get_int("darwin", "gain", 0);
	gain_mode = g >= 0 && g < DARWIN_GAIN_COUNT ? g : 0;
	int d = (int)config_get_int("darwin", "dsd_gain", 0);
	dsd_gain = d >= 0 && d < DARWIN_DSD_GAIN_COUNT ? d : 0;
	publish_harmonic();
	darwin_reapply();
}

int darwin_gain(void) { return gain_mode; }

void darwin_set_gain(int gain) {
	if (gain < 0 || gain >= DARWIN_GAIN_COUNT) {
		return;
	}
	gain_mode = gain;
	config_set_int("darwin", "gain", gain);
	config_save();
	write_gain();
}

int darwin_dsd_gain(void) { return dsd_gain; }

void darwin_set_dsd_gain(int gain) {
	if (gain < 0 || gain >= DARWIN_DSD_GAIN_COUNT) {
		return;
	}
	dsd_gain = gain;
	config_set_int("darwin", "dsd_gain", gain);
	config_save();
	write_dsd_gain();
}

int darwin_filter(void) { return filter_index; }

void darwin_set_filter(int index) {
	if (index < 0 || index >= DARWIN_FILTER_COUNT) {
		return;
	}
	filter_index = index;
	config_set_int("darwin", "filter", index);
	config_save();
	write_filter();
}

const char *darwin_filter_key(int index) {
	return index >= 0 && index < DARWIN_FILTER_COUNT ? FILTER_KEYS[index] : "";
}

bool darwin_nos(void) { return nos_on; }

void darwin_set_nos(bool on) {
	nos_on = on;
	config_set_int("darwin", "nos", on ? 1 : 0);
	config_save();
	write_nos();
}

bool darwin_harmonic_enabled(void) { return harmonic_on; }

void darwin_harmonic_set_enabled(bool on) {
	harmonic_on = on;
	config_set_int("darwin", "harmonic", on ? 1 : 0);
	config_save();
	publish_harmonic();
}

int darwin_harmonic_level(void) { return harmonic_level; }

void darwin_harmonic_set_level(int level) {
	if (level < 0) {
		level = 0;
	}
	if (level > DARWIN_HARMONIC_MAX) {
		level = DARWIN_HARMONIC_MAX;
	}
	harmonic_level = level;
	config_set_int("darwin", "harmonic_level", level);
	config_save();
	publish_harmonic();
}

// NOS before the filter, the order the stock player re-applies them in when a
// preset changes all three (settings case 0x68).
void darwin_reapply(void) {
	write_nos();
	write_filter();
	write_gain();
	write_dsd_gain();
}

bool darwin_harmonic_active(void) { return active_level > 0; }

void darwin_harmonic_apply_s32(int32_t *samples, int count) {
	float factor = active_factor;
	for (int i = 0; i < count; i++) {
		if (samples[i] < 0) {
			samples[i] = (int32_t)((float)samples[i] * factor);
		}
	}
}

void darwin_harmonic_apply_s16(int16_t *samples, int count) {
	float factor = active_factor;
	for (int i = 0; i < count; i++) {
		if (samples[i] < 0) {
			samples[i] = (int16_t)((float)samples[i] * factor);
		}
	}
}
