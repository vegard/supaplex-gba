/*
 * GameBoy Advance port of the Amiga/PC game Supaplex
 * Copyright (C) 2012  Vegard Nossum <vegard.nossum@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include "assert.hh"
#include "coordinate.hh"
#include "element_type.hh"
#include "graphics.cc" // XXX: fix
#include "tile.hh"

#define __iwram __attribute__ ((section(".iwram")))

#define TILE(tile) \
	{ \
		4 * TILE ## _ ## tile + 0, \
		4 * TILE ## _ ## tile + 1, \
		4 * TILE ## _ ## tile + 2, \
		4 * TILE ## _ ## tile + 3, \
	}

#define TILE_FLIP_X(tile) \
	{ \
		(4 * TILE ## _ ## tile + 1) | (1 << 10), \
		(4 * TILE ## _ ## tile + 0) | (1 << 10), \
		(4 * TILE ## _ ## tile + 3) | (1 << 10), \
		(4 * TILE ## _ ## tile + 2) | (1 << 10), \
	}

#define TILE_FLIP_Y(tile) \
	{ \
		(4 * TILE ## _ ## tile + 2) | (1 << 11), \
		(4 * TILE ## _ ## tile + 3) | (1 << 11), \
		(4 * TILE ## _ ## tile + 0) | (1 << 11), \
		(4 * TILE ## _ ## tile + 1) | (1 << 11), \
	}

/* 2 KiB lookup table for mapping the game field to BG map tiles */
/* XXX: We could compress this to use only 2 bytes (instead of 8) per
 * 16x16 tile with some extra logic in the function that updates the
 * BG map. */
/* XXX: This should go into WRAM. In other words, we should construct
 * it at run-time rather than make it const. */
static const uint16_t tiles[][4] = {
	TILE(SPACE),
	TILE(ZONK),
	TILE(BASE),
	TILE(MURPHY),
	TILE(INFOTRON),
	TILE(CHIP_SQUARE),
	TILE(WALL),
	TILE(EXIT),
	TILE(DISK_ORANGE),

	/* Regular ports */
	TILE(PORT_LEFT_TO_RIGHT),
	TILE(PORT_UP_TO_DOWN),
	TILE_FLIP_X(PORT_LEFT_TO_RIGHT),
	TILE_FLIP_Y(PORT_UP_TO_DOWN),

	/* Special ports (use the same tiles as regular ports) */
	TILE(PORT_LEFT_TO_RIGHT),
	TILE(PORT_UP_TO_DOWN),
	TILE_FLIP_X(PORT_LEFT_TO_RIGHT),
	TILE_FLIP_Y(PORT_UP_TO_DOWN),

	TILE(SNIK_SNAK),
	TILE(DISK_YELLOW),
	TILE(TERMINAL),
	TILE(DISK_RED),

	/* Two- and four-way ports */
	TILE(PORT_VERTICAL),
	TILE(PORT_HORIZONTAL),
	TILE(PORT_CROSS),

	TILE(ELECTRON),

	/* Bug */
	TILE(BASE),

	TILE(CHIP_HORIZONTAL_LEFT),
	TILE(CHIP_HORIZONTAL_RIGHT),

	TILE(HARDWARE_1),
	TILE(HARDWARE_LAMP_GREEN),
	TILE(HARDWARE_LAMP_BLUE),
	TILE(HARDWARE_LAMP_RED),
	TILE(HARDWARE_2),
	TILE(HARDWARE_3),
	TILE(HARDWARE_4),
	TILE(HARDWARE_5),
	TILE(HARDWARE_6),
	TILE(HARDWARE_7),
	TILE(CHIP_VERTICAL_TOP),
	TILE(CHIP_VERTICAL_BOTTOM),

	/* Invisible wall */
	TILE(SPACE),
};

/* We could put this in the GamePak ROM, however, the GamePak ROM has
 * horrible memory access latency compared with the internal WRAM. So
 * since this memory is used in a rather timing-sensitive operation
 * (updating the game field) we prefer to construct it at run-time. */
static void (*elements[NR_ELEMENTS])(const coordinate);


