#ifndef VITA_POPS
#define VITA_POPS

extern int (* _sceDisplaySetFrameBufferInternal)(int pri, void *topaddr, int width, int format, int sync);
extern int (* SetKeys)(char *filename, void *keys, void *keys2);
extern int (* scePopsSetKeys)(int size, void *keys, void *keys2);

int sctrlHENIsVitaPops();
void patchVitaPops(SceModule2* mod);
void patchVitaPopsManager(SceModule2* mod);
void patchVitaPopsDisplay();
__attribute__((noinline)) void PSXFlashScreen(u32 color);
int sceDisplaySetFrameBufferInternalHook(int pri, void *topaddr,
		int width, int format, int sync);

int vitaPopsSetKeysPatched(char *filename, u8 *keys, u8 *keys2);

#endif
