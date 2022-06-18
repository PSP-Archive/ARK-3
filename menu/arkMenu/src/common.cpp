#include "common.h"
#include <ctime>
#include <dirent.h>
#include <algorithm>

#include "systemctrl.h"

#define CONFIG_PATH "ms0:/ARKCONFIG.BIN"

using namespace common;

static Image* images[MAX_IMAGES];
static intraFont* font;
static MP3* sound_mp3;
static int argc;
static char **argv;
static float scrollX = 0.f;
static float scrollY = 0.f;
static float scrollXTmp = 0.f;
static int currentFont = 0;
static char exploitID[10] = {0};

static bool flipControl = false;

char* fonts[] = {
	"FONT.PGF",
	"flash0:/font/ltn0.pgf",
	"flash0:/font/ltn1.pgf",
	"flash0:/font/ltn2.pgf",
	"flash0:/font/ltn3.pgf",
	"flash0:/font/ltn4.pgf",
	"flash0:/font/ltn5.pgf",
	"flash0:/font/ltn6.pgf",
	"flash0:/font/ltn7.pgf",
	"flash0:/font/ltn8.pgf",
	"flash0:/font/ltn9.pgf",
	"flash0:/font/ltn19.pgf",
	"flash0:/font/ltn11.pgf",
	"flash0:/font/ltn12.pgf",
	"flash0:/font/ltn13.pgf",
	"flash0:/font/ltn14.pgf",
	"flash0:/font/ltn15.pgf",
	"flash0:/font/jpn0.pgf",
	"flash0:/font/kr0.pgf"
};

static t_conf config;

void setArgs(int ac, char** av){
	argc = ac;
	argv = av;
}

void loadConfig(){
	FILE* fp = fopen(CONFIG_PATH, "rb");
	if (fp == NULL){
		resetConf();
		return;
	}
	fseek(fp, 0, SEEK_END);
	
	if (ftell(fp) != sizeof(t_conf)){
		fclose(fp);
		resetConf();
		return;
	}
	fseek(fp, 0, SEEK_SET);
	fread(&config, 1, sizeof(t_conf), fp);
	fclose(fp);
}

char* common::getExploitID(){
	return exploitID;
}

void common::saveConf(){

	if (currentFont != config.font){
		if (!fileExists(fonts[config.font]))
			config.font = 1;
	
		intraFont* aux = font;
		font = NULL;
		intraFontUnload(aux);
		font = intraFontLoad(fonts[config.font], 0);
	}
	
	FILE* fp = fopen(CONFIG_PATH, "wb");
	fwrite(&config, 1, sizeof(t_conf), fp);
	fclose(fp);
}

t_conf* common::getConf(){
	return &config;
}

void common::resetConf(){
	config.iso_driver = 2;
	config.hide_exploit = 1;
	config.fast_gameboot = 0;
	config.language = 0;
	config.font = 1;
	config.plugins = 1;
	config.scan_save = 0;
	config.swap_buttons = 0;
	config.animation = 0;
}

SceOff common::findPkgOffset(const char* filename, unsigned* size){
    
    printf("finding file %s\n", filename);
    
	FILE* pkg = fopen(PKG_PATH, "rb");
	if (pkg == NULL)
		return 0;
     
	fseek(pkg, 0, SEEK_END);
     
	unsigned pkgsize = ftell(pkg);
	unsigned size2 = 0;
     
	fseek(pkg, 0, SEEK_SET);

	if (size != NULL)
		*size = 0;

	unsigned offset = 0;
	char name[64];
           
	while (offset != 0xFFFFFFFF){
		fread(&offset, 1, 4, pkg);
		if (offset == 0xFFFFFFFF){
			fclose(pkg);
			return 0;
		}
		unsigned namelength;
		fread(&namelength, 1, 4, pkg);
		fread(name, 1, namelength+1, pkg);
                   
		if (!strncmp(name, filename, namelength)){
			fread(&size2, 1, 4, pkg);
    
			if (size2 == 0xFFFFFFFF)
				size2 = pkgsize;

			if (size != NULL)
				*size = size2 - offset;
     
			fclose(pkg);
			return offset;
		}
	}
	return 0;
}

void* common::readFromPKG(const char* filename, unsigned* size){

	unsigned mySize;
	
	if (size == NULL)
		size = &mySize;

	unsigned offset = findPkgOffset(filename, size);
	
	FILE* fp = fopen(PKG_PATH, "rb");
	
	if (offset == 0 || fp == NULL){
		fclose(fp);
		return NULL;
	}
	
	void* data = malloc(*size);
	fseek(fp, offset, SEEK_SET);
	fread(data, 1, *size, fp);
	fclose(fp);
	return data;
}


int common::getArgc(){
	return argc;
}

char** common::getArgv(){
	return argv;
}

bool common::has_suffix(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

u32 common::getMagic(const char* filename, unsigned int offset){
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL)
		return 0;
	u32 magic;
	fseek(fp, offset, SEEK_SET);
	fread(&magic, 4, 1, fp);
	fclose(fp);
	return magic;
}