/* Game variables */

static unsigned int current_level;

static enum {
	MURPHY_FACING,
	MURPHY_MOVING,
} murphy_state;

static enum {
	MURPHY_FACING_LEFT,
	MURPHY_FACING_RIGHT,
} murphy_facing_direction;

static enum {
	MURPHY_LEFT,
	MURPHY_RIGHT,
	MURPHY_UP,
	MURPHY_DOWN,
} murphy_moving_direction;

static uint16_t murphy_frame;

/* The lower 4 bits are in pixels, while the next 6 bits give the position
 * on the game field. */
static uint16_t murphy_x;
static uint16_t murphy_y;

#include "element.hh"

static element field[60 * 24];

static void find_murphy()
{
	murphy_state = MURPHY_FACING;
	murphy_facing_direction = MURPHY_FACING_RIGHT;
	murphy_moving_direction = MURPHY_RIGHT;
	murphy_frame = 0;

	/* Find the first instance of Murphy */
	for (unsigned int y = 0; y < 24; ++y) {
		for (unsigned int x = 0; x < 60; ++x) {
			coordinate c(x, y);

			if (!field[c].is_murphy())
				continue;

			field[c] = ELEMENT_SPACE;
			murphy_x = x << 4;
			murphy_y = y << 4;
			return;
		}
	}

	assert(false);
}

static void load_level(unsigned int level)
{
	/* Initialise game variables */
	for (unsigned int y = 0; y < 24; ++y) {
		for (unsigned int x = 0; x < 60; ++x)
			field[coordinate(x, y)] = element((element_type) levels[level].field[y][x]);
	}

	find_murphy();
}

