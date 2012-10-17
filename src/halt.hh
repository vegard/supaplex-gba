#ifndef HALT_HH
#define HALT_HH

void halt()
{
#ifndef __thumb__
	asm volatile ("swi #0x260000"
		:
		:
		: "memory");
#else
	asm volatile ("swi #0x26"
		:
		:
		: "memory");
#endif
}

#endif