void common::loadData(int ac, char** av){

	if (sctrlHENGetVersion() >= 0x2003)
		sctrlGetExploitID(exploitID);

	argc = ac;
	argv = av;

	loadConfig();
	
	images[IMAGE_BG] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("DEFBG.PNG"));
	images[IMAGE_SPRITE] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("SPRITE.PNG"));
	images[IMAGE_LOADING] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("LOADING.PNG"));
	images[IMAGE_NOICON] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("NOICON.PNG"));
	images[IMAGE_WAITICON] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("WAIT.PNG"));
	images[IMAGE_GAME] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("GAME.PNG"));
	images[IMAGE_FTP] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("FTP.PNG"));
	images[IMAGE_SETTINGS] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("SETTINGS.PNG"));
	images[IMAGE_BROWSER] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("BROWSER.PNG"));
	images[IMAGE_DIALOG] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("BOX.PNG"));
	images[IMAGE_ZIP] = new Image(PKG_PATH, YA2D_PLACE_RAM, findPkgOffset("ZIP.PNG"));
	
	for (int i=0; i<MAX_IMAGES; i++){
		images[i]->swizzle();
		images[i]->is_system_image = true;
	}
	
	if (!fileExists(fonts[config.font]))
		config.font = 1;
	font = intraFontLoad(fonts[config.font], 0);
	
	currentFont = config.font;
	
	unsigned mp3_size;
	void* mp3_buffer = readFromPKG("SOUND.MP3", &mp3_size);
	sound_mp3 = new MP3(mp3_buffer, mp3_size);
}

void common::deleteData(){
	for (int i=0; i<MAX_IMAGES; i++)
		delete images[i];
	intraFontUnload(font);
	delete sound_mp3;
}

bool common::fileExists(const std::string &path){
	FILE* fp = fopen(path.c_str(), "rb");
	if (fp == NULL)
		return false;
	fclose(fp);
	return true;
}

bool common::folderExists(const std::string &path){
	DIR* dir = opendir(path.c_str());
	if (dir == NULL)
		return false;
	closedir(dir);
	return true;
}

Image* common::getImage(int which){
	return (which < MAX_IMAGES)? images[which] : images[IMAGE_BG];
}

bool common::isSharedImage(Image* img){

	if (img->is_system_image)
		return true;

	for (int i=0; i<MAX_IMAGES; i++){
		if (images[i] == img)
			return true;
	}
	return false;
}

intraFont* common::getFont(){
	return font;
}

MP3* common::getMP3Sound(){
	return sound_mp3;
}

void common::playMenuSound(){
	playMP3File(NULL, common::getMP3Sound()->getBuffer(), common::getMP3Sound()->getBufferSize());
	//sound_mp3->play();
}

void common::printText(float x, float y, const char *text, u32 color, float size, int glow, int scroll){

	if (font == NULL)
		return;

	u32 secondColor = BLACK_COLOR;
	u32 arg5 = 0;
	
	if (glow){
		float t = (float)((float)(clock() % CLOCKS_PER_SEC)) / ((float)CLOCKS_PER_SEC);
		int val = (t < 0.5f) ? t*511 : (1.0f-t)*511;
		secondColor = (0xFF<<24)+(val<<16)+(val<<8)+(val);
	}
	if (scroll){
		arg5 = INTRAFONT_SCROLL_LEFT;
	}
	
	intraFontSetStyle(font, size, color, secondColor, arg5);

	if (scroll){
		if (x != scrollX || y != scrollY){
			scrollX = x;
			scrollXTmp = x;
			scrollY = y;
		}
		scrollXTmp = intraFontPrintColumn(font, scrollXTmp, y, 200, text);
	}
	else
		intraFontPrint(font, x, y, text);
}

void common::clearScreen(u32 color){
	ya2d_start_drawing();
	ya2d_clear_screen(color);
	flipControl = true;
}

void common::drawBorder(){
	ya2d_draw_rect(10, 20, 80, 254, GRAY_COLOR, 0);
	ya2d_draw_rect(170, 20, 80, 254, GRAY_COLOR, 0);
	ya2d_draw_rect(330, 20, 80, 254, GRAY_COLOR, 0);
}


void common::flipScreen(){

	if (!flipControl)
		return;

	sceDisplayWaitVblankStart();
	ya2d_finish_drawing();
	ya2d_swapbuffers();
	flipControl = false;
};

void common::upperString(char* str){
	while (*str){
		if (*str >= 'a' && *str <= 'z')
			*str -= 0x20;
		str++;
	}
}

int common::maxString(string* strings, int n_strings){
	int max = 0;
	for (int i = 0; i<n_strings; i++){
		int len = strings[i].length();
		if (len > max)
			max = len;
	}
	return max;
}

std::string common::getExtension(std::string path){
	std::string ext = path.substr(path.find_last_of(".") + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return ext;
}