static void draw()
{
	/* The GBA LCD is 240x160 pixels, and since we use 16x16 tiles, this
	 * means we get 15x10 tiles on the screen. HOWEVER, scrolling may
	 * cause us to display an extra row/an extra column, so we should
	 * always have 16x11 tiles in the background map. */

	/* Calculate various positions and offsets */
	uint16_t sprite_x;
	uint16_t map_x;
	uint16_t scroll_x;

	if (murphy_x < 112) {
		map_x = 0;
		scroll_x = 0;
		sprite_x = murphy_x;
	} else if (murphy_x >= 832) {
		map_x = 720 >> 4;
		scroll_x = 0;
		sprite_x = 112 + murphy_x - 832;
	} else {
		map_x = (murphy_x - 112) >> 4;
		scroll_x = (murphy_x - 112) & 0xf;
		sprite_x = 112;
	}

	uint16_t sprite_y;
	uint16_t map_y;
	uint16_t scroll_y;

	if (murphy_y < 72) {
		map_y = 0;
		scroll_y = 0;
		sprite_y = murphy_y;
	} else if (murphy_y >= 296) {
		map_y = 224 >> 4;
		scroll_y = 0;
		sprite_y = 72 + murphy_y - 296;
	} else {
		map_y = (murphy_y - 72) >> 4;
		scroll_y = (murphy_y - 72) & 0xf;
		sprite_y = 72;
	}

	/* Update BG map */
	uint8_t sprite = 1;

	for (uint16_t y = 0; y < 11; ++y) {
		uint16_t _2y = y + y;

		for (uint16_t x = 0; x < 16; ++x) {
			uint16_t _2x = x + x;

			element e = field[coordinate(map_x + x, map_y + y)];
			uint8_t code = e.code;
			if (code >= NR_STATIC_ELEMENTS) {
				/* Put these in an array of callbacks */
				if (code == ELEMENT_ZONK_FALLING_DOWN_TOP) {
					*((uint16_t *) 0x07000000 + 4 * sprite) = (16 * y - scroll_y + e.frame);
					*((uint16_t *) 0x07000002 + 4 * sprite) = (16 * x - scroll_x) | (1 << 14);
					*((uint16_t *) 0x07000004 + 4 * sprite) = 15 << 2;
					++sprite;
				}

				else if (code == ELEMENT_ZONK_ROLLING_LEFT_RIGHT) {
					*((uint16_t *) 0x07000000 + 4 * sprite) = (16 * y - scroll_y);
					*((uint16_t *) 0x07000002 + 4 * sprite) = (16 * x - scroll_x - e.frame) | (1 << 14);
					*((uint16_t *) 0x07000004 + 4 * sprite) = (16 + (e.frame >> 2)) << 2;
					++sprite;
				}

				else if (code == ELEMENT_ZONK_ROLLING_RIGHT_LEFT) {
					*((uint16_t *) 0x07000000 + 4 * sprite) = (16 * y - scroll_y);
					*((uint16_t *) 0x07000002 + 4 * sprite) = (16 * x - scroll_x + e.frame) | (1 << 12) | (1 << 14);
					*((uint16_t *) 0x07000004 + 4 * sprite) = (16 + (e.frame >> 2)) << 2;
					++sprite;
				}

				code = ELEMENT_SPACE;
			}

			*((uint16_t *) 0x06008000 + 32 * (_2y + 0) + (_2x + 0)) = tiles[code][0];
			*((uint16_t *) 0x06008000 + 32 * (_2y + 1) + (_2x + 0)) = tiles[code][2];
			*((uint16_t *) 0x06008000 + 32 * (_2y + 0) + (_2x + 1)) = tiles[code][1];
			*((uint16_t *) 0x06008000 + 32 * (_2y + 1) + (_2x + 1)) = tiles[code][3];
		}
	}

	/* Update BG0 scroll/offset */
	*(volatile uint16_t *) 0x04000010 = scroll_x;
	*(volatile uint16_t *) 0x04000012 = scroll_y;

	/* Update sprites */
	uint16_t sprite_tile;
	bool sprite_flip_x;

	switch (murphy_state) {
	case MURPHY_FACING:
		sprite_tile = 0;
		break;
	case MURPHY_MOVING:
		sprite_tile = 0 + (murphy_frame >> 2);
		break;
	}

	switch (murphy_facing_direction) {
	case MURPHY_LEFT:
		sprite_flip_x = false;
		break;
	case MURPHY_RIGHT:
		sprite_flip_x = true;
		break;
	}

	*(uint16_t *) 0x07000000 = sprite_y;
	*(uint16_t *) 0x07000002 = sprite_x | (sprite_flip_x << 12) | (1 << 14);
	*(uint16_t *) 0x07000004 = sprite_tile << 2;

	/* Disable remaining sprites */
	for (; sprite < 128; ++sprite)
		*((uint16_t *) 0x07000000 + 4 * sprite) = (1 << 9);
}

