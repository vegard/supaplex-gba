#ifndef COORDINATE_HH
#define COORDINATE_HH

#include <stdint.h>

/* Pass by value */
class coordinate {
public:
	uint16_t raw;

	explicit coordinate(uint16_t raw):
		raw(raw)
	{
	}

	coordinate(unsigned int x, unsigned int y):
		raw(60 * y + x)
	{
	}

	coordinate next() const
	{
		return coordinate(raw - 1);
	}

	coordinate left() const
	{
		return coordinate(raw - 1);
	}

	coordinate right() const
	{
		return coordinate(raw + 1);
	}

	coordinate above() const
	{
		return coordinate(raw - 60);
	}

	coordinate below() const
	{
		return coordinate(raw + 60);
	}

	operator uint16_t() const
	{
		return raw;
	}
};

#endif
