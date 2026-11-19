#ifndef SEQ_ICONS_H
#define SEQ_ICONS_H

#include <Arduino.h>
#include "config.h"

// Bitmaps XBM (LSB-first) para U8g2::drawXBMP (armazenados em Flash)

// Clave de Sol (8x21 px)
static const uint8_t ICON_CLEF_TREBLE[] PROGMEM = {
    0x30, // ....##..
    0x48, // ...#..#.
    0x48, // ...#..#.
    0x30, // ....##..
    0x18, // ...##...
    0x0C, // ..##....
    0x16, // .##.#...
    0x12, // .#..#...
    0x11, // #...#...
    0x39, // #..###..
    0x55, // #.#.#.#.
    0x95, // #.#.#..#
    0x92, // .#..#..#
    0x3C, // ..####..
    0x10, // ....#...
    0x10, // ....#...
    0x10, // ....#...
    0x12, // .#..#...
    0x13, // ##..#...
    0x0E, // .###....
    0x00  // ........
};

// Clave de Fá (8x11 px)
static const uint8_t ICON_CLEF_BASS[] PROGMEM = {
    0x1C, // ..###...
    0x22, // .#...#..
    0xA3, // ##...#.#
    0x23, // ##...#..
    0x20, // .....#..
    0x10, // ....#...
    0x90, // ....#..#
    0x08, // ...#....
    0x04, // ..#.....
    0x02, // .#......
    0x01  // #.......
};

// Símbolo 0: Cachorro (7x7 px)
static const uint8_t ICON_SYM_DOG[] PROGMEM = {
    0x63, // ##...##
    0x7F, // #######
    0x6B, // ##.#.##
    0x3E, // .#####.
    0x14, // ..#.#..
    0x3E, // .#####.
    0x1C  // ..###..
};

// Símbolo 1: Gato (7x7 px)
static const uint8_t ICON_SYM_CAT[] PROGMEM = {
    0x41, // #.....#
    0x63, // ##...##
    0x7F, // #######
    0x6B, // ##.#.##
    0x3E, // .#####.
    0x49, // #..#..#
    0x1C  // ..###..
};

// Símbolo 2: Estrela (7x7 px)
static const uint8_t ICON_SYM_STAR[] PROGMEM = {
    0x08, // ...#...
    0x1C, // ..###..
    0x7F, // #######
    0x3E, // .#####.
    0x1C, // ..###..
    0x36, // .##.##.
    0x41  // #.....#
};

// Símbolo 3: Cogumelo (7x7 px)
static const uint8_t ICON_SYM_MUSHROOM[] PROGMEM = {
    0x1C, // ..###..
    0x2A, // .#.#.#.
    0x7F, // #######
    0x6B, // ##.#.##
    0x3E, // .#####.
    0x1C, // ..###..
    0x1C  // ..###..
};

static const uint8_t *const SEQ_SYMBOL_ICONS[SEQ_NUM_SYMBOLS] = {
    ICON_SYM_DOG,
    ICON_SYM_CAT,
    ICON_SYM_STAR,
    ICON_SYM_MUSHROOM
};

static const char *const SEQ_SYMBOL_NAMES[SEQ_NUM_SYMBOLS] = {
    "Cao",
    "Gato",
    "Estrela",
    "Cogu"
};

// Ícones da barra superior e acidentes
static const uint8_t ICON_PLAY[] PROGMEM = {
    0x01, // #....
    0x03, // ##...
    0x07, // ###..
    0x0F, // ####.
    0x07, // ###..
    0x03, // ##...
    0x01  // #....
};

static const uint8_t ICON_STOP[] PROGMEM = {
    0x1F, // #####
    0x1F, // #####
    0x1F, // #####
    0x1F, // #####
    0x1F  // #####
};

static const uint8_t ICON_GEAR[] PROGMEM = {
    0x08, // ...#...
    0x2A, // .#.#.#.
    0x1C, // ..###..
    0x6B, // ##.#.##
    0x1C, // ..###..
    0x2A, // .#.#.#.
    0x08  // ...#...
};

// Sustenido '#' (3x5 px)
static const uint8_t ICON_SHARP[] PROGMEM = {
    0x05, // #.#
    0x07, // ###
    0x05, // #.#
    0x07, // ###
    0x05  // #.#
};

#endif