static void
vblank_irq()
{
#if 0
	/* Only process every second V-blank IRQ */
	static uint8_t state = 0;
	state = !state;
	if (state)
		return;
#endif

	/* The first thing the V-blank IRQ handler should do is take care
	 * that use the opportunity to update VRAM and the LCD parameters.
	 * Then we may update the game field and do other stuff that
	 * possibly carries over into the next display cycle. (That's fine;
	 * the original Supaplex also run on hardware that was so slow that
	 * it needed two screen refreshes to do everything, hence the need
	 * for a speed fix on newer machines.) */

	draw();

	/* Update game field */
	for (uint16_t c = 0; c < 60 * 24; ++c) {
		void (*fn)(const coordinate) = elements[field[coordinate(c)].code];
		if (fn)
			fn(coordinate(c));
	}

	/* Update the state of murphy */
	switch (murphy_state) {
	case MURPHY_FACING:
		if (murphy_frame < 256)
			++murphy_frame;
		break;
	case MURPHY_MOVING:
		if (murphy_frame < 16) {
			++murphy_frame;
			if (murphy_frame == 16) {
				murphy_state = MURPHY_FACING;
				murphy_frame = 0;
			}

			switch (murphy_moving_direction) {
			case MURPHY_LEFT:
				--murphy_x;
				break;
			case MURPHY_RIGHT:
				++murphy_x;
				break;
			case MURPHY_UP:
				--murphy_y;
				break;
			case MURPHY_DOWN:
				++murphy_y;
				break;
			}
		}

		break;
	}

	/* Deal with keypad changes */
	static uint16_t keypad_prev = 0;
	uint16_t keypad = ~*(volatile uint16_t *) 0x04000130;
	uint16_t keypad_pressed = ~keypad_prev & keypad;
	uint16_t keypad_released = keypad_prev & ~keypad;

	if (murphy_state == MURPHY_FACING) {
		coordinate c(murphy_x >> 4, murphy_y >> 4);

		if (keypad & (1 << 4)) {
			/* Right */
			murphy_facing_direction = MURPHY_FACING_RIGHT;
			if (field[c.right()].is_edible()) {
				field[c] = ELEMENT_MURPHY_MOVING;
				/* XXX: */ field[c.right()] = ELEMENT_MURPHY_STANDING;
				murphy_state = MURPHY_MOVING;
				murphy_moving_direction = MURPHY_RIGHT;
				murphy_frame = 0;
			}
		} else if (keypad & (1 << 5)) {
			/* Left */
			murphy_facing_direction = MURPHY_FACING_LEFT;
			if (field[c.left()].is_edible()) {
				field[c] = ELEMENT_MURPHY_MOVING;
				/* XXX: */ field[c.left()] = ELEMENT_MURPHY_STANDING;
				murphy_state = MURPHY_MOVING;
				murphy_moving_direction = MURPHY_LEFT;
				murphy_frame = 0;
			}
		} else if (keypad & (1 << 6)) {
			/* Up */
			if (field[c.above()].is_edible()) {
				field[c] = ELEMENT_MURPHY_MOVING;
				/* XXX: */ field[c.above()] = ELEMENT_MURPHY_STANDING;
				murphy_state = MURPHY_MOVING;
				murphy_moving_direction = MURPHY_UP;
				murphy_frame = 0;
			}
		} else if (keypad & (1 << 7)) {
			/* Down */
			if (field[c.below()].is_edible()) {
				field[c] = ELEMENT_MURPHY_MOVING;
				/* XXX: */ field[c.below()] = ELEMENT_MURPHY_STANDING;
				murphy_state = MURPHY_MOVING;
				murphy_moving_direction = MURPHY_DOWN;
				murphy_frame = 0;
			}
		}
	}

	if (keypad_pressed & (1 << 8)) {
		/* R */
		if (current_level < sizeof(levels) / sizeof(*levels) - 1)
			load_level(++current_level);
	}

	if (keypad_pressed & (1 << 9)) {
		/* L */
		if (current_level > 0)
			load_level(--current_level);
	}

	keypad_prev = keypad;
}

static void keypad_irq()
{
}

extern void irq()
{
	uint16_t flags = *(volatile uint16_t *) 0x04000202;

	if (flags & (1 << 0))
		vblank_irq();

	/* Intended for very low power mode only */
	if (flags & (1 << 12))
		keypad_irq();

	/* Acknowledge IRQ */
	*(volatile uint16_t *) 0x03007ff8 |= flags;
	*(volatile uint16_t *) 0x04000202 = flags;
}

static inline void vblank_wait()
{
#ifndef __thumb__
	asm volatile ("swi #0x50000"
		:
		:
		: "memory", "r0", "r1");
#else
	asm volatile ("swi #0x5"
		:
		:
		: "memory", "r0", "r1");
#endif
}

