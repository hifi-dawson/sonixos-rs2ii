#ifndef DARWIN_H
#define DARWIN_H

#include <stdbool.h>
#include <stdint.h>

// Darwin, the RS2 II's converter: an R2R ladder driven by an FPGA (the HM200,
// sa_sound_hm200.ko, programmed at boot by the stock sound-card module). There
// is no DAC chip and so none of the codec's mixer controls; the stock player
// reaches the converter through two attributes of the FPGA's SPI device, and
// the third Darwin setting is not in the FPGA at all.
//
// All three are as the stock player does them (routines named in darwin.c):
//
//   digital filter   a 2 KB coefficient file written to filter_path. Eleven
//                    of them, "Darwin Default" and Darwin 1 to 10, in
//                    /usr/resource/filter; one of the same name on the card's
//                    filter/ folder wins.
//   NOS              osr_bypass, 1 to turn the oversampling off.
//   harmonics        software, on the analogue output only: every negative
//                    sample scaled by (65536 - level) / 65536. Lowering one
//                    half of the wave and not the other is what adds the even
//                    harmonics, the second above all.
//
// Everything here is a no-op off the Darwin board (alsa_board_is_darwin()).

#define DARWIN_FILTER_COUNT 11
#define DARWIN_HARMONIC_MAX 255

// Reads the saved settings and writes the filter and NOS to the FPGA. Called
// once at startup, after the sound card is up.
void darwin_init(void);

// The filters by index, 0 = Darwin Default. Saved to config.
int darwin_filter(void);
void darwin_set_filter(int index);
// The language key of a filter's name: "darwin_filter_0" and so on.
const char *darwin_filter_key(int index);

// Non-oversampling. Saved to config.
bool darwin_nos(void);
void darwin_set_nos(bool on);

// The harmonic stage: a switch and a level 0..DARWIN_HARMONIC_MAX, the way
// the stock player's Darwin menu has them. The switch keeps the level while
// off; the stage runs when it is on and the level is above 0. Saved to
// config.
bool darwin_harmonic_enabled(void);
void darwin_harmonic_set_enabled(bool on);
int darwin_harmonic_level(void);
void darwin_harmonic_set_level(int level);

// The card's "Gain Mode", 0..2. 0 is "Low gain" on the stock player (read
// from a unit); 1 and 2 are taken to be its middle and high positions.
// Saved to config.
#define DARWIN_GAIN_COUNT 3
int darwin_gain(void);
void darwin_set_gain(int gain);

// The card's "DSD Gain", 0..3. 0 is the stock player's "0 dB" (read from a
// unit); what the other three are is not known yet. Saved to config.
#define DARWIN_DSD_GAIN_COUNT 4
int darwin_dsd_gain(void);
void darwin_set_dsd_gain(int gain);

// The filter, NOS and the two gains written again, whatever was written
// last. For after the FPGA has been powered down and reloaded (a suspend).
void darwin_reapply(void);

// Whether the harmonic stage changes anything right now: the Darwin board
// and a level above 0. The playback loop asks before every buffer; it is one
// load and a compare.
bool darwin_harmonic_active(void);

// The stage itself, in place on interleaved samples. `count` is samples, not
// frames. Only for PCM on the analogue jacks: not DoP, not USB, not Bluetooth.
void darwin_harmonic_apply_s32(int32_t *samples, int count);
void darwin_harmonic_apply_s16(int16_t *samples, int count);

#endif /* DARWIN_H */
