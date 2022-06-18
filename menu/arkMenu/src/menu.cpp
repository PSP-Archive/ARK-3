/* Category Class that defines each of the individual menus */

#include "menu.h"
#include "mp3.h"

static SceUID iconSema = -1;

CatMenu::CatMenu(EntryType t){
	this->type = t;
	this->index = 0;
	this->threadIndex = 0;
	this->animating = 0;
	this->fastScroll = 1;
	this->fastScrolling = false;
	this->animDelay = true;
	this->animState = 0.f;
	this->initLoad = false;
	this->stopLoading = false;
	this->entries = new vector<Entry*>();
	if (iconSema < 0)
		iconSema = sceKernelCreateSema("icon_sema",  0, 1, 1, NULL);
}


CatMenu::~CatMenu(){
	this->clearEntries();
	delete this->entries;
}

void CatMenu::freeIcons(){
	for (int i = 0; i < this->threadIndex-5; i++)
		this->getEntry(i)->freeIcon();

	for (int i = this->threadIndex+6; i<this->getVectorSize(); i++)
		this->getEntry(i)->freeIcon();
}

bool CatMenu::checkIconsNeeded(bool isSelected){
	int umbral = (isSelected)? 2 : 5;
	for (int i = max(this->threadIndex-umbral, 0); i < min((int)this->getVectorSize(), this->threadIndex+umbral+1); i++){
		if (this->getEntry(i)->getIcon() == common::getImage(IMAGE_WAITICON))
			return true;
	}
	return false;
}

void CatMenu::loadIconsDynamic(bool isSelected){

	if (this->fastScrolling || this->getVectorSize() == 0 || stopLoading)
		return; // we don't need to load any icons

	sceKernelWaitSema(iconSema, 1, NULL);

	this->stopLoading = false;
	this->threadIndex = this->index; // prevents our working index from changing
	freeIcons(); // delete any icon that won't be loaded
	if (!checkIconsNeeded(isSelected)){ // check if we need to load icons
		sceKernelSignalSema(iconSema, 1);
		return;
	}
	// start loading the needed icons
	for (int i = max(0, this->threadIndex-5); i < min((int)this->getVectorSize(), this->threadIndex+6); i++){
		if (this->index != this->threadIndex || this->stopLoading)
			break; // stop loading the icons if the state of the menu has changed
		if (this->getEntry(i)->getIcon() == common::getImage(IMAGE_WAITICON))
			this->getEntry(i)->loadIcon();
	}
	initLoad = !this->stopLoading;
	sceKernelSignalSema(iconSema, 1);
}

bool CatMenu::waitIconsLoad(bool isSelected, bool forceQuit){
	this->stopLoading = forceQuit;
	sceKernelWaitSema(iconSema, 1, NULL); // wait for the thread to release the semaphore
	if (this->getEntry()->getIcon() == common::getImage(IMAGE_WAITICON))
		this->getEntry()->loadIcon();
	sceKernelSignalSema(iconSema, 1);
	return true;
}

void CatMenu::resumeIconLoading(){
	this->stopLoading = false;
}