int main(void)
{
	/* Initialise element update functions */
	for (unsigned int i = 0; i < NR_ELEMENTS; ++i)
		elements[i] = 0;

	elements[ELEMENT_MURPHY_MOVING] = [](const coordinate c) {
		if (field[c].next_frame())
			field[c] = ELEMENT_SPACE;
	};

	elements[ELEMENT_ZONK] = [](const coordinate c) {
		coordinate below = c.below();

		if (field[below].is_space() || field[below].is_reserved()) {
			/* Fall down */
			field[c] = ELEMENT_ZONK_FALLING_DOWN_TOP;
			field[below] = ELEMENT_ZONK_FALLING_DOWN_BOTTOM;
		} else if (field[below].is_round()) {
			/* Roll off (XXX: Check priority) */
			if (field[c.right()].is_space() && field[below.right()].is_space()) {
				field[below.right()] = ELEMENT_RESERVED;
				field[c.right()] = ELEMENT_ZONK_ROLLING_RIGHT_RIGHT;
				field[c] = ELEMENT_ZONK_ROLLING_RIGHT_LEFT;
			} else if (field[c.left()].is_space() && field[below.left()].is_space()) {
				field[below.left()] = ELEMENT_RESERVED;
				field[c.left()] = ELEMENT_ZONK_ROLLING_LEFT_LEFT;
				field[c] = ELEMENT_ZONK_ROLLING_LEFT_RIGHT;
			}
		}
	};

	elements[ELEMENT_ZONK_FALLING_DOWN_TOP] = [](const coordinate c) {
		/* It fell out */
		if (field[c].next_frame())
			field[c] = ELEMENT_SPACE;
	};

	elements[ELEMENT_ZONK_FALLING_DOWN_BOTTOM] = [](const coordinate c) {
		/* It fell down */
		if (field[c].next_frame())
			field[c] = ELEMENT_ZONK;
	};

	elements[ELEMENT_ZONK_ROLLING_LEFT_LEFT] = [](const coordinate c) {
		if (field[c].next_frame())
			field[c] = ELEMENT_ZONK;
	};

	elements[ELEMENT_ZONK_ROLLING_LEFT_RIGHT] = [](const coordinate c) {
		if (field[c].next_frame())
			field[c] = ELEMENT_SPACE;
	};

	elements[ELEMENT_ZONK_ROLLING_RIGHT_RIGHT] = [](const coordinate c) {
		if (field[c].next_frame())
			field[c] = ELEMENT_ZONK;
	};

	elements[ELEMENT_ZONK_ROLLING_RIGHT_LEFT] = [](const coordinate c) {
		if (field[c].next_frame())
			field[c] = ELEMENT_SPACE;
	};

	/* LCD off */
	*(volatile uint16_t *) 0x04000000 = (1 << 7);

	/* Palettes */
	for (unsigned int i = 0; i < sizeof(palettes) / sizeof(*palettes); ++i)
		*((uint16_t *) 0x05000000 + i) = palettes[i];

	/* Sprite palettes */
	for (unsigned int i = 0; i < sizeof(palettes) / sizeof(*palettes); ++i)
		*((uint16_t *) 0x05000200 + i) = palettes[i];

	/* Tiles */
	for (unsigned int i = 0; i < sizeof(fixed) / sizeof(*fixed); ++i)
		*((uint32_t *) 0x06000000 + i) = fixed[i];

	/* Sprites */
	for (unsigned int i = 0; i < sizeof(moving) / sizeof(*moving); ++i)
		*((uint32_t *) 0x06010000 + i) = moving[i];

	/* BG0 control */
	*(volatile uint16_t *) 0x04000008 = (16 << 8);

	/* Disable unused sprites */
	for (unsigned int i = 0; i < 128; ++i)
		*((volatile uint16_t *) 0x07000000 + 4 * i) = (1 << 9);

	/* Set BG mode */
	*(volatile uint16_t *) 0x04000000 = (1 << 6) | (1 << 8) | (1 << 12);

	load_level(current_level = 0);
	draw();

	/* Set up interrupt handler */
	*(volatile void **) 0x03007ffc = (void *) &irq;

	/* Acknowledge any outstanding IRQs */
	//*(volatile uint16_t *) 0x04000202 = *(volatile uint16_t *) 0x04000202;

	/* Request V-blank interrupt */
	*(volatile uint16_t *) 0x04000004 = (1 << 3);
	*(volatile uint16_t *) 0x04000200 |= (1 << 0);

	/* Request keypad interrupt */
	*(volatile uint16_t *) 0x04000132 = /* Start: */ (1 << 3) | (1 << 14);
	*(volatile uint16_t *) 0x04000200 |= (1 << 12);

	/* Master interrupt enable */
	*(volatile uint16_t *) 0x04000208 = 1;

	while (true)
		vblank_wait();
}
