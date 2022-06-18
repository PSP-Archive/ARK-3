#include <pspsdk.h>
#include <offsets.h>
#include <graphics.h>
#include <macros.h>
#include <module2.h>
#include <pspdisplay_kernel.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <functions.h>
#include "custom_png.h"
#include "filesystem.h"
#include "vitapops.h"

int (* _sceDisplaySetFrameBufferInternal)(int pri, void *topaddr, int width, int format, int sync) = 0;
int (* SetKeys)(char *filename, void *keys, void *keys2) = 0;
int (* scePopsSetKeys)(int size, void *keys, void *keys2) = 0;

int sctrlHENIsVitaPops(){
	int k1 = pspSdkSetK1(0);
	int ret = IS_VITA_POPS;
	pspSdkSetK1(k1);
	return ret;
}

void* sctrlHENSetPSXVramHandler(void (*handler)(u32* psp_vram, u16* ps1_vram)){
	int k1 = pspSdkSetK1(0);
	void* prev = registerPSXVramHandler(handler);
	pspSdkSetK1(k1);
	return prev;
}

int ReadFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	if(fd < 0) return fd;
	int read = sceIoRead(fd, buf, size);
	sceIoClose(fd);
	return read;
}

int WriteFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0) return fd;
	int written = sceIoWrite(fd, buf, size);
	sceIoClose(fd);
	return written;
}

int (* scePopsManExitVSHKernel)(u32 error);
int scePopsManExitVSHKernelPSX(u32 destSize, u8 *src, u8 *dest)
{
	int k1 = pspSdkSetK1(0);

	if(destSize & 0x80000000)
	{
		scePopsManExitVSHKernel(destSize);
		pspSdkSetK1(k1);
		return 0;
	}

	int size = sceKernelDeflateDecompress(dest, destSize, src, 0);
	pspSdkSetK1(k1);

	return (size ^ 0x9300 ? size : 0x92FF);
}

int sceDisplaySetFrameBufferInternalHook(int pri, void *topaddr,
		int width, int format, int sync){
	copyPSPVram(topaddr);
	return _sceDisplaySetFrameBufferInternal(pri, topaddr, width, format, sync); 
}

void patchVitaPops(SceModule2* mod){
	
	if(is_custom_psx)
	{
		u32 text_addr = mod->text_addr;
		/* Use our decompression function */
		_sw(_lw(text_addr + 0x0000C750), text_addr + 0x0000CA50);

		/* Patch PNG size */
		if(use_custom_png)
		{
			_sw(0x24050000 | (size_custom_png & 0xFFFF), text_addr + 0x00017DEC);
		}
	}
}

void patchVitaPopsManager(SceModule2* mod){
	if (!IS_VITA_POPS)
		return;

	SceUID fd = sceIoOpen(sceKernelInitFileName(), PSP_O_RDONLY, 0);
	if(fd >= 0)
	{
		char* modname = mod->modname;
		u32 text_addr = mod->text_addr;
		PBPHeader header;
		sceIoRead(fd, &header, sizeof(PBPHeader));

		u32 pgd_offset = header.psar_offset;
		u32 icon0_offset = header.icon0_offset;

		u8 buffer[8];
		sceIoLseek(fd, header.psar_offset, PSP_SEEK_SET);
		sceIoRead(fd, buffer, 7);

		if(memcmp(buffer, "PSTITLE", 7) == 0) //official psx game
		{
			pgd_offset += 0x200;
		}
		else
		{
			pgd_offset += 0x400;
		}

		u32 pgd_header;
		sceIoLseek(fd, pgd_offset, PSP_SEEK_SET);
		sceIoRead(fd, &pgd_header, sizeof(u32));

		/* Is not PGD header */
		if(pgd_header != 0x44475000)
		{
			is_custom_psx = 1;

			u32 icon_header[6];
			sceIoLseek(fd, icon0_offset, PSP_SEEK_SET);
			sceIoRead(fd, icon_header, sizeof(icon_header));

			/* Check 80x80 PNG */
			if(icon_header[0] == 0x474E5089 &&
			   icon_header[1] == 0x0A1A0A0D &&
			   icon_header[3] == 0x52444849 &&
			   icon_header[4] == 0x50000000 &&
			   icon_header[5] == 0x50000000)
			{
				use_custom_png = 0;
			}

			scePopsManExitVSHKernel = (void *)FindFunction("scePops_Manager", "scePopsMan", 0x0090B2C8);
			sctrlHENPatchSyscall((u32)scePopsManExitVSHKernel, scePopsManExitVSHKernelPSX);

			/* Patch IO */
			MAKE_JUMP_PATCH(sctrlHENFindImport(modname, "IoFileMgrForKernel", 0x109F50BC), sceIoOpenPSX);
			MAKE_JUMP_PATCH(sctrlHENFindImport(modname, "IoFileMgrForKernel", 0x63632449), sceIoIoctlPSX);
			MAKE_JUMP_PATCH(sctrlHENFindImport(modname, "IoFileMgrForKernel", 0x6A638D83), sceIoReadPSX);

			/* Dummy amctrl decryption functions */
			MAKE_DUMMY_FUNCTION(text_addr + 0x00000E84, 0);
			MAKE_DUMMY_FUNCTION(FindFunction(modname, "sceMeAudio", 0xF6637A72), 1);
			_sw(0, text_addr + 0x0000053C);

			/* Allow lower compiled sdk versions */
			_sw(0, text_addr + 0x000010D0);
		}

		sceIoClose(fd);

		scePopsSetKeys = (void *)text_addr + 0x00000124;

		HIJACK_FUNCTION(text_addr + 0x000014FC, vitaPopsSetKeysPatched, SetKeys);
	}
}

// TODO: Create a PSX Vram handler for hardware copy
void psxVramHardwareHandler(u32* psp_vram, u16* psx_vram){
}

void patchVitaPopsDisplay(){
	
	u32 display_func = FindFunction("sceDisplay_Service", "sceDisplay_driver", 0x3E17FE8D);
	
	if (display_func != 0){
		HIJACK_FUNCTION(display_func, sceDisplaySetFrameBufferInternalHook,
			_sceDisplaySetFrameBufferInternal);
	}
	
	// TODO: register a Vram Handler for Hardware drawing
	//registerPSXVramHandler(&psxVramHardwareHandler);
}

int vitaPopsSetKeysPatched(char *filename, u8 *keys, u8 *keys2)
{
	char path[64];
	strcpy(path, filename);

	char *p = strrchr(path, '/');
	if(!p) return 0xCA000000;

	strcpy(p + 1, "KEYS.BIN");

	if(ReadFile(path, keys, 0x10) != 0x10)
	{
		SceUID fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
		if(fd >= 0)
		{
			u32 header[0x28/4];
			sceIoRead(fd, header, 0x28);
			sceIoLseek(fd, header[0x20/4], PSP_SEEK_SET);
			sceIoRead(fd, header, 4);
			sceIoClose(fd);

			if(header[0] == 0x464C457F)
			{
				memset(keys, 'X', 0x10);
				goto SET_KEYS;
			}
		}

		int res = SetKeys(filename, keys, keys2);
		if(res >= 0) WriteFile(path, keys, 0x10);
		return res;
	}

SET_KEYS:
	scePopsSetKeys(0x10, keys, keys);
	return 0;
}
