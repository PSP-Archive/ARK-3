#include <pspmoduleexport.h>
#define NULL ((void *) 0)

extern void module_start;
extern void module_info;
static const unsigned int __syslib_exports[4] __attribute__((section(".rodata.sceResident"))) = {
	0xD632ACDB,
	0xF01D73A7,
	(unsigned int) &module_start,
	(unsigned int) &module_info,
};

extern void myUtilityLoadModule;
extern void myUtilityUnloadModule;
extern void myKernelLoadModule;
static const unsigned int __stargate_exports[6] __attribute__((section(".rodata.sceResident"))) = {
	0x325FE63A,
	0x568839DC,
	0xF104524C,
	(unsigned int) &myUtilityLoadModule,
	(unsigned int) &myUtilityUnloadModule,
	(unsigned int) &myKernelLoadModule,
};

const struct _PspLibraryEntry __library_exports[2] __attribute__((section(".lib.ent"), used)) = {
	{ NULL, 0x0000, 0x8000, 4, 1, 1, &__syslib_exports },
	{ "stargate", 0x0011, 0x4001, 4, 0, 3, &__stargate_exports },
};