void CatMenu::draw(bool selected){

	if (this->getVectorSize() == 0)
		return;

	int xoffset = ((int)this->type)*144 + ((int)this->type+1)*10 + 5;
	int yoffset = 20;
	int curentry_yoffset = 0;
	
	float anim = 0.0f;
	if (animState >= 1.0f){
		if (animating == -1){
			if (this->index < this->getVectorSize()-1)
				this->index++;
			else
				animating = 0;
		}
		else if (animating == 1){
			if (this->index > 0)
				this->index--;
			else
				animating = 0;
		}
		if (fastScrolling){
			animState = 0.74f;
			animDelay = false;
		}
		else
			animating = 0;
	}
	
	if (animating){
		anim = (animating != 2)? animState : 0.f;
		if (!this->animDelay)
			animState += (animating == 2)? 0.5f : 0.25f;
		if (animating == -1)
			anim *= -1;
	}

	float scale = 1.f;
	if (animating){
		if (animState < 0.5f)
			scale = 0.5f;
		else if (animState < 1.f)
			scale = 0.75f;
	}

	for (int i=this->index-2; yoffset<272; i++){
		if (i < 0){
			yoffset+=40;
			continue;
		}
		if (i >= this->getVectorSize())
			break;
			
		if (selected && i == this->index){
			curentry_yoffset = yoffset;
			if (animating)
				yoffset += 70;
			else
				yoffset += 90;
			continue;
		}
		else{
		
			if (animating == 1 && i == this->index-1 && !fastScrolling){
				getEntry(i)->getIcon()->draw_scale(xoffset+2, yoffset+anim*40, 0.75f, 0.75f);
				yoffset += 60;
			}
			else if (animating == -1 && i == this->index+1 && !fastScrolling){
				getEntry(i)->getIcon()->draw_scale(xoffset+2, yoffset+anim*40, 0.75f, 0.75f);
				yoffset += 60;
			}
			else{
				getEntry(i)->getIcon()->draw_scale(xoffset, yoffset+anim*40, 0.5f, 0.5f);
				yoffset += 40;
			}
		}
	}
	if (selected){
		int height = getEntry(this->index)->getIcon()->getTexture()->height;
		if (height != 80)
			curentry_yoffset = (272-height)/2;
		
		if (animating){
			getEntry(this->index)->getIcon()->draw_scale(xoffset+2, curentry_yoffset+5+anim*40, 0.75f, 0.75f);
		}
		else {
			getEntry(this->index)->getIcon()->draw(xoffset+5, curentry_yoffset+5+anim*40);
		}
		
		//getEntry(this->index)->getIcon()->draw_scale(xoffset+2, curentry_yoffset+5+anim*40, scale, scale);
	}
	/*
	if (animating && animating != 2 && !fastScrolling){
		Controller pad;
		pad.update();
		if (pad.up() && animating == 1 && this->animState >= 0.75f)  // the user might want to fast scroll
			this->animDelay = true;  // so let's delay the animation by redoing it
		else if (pad.down() && animating == -1 && this->animState >= 0.75f)
			this->animDelay = true;
		else if (pad.down() && animating == 1 && this->animState >= 0.75f){ // if the user presses the opposite button
			animating = 0;   // stop the animation
			fastScrolling = false;
		}
		else if (pad.up() && animating == -1 && this->animState >= 0.75f){
			animating = 0;
			fastScrolling = false;
		}
		else
			this->animDelay = false; // otherwise stop delaying the animation
	}
	else
		this->animDelay = false;
	*/
}

void CatMenu::animStart(int direction){
	animating = direction;
	animState = 0.f;
	animDelay = false;
}

bool CatMenu::empty(){
	bool ret = !this->entries->size();
	return ret;
}

void CatMenu::addEntry(Entry* e){
	this->entries->push_back(e);
}

Entry* CatMenu::getEntry(){
	return entries->at(this->index);
}

Entry* CatMenu::getEntry(int index){
	return entries->at(index);
}

void CatMenu::clearEntries(){
	this->entries->clear();
	this->index = 0;
	this->initLoad = false;
}

size_t CatMenu::getVectorSize(){
	size_t ret = this->entries->size();
	return ret;
}

vector<Entry*>* CatMenu::getVector(){
	return entries;
}

void CatMenu::moveUp(){
	if (animating || fastScrolling){
		//animating = 0;
		fastScrolling = true;
		this->index -= fastScroll;
		fastScroll++;
		if (this->index <= 0){
			animating = 0;
			this->index = 0;
			this->stopFastScroll();
		}
		else
			common::playMenuSound();
	}
	else if (this->index > 0){
		common::playMenuSound();
		this->stopFastScroll();
		animStart(1);
	}
	else
		this->stopFastScroll();
}

void CatMenu::moveDown(){
	if (animating || fastScrolling){
		//animating = 0;
		fastScrolling = true;
		this->index += fastScroll;
		fastScroll++;
		if (this->index >= this->getVectorSize()-1){
			animating = 0;
			this->index = this->getVectorSize()-1;
			this->stopFastScroll();
		}
		else
			common::playMenuSound();
	}
	else if (this->index < this->getVectorSize()-1){
		common::playMenuSound();
		this->stopFastScroll();
		animStart(-1);
	}
	else
		this->stopFastScroll();
}

void CatMenu::stopFastScroll(){
	fastScrolling = false;
	fastScroll = 1;
	animDelay = false;
}
