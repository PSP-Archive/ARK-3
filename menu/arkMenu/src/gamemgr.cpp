/* Game Manager class */

#include <psppower.h>
#include <sstream>
#include "gamemgr.h"
#include "zip.h"
#include "ftp_main.h"
#include "osk.h"

static struct {
	int x;
	int y;
	char* name;
} pEntries[3] = {
	{55, 125, "Game"},
	{225, 125, "FTP"},
	{375, 125, "Browser"},
};

bool GameManager::update_game_list = false;

int PopupMenu = 1;

static GameManager* self = NULL;

static bool loadingData = false;

GameManager::GameManager(){

	// set the global self variable as this instance for the threads to use it
	self = this;

	// start the drawing thread
	this->hasLoaded = false;
	this->drawThreadID = sceKernelCreateThread("draw_thread", GameManager::drawThread, 0x10, 0x8000, PSP_THREAD_ATTR_USER, NULL);
	this->drawSema = sceKernelCreateSema("draw_sema",  0, 1, 1, NULL);
	this->optionsDrawState = -1;
	this->optionsAnimState = 0;
	this->drawVSH = false;
	this->drawMode = 0;
	
	this->vshmenu = new VSHMenu();

	animations[0] = new PixelAnim();
	animations[1] = new Waves();
	animations[2] = new Sprites();
	animations[3] = new Fire();
	animations[4] = new Tetris();
	animations[5] = new Matrix();
	animations[6] = new NoAnim();
	
	sceKernelStartThread(drawThreadID, 0, NULL);
	
	browser = new Browser();
	
	browser->setDrawThreadID(this->drawThreadID);
	
	GameManager::update_game_list = false;
	
	// initialize the categories
	this->selectedCategory = -1;
	for (int i=0; i<MAX_CATEGORIES; i++){
		this->categories[i] = new CatMenu((EntryType)i);
	}
	
	// find all available entries
	this->findEntries();
	
	// start the multithreaded icon loading
	this->dynamicIconRunning = true;
	this->iconSema = sceKernelCreateSema("icon0_sema",  0, 1, 1, NULL);
	this->iconThread = sceKernelCreateThread("icon0_thread", GameManager::loadIcons, 0x10, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	sceKernelStartThread(this->iconThread,  0, NULL);
	
	// start the index for the options menu
	this->pEntryIndex = 0;
}

GameManager::~GameManager(){
	for (int i=0; i<MAX_CATEGORIES; i++){
		delete this->categories[i];
	}
}

void GameManager::installPlugin(string path){
	self->vshmenu->getPluginsManager()->addNewPlugin(path);
}

int GameManager::loadIcons(SceSize _args, void *_argp){

	sceKernelDelayThread(0);

	while (self->dynamicIconRunning){
		for (int i=0; i<MAX_CATEGORIES; i++){
			sceKernelWaitSema(self->iconSema, 1, NULL);
			self->categories[i]->loadIconsDynamic(i == self->selectedCategory);
			sceKernelSignalSema(self->iconSema, 1);
			sceKernelDelayThread(0);
		}
	}

	sceKernelExitDeleteThread(0);
	
	return 0;
}

void GameManager::pauseIcons(){
	for (int i=0; i<MAX_CATEGORIES; i++)
		categories[i]->waitIconsLoad(i == selectedCategory, true);
	sceKernelWaitSema(iconSema, 1, NULL);
}
void GameManager::resumeIcons(){
	for (int i=0; i<MAX_CATEGORIES; i++)
		categories[i]->resumeIconLoading();
	sceKernelSignalSema(iconSema, 1);
	sceKernelDelayThread(0);
}

bool GameManager::waitIconsLoad(bool forceQuit){
	for (int i = 0; i<MAX_CATEGORIES; i++){
		if (!this->categories[i]->waitIconsLoad(i == selectedCategory, forceQuit))
			return false;
	}
	return true;
}

CatMenu* GameManager::getMenu(EntryType t){
	return this->categories[(int)t];
}

void GameManager::findEntries(){
	this->categories[0]->clearEntries();
	this->categories[1]->clearEntries();
	this->categories[2]->clearEntries();
	this->findEboots();
	this->findISOs();
	if (common::getConf()->scan_save)
		this->findSaveEntries();
	this->selectedCategory = -1;
	// find the first category with entries
	for (int i=0; i<MAX_CATEGORIES && selectedCategory < 0; i++){
		if (!this->categories[i]->empty())
			this->selectedCategory = i;
	}
}

void GameManager::findEboots(){
	struct dirent* dit;
	DIR* dir = opendir("ms0:/PSP/GAME/");
	
	if (dir == NULL)
		return;
		
	while ((dit = readdir(dir))){

		string fullpath = Eboot::fullEbootPath(dit->d_name);
		if (fullpath == "") continue;
		if (strcmp(dit->d_name, ".") == 0) continue;
		if (strcmp(dit->d_name, "..") == 0) continue;
		if (common::fileExists(string("ms0:/PSP/GAME/")+string(dit->d_name))) continue;
		
		Eboot* e = new Eboot(fullpath);
		switch (Eboot::getEbootType(fullpath.c_str())){
		case TYPE_HOMEBREW:	this->categories[HOMEBREW]->addEntry(e);	break;
		case TYPE_PSN:		this->categories[GAME]->addEntry(e);		break;
		case TYPE_POPS:		this->categories[POPS]->addEntry(e);		break;
		}
	}
	closedir(dir);
}

void GameManager::findISOs(){

	struct dirent* dit;
	DIR* dir = opendir("ms0:/ISO/");
	
	if (dir == NULL)
		return;
		
	while ((dit = readdir(dir))){

		string fullpath = string("ms0:/ISO/")+string(dit->d_name);

		if (strcmp(dit->d_name, ".") == 0) continue;
		if (strcmp(dit->d_name, "..") == 0) continue;
		if (!common::fileExists(fullpath)) continue;
		if (Iso::isISO(fullpath.c_str())) this->categories[GAME]->addEntry(new Iso(fullpath));
		else if (Cso::isCSO(fullpath.c_str())) this->categories[GAME]->addEntry(new Cso(fullpath));
	}
	closedir(dir);
}

void GameManager::findSaveEntries(){
	struct dirent* dit;
	DIR* dir = opendir("ms0:/PSP/SAVEDATA/");
	
	if (dir == NULL)
		return;
		
	while ((dit = readdir(dir))){

		string fullpath = string("ms0:/PSP/SAVEDATA/")+string(dit->d_name);

		if (strcmp(dit->d_name, ".") == 0) continue;
		if (strcmp(dit->d_name, "..") == 0) continue;
		if (common::folderExists(fullpath)){
			struct dirent* savedit;
			DIR* savedir = opendir(fullpath.c_str());
			if (savedir == NULL)
				continue;
			while ((savedit = readdir(savedir))){
				if (strcmp(savedit->d_name, ".") == 0) continue;
				if (strcmp(savedit->d_name, "..") == 0) continue;
				string fullentrypath = fullpath + "/" + string(savedit->d_name);
				if ((common::getExtension(fullentrypath) == string("iso"))){
					if (Iso::isISO(fullentrypath.c_str()))
						this->categories[GAME]->addEntry(new Iso(fullentrypath));
				}
				else if ((common::getExtension(fullentrypath) == string("cso"))){
					if (Cso::isCSO(fullentrypath.c_str()))
						this->categories[GAME]->addEntry(new Cso(fullentrypath));
				}
				else if ((common::getExtension(fullentrypath) == string("zip"))){
					if (Zip::isZip(fullentrypath.c_str()))
						this->categories[HOMEBREW]->addEntry(new Zip(fullentrypath));
				}
				else if ((common::getExtension(fullentrypath) == string("pbp"))){
					if (Eboot::isEboot(fullentrypath.c_str())){
						Eboot* e = new Eboot(fullentrypath);
						switch (Eboot::getEbootType(fullentrypath.c_str())){
						case TYPE_HOMEBREW:	this->categories[HOMEBREW]->addEntry(e);	break;
						case TYPE_PSN:		this->categories[GAME]->addEntry(e);		break;
						case TYPE_POPS:		this->categories[POPS]->addEntry(e);		break;
						}
					}
				}
			}
			closedir(savedir);
		}
	}
	closedir(dir);
}

Entry* GameManager::getEntry(){
	if (selectedCategory < 0)
		return NULL;
	return this->categories[this->selectedCategory]->getEntry();
}

int GameManager::getPreviousCategory(int current){
	if (current > 0){
		current--;
	}
	else{
		current = MAX_CATEGORIES-1;
	}
	if (this->categories[current]->empty())
		return getPreviousCategory(current);
	return current;
}

int GameManager::getNextCategory(int current){
	if (current < MAX_CATEGORIES-1){
		current++;
	}
	else{
		current = 0;
	}
	if (this->categories[current]->empty())
		return getNextCategory(current);
	return current;
}

void GameManager::moveLeft(){
	if (selectedCategory < 0)
		return;

	int auxCategory = getPreviousCategory(selectedCategory);

	if (auxCategory != selectedCategory){
		this->categories[selectedCategory]->animStart(2);
		this->categories[auxCategory]->animStart(2);
		this->selectedCategory = auxCategory;
		common::playMenuSound();
	}
	sceKernelDelayThread(100000);
}

void GameManager::moveRight(){
	if (selectedCategory < 0)
		return;
	
	int auxCategory = getNextCategory(selectedCategory);
	
	if (auxCategory != selectedCategory){
		this->categories[selectedCategory]->animStart(2);
		this->categories[auxCategory]->animStart(2);
		this->selectedCategory = auxCategory;
		common::playMenuSound();
	}
	sceKernelDelayThread(100000);
}

void GameManager::moveUp(){
	if (selectedCategory < 0)
		return;
	this->categories[this->selectedCategory]->moveUp();
}

void GameManager::moveDown(){
	if (selectedCategory < 0)
		return;
	this->categories[this->selectedCategory]->moveDown();
}

void GameManager::stopFastScroll(){
	if (selectedCategory < 0)
		return;
	this->categories[this->selectedCategory]->stopFastScroll();
}

void GameManager::draw(){

	if (animations[common::getConf()->animation]->drawBackground())
		common::getImage(IMAGE_BG)->draw(0, 0);

	animations[common::getConf()->animation]->draw();
	
	ostringstream fps;
	ya2d_calc_fps();
	fps<<ya2d_get_fps();
	
	common::printText(460, 260, fps.str().c_str());

	if (this->hasLoaded){

		char* entryName;

		if (this->selectedCategory >= 0){
			for (int i=0; i<MAX_CATEGORIES; i++){
				if (i == (int)this->selectedCategory)
					continue;
				this->categories[i]->draw(false);
				sceKernelDelayThread(0);
			}
			this->categories[this->selectedCategory]->draw(true);
			entryName = (char*)this->getEntry()->getName().c_str();
		}
		else
			entryName = "No games available";
	
		if (optionsDrawState == -1){
			common::getImage(IMAGE_DIALOG)->draw_scale(0, 0, 480, 20);
			common::printText(0, 13, entryName, LITEGRAY, SIZE_BIG, true);
			drawBattery();
			if (loadingData){
				Image* img = common::getImage(IMAGE_WAITICON);
				img->draw((480-img->getTexture()->width)/2, (272-img->getTexture()->height)/2);
			}
			if (this->drawVSH)
				this->vshmenu->draw();
		}
		else
			drawOptionsMenu();
	}
}

void GameManager::updateScreen(){
	common::clearScreen(CLEAR_COLOR);
	this->draw();
	common::flipScreen();
}

int GameManager::drawThread(SceSize _args, void *_argp){
	self->keepDrawing = true;
	while (self->keepDrawing){
		sceKernelWaitSema(self->drawSema, 1, NULL);
		switch (self->drawMode){
		case 0:	self->updateScreen(); break;
		case 1: break; // let FTP draw itself
		case 2: self->keepDrawing = self->browser->drawThread(); break;
		}
		sceKernelSignalSema(self->drawSema, 1);
		sceKernelDelayThread(0);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

void GameManager::pauseDraw(){
	sceKernelWaitSema(this->drawSema, 1, NULL);
}

void GameManager::resumeDraw(){
	sceKernelSignalSema(this->drawSema, 1);
	sceKernelDelayThread(0);
}

void GameManager::drawOptionsMenuCommon(){
	common::getImage(IMAGE_DIALOG)->draw_scale(0, optionsAnimState, 480, 140);
	common::getImage(IMAGE_GAME)->draw(30, optionsAnimState+10);
	common::getImage(IMAGE_FTP)->draw(190, optionsAnimState+10);
	common::getImage(IMAGE_BROWSER)->draw(350, optionsAnimState+10);
}

void GameManager::drawOptionsMenu(){

	switch (optionsDrawState){
		case -1: return; // don't draw
		case 1: // draw opening animation
			drawOptionsMenuCommon();
			optionsAnimState += 20;
			if (optionsAnimState > 0)
				optionsDrawState = 2;
			break;
		case 2: // draw menu
			optionsAnimState = 0;
			drawOptionsMenuCommon();
			if (optionsTextAnim >= 0){
				int x = (pEntries[pEntryIndex].x + pEntries[optionsTextAnim].x)/2;
				common::printText(x, pEntries[pEntryIndex].y, pEntries[optionsTextAnim].name, LITEGRAY, SIZE_BIG, true);
				optionsTextAnim = -1;
			}
			else
				common::printText(pEntries[pEntryIndex].x, pEntries[pEntryIndex].y, pEntries[pEntryIndex].name, LITEGRAY, SIZE_BIG, true);
			break;
		case 3: // draw closing animation
			drawOptionsMenuCommon();
			optionsAnimState -= 20;
			if (optionsAnimState < -120)
				optionsDrawState = -1;
			break;
	}
}
		
void GameManager::drawBattery(){

	if (scePowerIsBatteryExist()) {
		int percent = scePowerGetBatteryLifePercent();
		
		
		if (percent < 0)
			return;

		u32 color;

		if (percent == 100)
			color = GREEN;
		else if (percent >= 17)
			color = LITEGRAY;
		else
			color = RED;

		ya2d_draw_rect(455, 6, 20, 8, color, 0);
		ya2d_draw_rect(454, 8, 1, 5, color, 1);
		ya2d_draw_pixel(475, 14, color);
		
		if (percent >= 5){
			int width = percent*17/100;
			ya2d_draw_rect(457+(17-width), 8, width, 5, color, 1);
		}
	}
}

void GameManager::MenuPopup(){

	common::playMenuSound();
	
	optionsTextAnim = -1;
	
	optionsAnimState = -120;
	optionsDrawState = 1;
	while (optionsDrawState != 2)
		sceKernelDelayThread(0);

	bool running = true;
	Controller control;

	while (running){
		control.update();

		if (control.accept()){
			common::playMenuSound();
			this->pauseIcons();
			this->drawMode = pEntryIndex;
			switch (pEntryIndex){
			case 0: running = false; break;
			case 1: ftpStart(common::getArgc(), common::getArgv()); break;
			case 2: browser->run(); break;
			}
			this->drawMode = 0;
			this->resumeIcons();
			control.update();
		}		
		else if (control.decline()){
			common::playMenuSound();
			break;
		}
		else if (control.left()){
			int oldIndex = pEntryIndex;
			int newIndex = pEntryIndex;
			common::playMenuSound();
			if (newIndex > 0)
				newIndex--;
			else
				newIndex = 2;
			pEntryIndex = newIndex;
			optionsTextAnim = oldIndex;
		}
		else if (control.right()){
			int oldIndex = pEntryIndex;
			int newIndex = pEntryIndex;
			common::playMenuSound();
			if (newIndex < 2)
				newIndex++;
			else
				newIndex = 0;
			pEntryIndex = newIndex;
			optionsTextAnim = oldIndex;
		}
	}

	optionsAnimState = 0;
	optionsDrawState = 3;
	while (optionsDrawState != -1)
		sceKernelDelayThread(0);
}

void GameManager::animAppear(){
	for (int i=480; i>=0; i-=40){
		common::clearScreen(CLEAR_COLOR);
		this->draw();
		Image* pic1 = this->getEntry()->getPic1();
		bool canDrawBg = !animations[common::getConf()->animation]->drawBackground();
		if (pic1 != common::getImage(IMAGE_BG) || canDrawBg)
			pic1->draw(i, 0);
		Image* pic0 = this->getEntry()->getPic0();
		if (pic0 != NULL)
			pic1->draw(i+160, 85);
		this->getEntry()->getIcon()->draw(i+10, 98);
		common::flipScreen();
	}
}

void GameManager::animDisappear(){
	for (int i=0; i<=480; i+=40){
		common::clearScreen(CLEAR_COLOR);
		this->draw();
		Image* pic1 = this->getEntry()->getPic1();
		bool canDrawBg = !animations[common::getConf()->animation]->drawBackground();
		if (pic1 != common::getImage(IMAGE_BG) || canDrawBg)
			pic1->draw(i, 0);
		Image* pic0 = this->getEntry()->getPic0();
		if (pic0 != NULL)
			pic1->draw(i+160, 85);
		this->getEntry()->getIcon()->draw(i+10, 98);
		common::flipScreen();
	}
}

void GameManager::endAllThreads(){
	dynamicIconRunning = false;
	sceKernelWaitThreadEnd(iconThread, 0);
	keepDrawing = false;
	sceKernelWaitThreadEnd(drawThreadID, 0);
}

void GameManager::run(){

	this->hasLoaded = true;

	Controller control;
	while(true){

		control.update();
		
		if (control.down()){
			this->moveDown();
		}
		else if (control.up()){
			this->moveUp();
		}
		else {
			this->stopFastScroll();
		}
		
		if (control.left())
			this->moveLeft();
		else if (control.right())
			this->moveRight();
		else if (control.accept()){
			if (selectedCategory >= 0){
				if (string(this->getEntry()->getType()) == string("ZIP"))
					this->extractHomebrew();
				else
					this->execApp();
			}
		}
		else if (control.start()){
			vshmenu->getPluginsManager()->writeFiles(this->getEntry());
			this->waitIconsLoad(true);
			this->endAllThreads();
			if (selectedCategory >= 0)
				this->getEntry()->execute();
			break;
		}
		else if (control.triangle()){
			MenuPopup();
		}
		else if (control.decline()){
			this->waitIconsLoad(true);
			this->endAllThreads();
			break;
		}
		else if (control.select()){
			common::playMenuSound();
			this->drawVSH = true;
			this->vshmenu->control(this->getEntry());
			this->drawVSH = false;
		}
		if (GameManager::update_game_list){
			//this->pauseDraw();
			this->hasLoaded = false;
			this->pauseIcons();
			this->findEntries();
			GameManager::update_game_list = false;
			this->hasLoaded = true;
			//this->resumeDraw();
			this->resumeIcons();
		}
	}
}

void GameManager::execApp(){
	this->waitIconsLoad();			
	if (common::getConf()->fast_gameboot){
		this->endAllThreads();
		this->getEntry()->execute();
	}
					
	this->pauseIcons();
	loadingData = true;
	this->getEntry()->getTempData1();
	this->pauseDraw();
	loadingData = false;
	animAppear();
	if (this->getEntry()->run()){
		this->resumeIcons();
		this->resumeDraw();
		vshmenu->getPluginsManager()->writeFiles(this->getEntry());
		this->waitIconsLoad(true);
		this->endAllThreads();
		this->getEntry()->execute();
	}
	animDisappear();
	this->getEntry()->freeTempData();
	this->resumeDraw();
	this->resumeIcons();
	sceKernelDelayThread(0);
}

void GameManager::extractHomebrew(){
	string text[6] = {
		"Extracting",
		"    "+this->getEntry()->getPath(),
		"into",
		"    ms0:/PSP/GAME/",
		"    ",
		"please wait..."
	};
	this->drawVSH = true;
	this->vshmenu->setCustomText(text, 6);
	unzipToDir(this->getEntry()->getPath().c_str(), (char*)"ms0:/PSP/GAME/", NULL);
	this->vshmenu->unsetCustomText();
	this->drawVSH = false;
	GameManager::update_game_list = true;
}
