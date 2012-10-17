#ifndef ASSERT_H
#define ASSERT_H

#include "halt.hh"

#define assert(x) \
	do { \
		if (!(x)) { \
			halt();\
		} \
	} while (0)

#endif
