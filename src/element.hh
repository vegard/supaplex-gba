#ifndef ELEMENT_HH
#define ELEMENT_HH

#include <stdint.h>

#include "element_type.hh"

class element {
public:
	uint8_t code;
	uint8_t frame;

	element()
	{
	}

	explicit element(element_type code):
		code(code),
		frame(0)
	{
	}

	void operator=(element_type new_code)
	{
		code = new_code;
		frame = 0;
	}

	/* XXX: Needed? */
	bool is_space() const
	{
		return code == ELEMENT_SPACE;
	}

	bool is_murphy() const
	{
		return code == ELEMENT_MURPHY;
	}

	bool is_exit() const
	{
		return code == ELEMENT_EXIT;
	}

	bool is_round() const
	{
		if (code == ELEMENT_CHIP_SQUARE || code == ELEMENT_CHIP_VERTICAL_TOP)
			return true;
		if (code == ELEMENT_CHIP_HORIZONTAL_LEFT || code == ELEMENT_CHIP_HORIZONTAL_RIGHT)
			return true;

		return code == ELEMENT_ZONK || code == ELEMENT_INFOTRON;
	}

	bool is_edible() const
	{
		return code == ELEMENT_SPACE
			|| code == ELEMENT_BASE
			|| code == ELEMENT_INFOTRON
			|| code == ELEMENT_DISK_RED
			|| code == ELEMENT_BUG;
	}

	bool is_explodable() const
	{
		if (code == ELEMENT_WALL || code == ELEMENT_WALL_INVISIBLE)
			return false;

		return code < ELEMENT_HARDWARE_1 || code > ELEMENT_HARDWARE_7;
	}

	bool is_reserved() const
	{
		return code == ELEMENT_RESERVED;
	}

	/* Move to the next frame. If we reached the final number of frames
	 * (the first parameter), return true and reset the counter to 0. */
	bool next_frame(unsigned int nr_frames = 16)
	{
		if (++frame < nr_frames)
			return false;

		frame = 0;
		return true;
	}
};

#endif
