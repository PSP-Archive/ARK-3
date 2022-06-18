#include <cstdio>
#include <sstream>
#include <dirent.h>

#include <pspiofilemgr.h>

#include "browser.h"
#include "gamemgr.h"
#include "osk.h"

#include "iso.h"
#include "cso.h"
#include "eboot.h"
#include "zip.h"

#define INITIAL_DIR "ms0:/"
#define PAGE_SIZE 10

#include "browser_entries.h"

static Browser* self;

static const struct {
		int x;
		int y;
		char* name;
} pEntries[9] = {
	{10, 40, "Cancel"},
	{10, 60, "Copy"},
	{10, 80, "Cut"},
	{10, 100, "Paste"},
	{10, 120, "Delete"},
	{10, 140, "Rename"},
	{10, 160, "New Dir"},
	{10, 180, "Send"},
	{10, 200, "Receive"},
};

Browser::Browser(){
	self = this;
	this->cwd = INITIAL_DIR; // current working directory (cwd)
	this->entries = new vector<Entry*>(); // list of files and folders in cwd
	this->pasteMode = NO_MODE;
	this->index = 0;
	this->start = 0;
	this->animating = false;
	this->enableSelection = true;
	this->selectedBuffer = new vector<string>(); // list of paths to paste
	this->draw_progress = false;
	this->optionsmenu = NULL;
	this->checkBox = new Image(PKG_PATH, YA2D_PLACE_RAM, common::findPkgOffset("CHECK.PNG"));
	this->uncheckBox = new Image(PKG_PATH, YA2D_PLACE_RAM, common::findPkgOffset("UNCHECK.PNG"));
	this->folderIcon = new Image(PKG_PATH, YA2D_PLACE_RAM, common::findPkgOffset("FOLDER.PNG"));
	this->fileIcon = new Image(PKG_PATH, YA2D_PLACE_RAM, common::findPkgOffset("FILE.PNG"));
	
	this->keepDrawing = true;
	this->enableDraw = false;
	this->drawThreadWaiting = false;
	this->optionsDrawState = -1;
	this->optionsAnimState = 0;
	this->pEntryIndex = 0;
	this->drawThreadID = -1;
}

Browser::~Browser(){
	this->entries->clear();
	this->selectedBuffer->clear();
	delete this->entries;
	delete this->selectedBuffer;
	this->keepDrawing = false;
	sceKernelWaitThreadEnd(this->drawThreadID, 0);
}

void Browser::setDrawThreadID(SceUID id){
	this->drawThreadID = id;
}

void Browser::run(){
	// Run the menu
	if (this->drawThreadID < 0)
		return;
	this->refreshDirs();
	this->control();
}

void Browser::moveDirUp(){
	// Move to the parent directory of this->cwd
	if (this->cwd == INITIAL_DIR)
		return;
	size_t lastSlash = this->cwd.rfind("/", this->cwd.rfind("/", string::npos)-1);
	this->cwd = this->cwd.substr(0, lastSlash+1);
	this->refreshDirs();
}
		
void Browser::update(){
	// Move to the next directory pointed by the currently selected entry or run an app if selected file is one
	if (this->entries->size() == 0)
		return;
	common::playMenuSound();
	if (this->get()->getName() == "./")
		refreshDirs();
	else if (this->get()->getName() == "../")
		moveDirUp();
	else if (string(this->get()->getType()) == "FOLDER"){
		this->cwd = this->get()->getPath();
		this->refreshDirs();
	}
	else if (Iso::isISO(this->get()->getPath().c_str())){
		this->stopDraw();
		Iso* iso = new Iso(this->get()->getPath());
		iso->execute();
	}
	else if (Cso::isCSO(this->get()->getPath().c_str())){
		this->stopDraw();
		Cso* cso = new Cso(this->get()->getPath());
		cso->execute();
	}
	else if (Eboot::isEboot(this->get()->getPath().c_str())){
		this->stopDraw();
		Eboot* eboot = new Eboot(this->get()->getPath());
		eboot->execute();
	}
	else if (Zip::isZip(this->get()->getPath().c_str())){
		extractArchive(0);
	}
	else if (Zip::isRar(this->get()->getPath().c_str())){
		extractArchive(1);
	}
	else if (common::getExtension(this->get()->getPath()) == string("prx")){
		progress_desc[0] = "Installing plugin";
		progress_desc[1] = "    "+this->get()->getPath();
		progress_desc[2] = "";
		progress_desc[3] = "";
		progress_desc[4] = "Please Wait";
		draw_progress = true;
		GameManager::installPlugin(this->get()->getPath());
		sceKernelDelayThread(100000);
		draw_progress = false;
	}
}

void Browser::extractArchive(int type){

	string name = this->get()->getName();
	string dest = this->cwd + name.substr(0, name.rfind('.')) + "/";
	sceIoMkdir(dest.c_str(), 0777);

	progress_desc[0] = "Extracting archive";
	progress_desc[1] = "    "+name;
	progress_desc[2] = "into";
	progress_desc[3] = "    "+dest;
	progress_desc[4] = "Please Wait";
	
	bool noRedraw = draw_progress;
	if (!noRedraw)
		draw_progress = true;
	
	if (type)
		DoExtractRAR(this->get()->getPath().c_str(), dest.c_str(), NULL);
	else
		unzipToDir(this->get()->getPath().c_str(), dest.c_str(), NULL);
	
	if (!noRedraw)
		draw_progress = false;
	
	this->refreshDirs();
	
	if (!strncmp(dest.c_str(), "ms0:/PSP/GAME/", 14) || !strncmp(dest.c_str(), "ms0:/ISO/", 9))
		GameManager::update_game_list = true; // if something has been done in ms0:/PSP/GAME or ms0:/ISO, tell GameManager
}

void Browser::refreshDirs(){
	// Refresh the list of files and dirs
	this->enableDraw = false;
	while (!this->drawThreadWaiting)
		sceKernelDelayThread(0);
	this->index = 0;
	this->start = 0;
	this->entries->clear();
	this->animating = false;
	this->draw_progress = false;
	this->optionsmenu = NULL;

	struct dirent* dit;
	DIR* dir = opendir(this->cwd.c_str());
	
	if (dir == NULL){
		this->cwd = INITIAL_DIR;
		refreshDirs();
		return;
	}
		
	vector<Entry*>* folders = new vector<Entry*>();
	vector<Entry*>* files = new vector<Entry*>();
		
	while ((dit = readdir(dir))){
	
		if (common::fileExists(string(this->cwd)+string(dit->d_name)))
			files->push_back(new File(string(this->cwd)+string(dit->d_name)));
		else
			folders->push_back(new Folder(string(this->cwd)+string(dit->d_name)+"/"));
	}
	closedir(dir);
	
	for (int i=0; i<folders->size(); i++)
		entries->push_back(folders->at(i));
	for (int i=0; i<files->size(); i++)
		entries->push_back(files->at(i));
	
	delete folders;
	delete files;
	
	if (this->entries->size() == 0){
		this->cwd = INITIAL_DIR;
		refreshDirs();
		return;
	}
	
	this->enableDraw = true;
	while (this->drawThreadWaiting)
		sceKernelDelayThread(0);
}
		

void Browser::drawScreen(){

		common::getImage(IMAGE_BG)->draw(0, 0);

		const int xoffset = 165;
		int yoffset = 30;
		
		for (int i=this->start; i<min(this->start+PAGE_SIZE, (int)entries->size()); i++){
			File* e = (File*)this->entries->at(i);
			
			if(e->isSelected()){
				this->checkBox->draw(xoffset-30, yoffset-10);
			}else{
				this->uncheckBox->draw(xoffset-30, yoffset-10);
			}if (e == this->get() && this->enableSelection){
				if (animating){
					common::printText(xoffset, yoffset, e->getName().c_str(), LITEGRAY, SIZE_MEDIUM, true, true);
					animating = false;
				}
				else
					common::printText(xoffset, yoffset, e->getName().c_str(), LITEGRAY, SIZE_BIG, true, true);
			}
			else{
				common::printText(xoffset, yoffset, this->formatText(e->getName()).c_str());
			}
			common::printText(400, yoffset, e->getSize().c_str());
			if (string(e->getType()) == "FOLDER")
				this->folderIcon->draw(xoffset-15, yoffset-10);
			else
				this->fileIcon->draw(xoffset-15, yoffset-10);
			yoffset += 20;
		}
		ostringstream stream;
		stream<<"Number of selected items: ";
		stream<<this->selectedBuffer->size();
		common::printText(10, 245, stream.str().c_str());
		common::printText(10, 260, this->cwd.c_str());
}

void Browser::drawProgress(){
	if (!draw_progress)
		return;
	
	if (progress_desc[4] == ""){
		ostringstream s;
		s<<progress<<" / "<<max_progress;	
		progress_desc[4] = s.str();
	}
		
	int w = min(480, 10*common::maxString(progress_desc, 5));
	int h = 30 + 15*4;
	int x = (480-w)/2;
	int y = (272-h)/2;
	common::getImage(IMAGE_DIALOG)->draw_scale(x, y, w, h);
	
	int yoffset = y+10;
	for (int i=0; i<5; i++){
		common::printText(x+20, yoffset, progress_desc[i].c_str());
		yoffset+=15;
	}
	
}
	

bool Browser::drawThread(){
	if (self->keepDrawing){
		while (!this->enableDraw){
			if (!this->keepDrawing){
				sceKernelExitDeleteThread(0);
				return 0;
			}
			this->drawThreadWaiting = true;
			sceKernelDelayThread(100000);
		}
		this->drawThreadWaiting = false;
		common::clearScreen(CLEAR_COLOR);
		this->drawScreen();
		this->drawOptionsMenu();
		this->drawProgress();
		if (this->optionsmenu != NULL)
			this->optionsmenu->draw();
		common::flipScreen();
		sceKernelDelayThread(0);
	}
	return this->keepDrawing;
}

void Browser::pauseDraw(){
	this->enableDraw = false;
	while (!this->drawThreadWaiting)
		sceKernelDelayThread(0);
}

void Browser::resumeDraw(){
	this->enableDraw = true;
	while (this->drawThreadWaiting)
		sceKernelDelayThread(0);
}

void Browser::stopDraw(){
	this->keepDrawing = false;
	sceKernelWaitThreadEnd(this->drawThreadID, NULL);
}

string Browser::formatText(string text){
	// Format the text shown, text with more than 13 characters will be truncated and ... be appended to the name
	if (text.length() <= 25)
		return text;
	else{
		string* ret = new string(text.substr(0, 22));
		*ret += "...";
		return *ret;
	}
}
		
void Browser::select(){
	// Select or unselect the entry pointed by the cursor
	if (this->entries->size() == 0)
		return;
	Folder* e = (Folder*)this->get();
	if (string(e->getName()) == "./")
		return;
	else if (string(e->getName()) == "../")
		return;
	e->changeSelection();
}

Entry* Browser::get(){
	// Obtain the currectly selected entry, this will return the instance of the entry, not it's name
	return this->entries->at(this->index);
}
		
void Browser::down(){
	// Move the cursor down, this updates index and page
	if (this->entries->size() == 0)
		return;
	if (this->index == (entries->size()-1))
		return;
	else if (this->index-this->start == PAGE_SIZE-1){
		if (this->index+1 < entries->size())
			this->index++;
		if (this->start+PAGE_SIZE < entries->size())
			this->start++;
	}
	else if (this->index+1 < entries->size())
		this->index++;
	this->animating = true;
	common::playMenuSound();
}
		
void Browser::up(){
	// Move the cursor up, this updates index and page
	if (this->entries->size() == 0)
		return;
	if (this->index == 0)
		return;
	else if (this->index == this->start){
		this->index--;
		if (this->start>0)
			this->start--;
	}
	else
		this->index--;
	this->animating = true;
	common::playMenuSound();
}

void Browser::deleteFolder(string path){
	// Recursively delete the path
	
	progress_desc[0] = "Deleting folder";
	progress_desc[1] = "    "+path;
	progress_desc[2] = "";
	progress_desc[3] = "";
	progress_desc[4] = "Please Wait";
	
	bool noRedraw = draw_progress;
	if (!noRedraw)
		draw_progress = true;
	
	 //protect some folders
	if(path == string("ms0:/PSP/") || path == string("ms0:/PSP/GAME/") || path == string("ms0:/PSP/LICENSE/"))
		return;

	//try to open directory
	SceUID d = sceIoDopen(path.c_str());
	
	if(d >= 0)
	{
		SceIoDirent entry;
		memset(&entry, 0, sizeof(SceIoDirent));
		
		//allocate memory to store the full file paths
		char * new_path = new char[path.length() + 256];

		//start reading directory entries
		while(sceIoDread(d, &entry) > 0)
		{
			//skip . and .. entries
			if (!strcmp(".", entry.d_name) || !strcmp("..", entry.d_name)) 
			{
				memset(&entry, 0, sizeof(SceIoDirent));
				continue;
			};
			
			//build new file path
			strcpy(new_path, path.c_str());
			strcat(new_path, entry.d_name);
			
			if(common::fileExists(new_path))
				deleteFile(new_path);
			else {
				//not a file? must be a folder
				strcat(new_path, "/");
				deleteFolder(string(new_path)); //try to delete folder content
			}
			
		};
		
		sceIoDclose(d); //close directory
		sceIoRmdir(path.substr(0, path.length()-1).c_str()); //delete empty folder

		delete [] new_path; //clear allocated memory
	};
	if (!noRedraw)
		draw_progress = false;
}

void Browser::deleteFile(string path){
	progress_desc[0] = "Deleting file";
	progress_desc[1] = "    "+path;
	progress_desc[2] = "";
	progress_desc[3] = "";
	progress_desc[4] = "Please Wait";
	
	bool noRedraw = draw_progress;
	if (!noRedraw)
		draw_progress = true;
	
	sceIoRemove(path.c_str());
	
	if (!noRedraw)
		draw_progress = false;
}

int Browser::copy_folder_recursive(const char * source, const char * destination)
{
	//create new folder
	sceIoMkdir(destination, 0777);
	
	char * new_destination = new char[strlen(destination) + 256];
	strcpy(new_destination, destination);
	strcat(new_destination, "/");
	
	char* entry_copy = new_destination + strlen(destination) + 1;
	
	//try to open source folder
	SceUID dir = sceIoDopen(source);
	
	if(dir >= 0)
	{
		SceIoDirent entry;
		memset(&entry, 0, sizeof(SceIoDirent));
		
		char * read_path = new char[strlen(source) + 256];
		
		//start reading directory entries
		while(sceIoDread(dir, &entry) > 0)
		{
			//skip . and .. entries
			if (!strcmp(".", entry.d_name) || !strcmp("..", entry.d_name)) 
			{
				memset(&entry, 0, sizeof(SceIoDirent));
				continue;
			};
		
			//build read path
			strcpy(read_path, source);
			strcat(read_path, "/");
			strcat(read_path, entry.d_name);
		
			//pspDebugScreenPrintf("file copy from: %s\n", read_path);
		
			strcpy(entry_copy, entry.d_name); //new file

			//pspDebugScreenPrintf("to %s\n", new_destination);
			
			if (common::fileExists(read_path)) //is it a file
				copyFile(string(read_path), string(destination)+string("/")); //copy file
			else
			{
				//try to copy as a folder
				//strcat(new_destination, "/");
				//strcat(read_path, "/");
				copy_folder_recursive(read_path, new_destination);
			};

		};
		
		delete [] read_path;
		
		//close folder
		sceIoDclose(dir);
	};
	
	//free allocated path
	delete [] new_destination;
	
	return 1;
};

void Browser::copyFolder(string path){
	// Copy the folder into cwd

	if(path == this->cwd)
		return;
	
	if(!strncmp(path.c_str(), this->cwd.c_str(), path.length())) //avoid inception
		return;
	
	Folder* f = new Folder(path);
	
	string destination = this->cwd + f->getName().substr(0, f->getName().length()-1);
	
	copy_folder_recursive(path.substr(0, path.length()-1).c_str(), destination.c_str());
}

void Browser::copyFile(string path, string destination){
	// Copy file into cwd
	
	size_t lastSlash = path.rfind("/", string::npos);
	string name = path.substr(lastSlash+1, string::npos);
	string dest = destination + name;
	
	check_destination:
	if (common::fileExists(dest)){
		char* description = "Destination file exists, what to do?";
		t_options_entry options_entries[3] = {
			{OPTIONS_CANCELLED, "Cancel"}, {1, "Rename destination"}, {0, "Overwrite"}
		};
		optionsmenu = new OptionsMenu(description, (path == dest)? 2 : 3, options_entries);
		int ret = optionsmenu->control();
		
		OptionsMenu* aux = optionsmenu;
		optionsmenu = NULL;
		delete aux;
		
		switch (ret){
		case OPTIONS_CANCELLED: return; break;
		case 0: sceIoRemove(dest.c_str()); break;
		case 1:
			{
				this->pauseDraw();
				OSK osk;
				osk.init("New name for destination", name.c_str(), 50);
				osk.loop();
				if(osk.getResult() != OSK_CANCEL)
				{
					char tmpText[51];
					osk.getText((char*)tmpText);
					string dirName = string(tmpText);
					dest = destination + string(tmpText);
				}
				osk.end();
				this->resumeDraw();
				goto check_destination;
			}
			break;
		default: return; break;
		}
	}
	
	SceUID src = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0777);
	SceUID dst = sceIoOpen(dest.c_str(), PSP_O_WRONLY | PSP_O_CREAT, 0777);
	
	progress_desc[0] = "Copying file";
	progress_desc[1] = "    "+path;
	progress_desc[2] = "into";
	progress_desc[3] = "    "+dest;
	progress_desc[4] = "";
	
	progress = 0;
	max_progress = sceIoLseek(src, 0, SEEK_END);
	sceIoLseek(src, 0, SEEK_SET);

	bool noRedraw = draw_progress;
	if (!noRedraw)	
		draw_progress = true;
	
	int read;
	u8* buffer = new u8[1024*16]; // 16 kB buffer
	
	do {
		read = sceIoRead(src, buffer, sizeof(buffer));
		sceIoWrite(dst, buffer, read);
		progress += read;
	} while (read);
	sceIoClose(src);
	sceIoClose(dst);
	delete buffer;
	
	if (!noRedraw)
		draw_progress = false;
}

void Browser::copyFile(string path){
	copyFile(path, this->cwd);
}

void Browser::fillSelectedBuffer(){
	this->selectedBuffer->clear();
	for (int i=0; i<entries->size(); i++){
		File* e = (File*)entries->at(i);
		if (e->isSelected())
			this->selectedBuffer->push_back(e->getPath());
	}
}

void Browser::copy(){
	// Mark the paste mode as copy
	this->pasteMode = COPY;
	this->fillSelectedBuffer();
}

void Browser::cut(){
	// Mark the paste mode as cut
	this->pasteMode = CUT;
	this->fillSelectedBuffer();
}

void Browser::paste(){
	// Copy or cut all paths in the paste buffer to the cwd
	for (int i = 0; i<selectedBuffer->size(); i++){
		string e = selectedBuffer->at(i);
		if (common::fileExists(e)){
			this->copyFile(e);
			if (pasteMode == CUT) sceIoRemove(e.c_str());
		}
		else {
			this->copyFolder(e);
			if (pasteMode == CUT) this->deleteFolder(e);
		}
	}
	this->selectedBuffer->clear();
}

void Browser::rename(){
	this->pauseDraw();
	string name = this->get()->getName();
	OSK osk;
	char* oldname = (char*)malloc(name.length());
	if (name.at(name.length()-1) == '/')
		strcpy(oldname, name.substr(0, name.length()-1).c_str());
	else
		strcpy(oldname, name.c_str());
	
	osk.init("New name for file/folder", oldname, 50);
	osk.loop();
	if(osk.getResult() != OSK_CANCEL)
	{
		char tmpText[51];
		osk.getText((char*)tmpText);
		sceIoRename((this->cwd+string(oldname)).c_str(), (this->cwd+string(tmpText)).c_str());
	}
	osk.end();
	free(oldname);
	this->resumeDraw();
}

void Browser::send(){}
void Browser::recv(){}

void Browser::removeSelection(){
	// Delete all paths in the paste buffer
	this->fillSelectedBuffer();
	if (this->selectedBuffer->size() == 0)
		this->selectedBuffer->push_back(this->get()->getPath());
		
	if (this->selectedBuffer->size() > 0){
		for (int i = 0; i<selectedBuffer->size(); i++){
			string e = selectedBuffer->at(i);
			if (common::fileExists(e))
				deleteFile(e);
			else
				this->deleteFolder(e);
		}
		this->selectedBuffer->clear();
	}
}

void Browser::makedir(){
	this->pauseDraw();
	OSK osk;
	osk.init("Name of new directory", "new dir", 50);
	osk.loop();
	if(osk.getResult() != OSK_CANCEL)
	{
		char tmpText[51];
		osk.getText((char*)tmpText);
		string dirName = string(tmpText);
		sceIoMkdir((this->cwd+dirName).c_str(), 0777);
		this->refreshDirs();
	}
	osk.end();
	this->resumeDraw();
}

void Browser::drawOptionsMenu(){

	switch (optionsDrawState){
		case -1:
			common::getImage(IMAGE_DIALOG)->draw_scale(0, 10, 12, 220);
			break;
		case 1: // draw opening animation
			common::getImage(IMAGE_DIALOG)->draw_scale(optionsAnimState, 10, 132, 220);
			optionsAnimState += 20;
			if (optionsAnimState > 0)
				optionsDrawState = 2;
			break;
		case 2: // draw menu
			optionsAnimState = 0;
			common::getImage(IMAGE_DIALOG)->draw_scale(0, 10, 132, 220);
		
			for (int i=0; i<9; i++){
				if (i == pEntryIndex)
					common::printText(pEntries[i].x, pEntries[i].y, pEntries[i].name, LITEGRAY, SIZE_BIG, true);
				else
					common::printText(pEntries[i].x, pEntries[i].y, pEntries[i].name);
			}
			break;
		case 3: // draw closing animation
			common::getImage(IMAGE_DIALOG)->draw_scale(optionsAnimState, 10, 132, 220);
			optionsAnimState -= 20;
			if (optionsAnimState < -120)
				optionsDrawState = -1;
			break;
	}
}

void Browser::optionsMenu(){
	
	this->enableSelection = false;

	optionsAnimState = -100;
	optionsDrawState = 1;
	while (optionsDrawState != 2)
		sceKernelDelayThread(0);

	Controller pad;
	
	while (true){
		
		pad.update();
		
		if (pad.down()){
			if (pEntryIndex < 8){
				common::playMenuSound();
				pEntryIndex++;
			}
		}
		else if (pad.up()){
			if (pEntryIndex > 0){		
				common::playMenuSound();
				pEntryIndex--;
			}
		}

		else if (pad.decline()){
			pEntryIndex = 0;
			break;
		}
		else if (pad.accept())
			break;
	}
	
	common::playMenuSound();
	
	optionsAnimState = 0;
	optionsDrawState = 3;
	while (optionsDrawState != -1)
		sceKernelDelayThread(0);
	
	this->enableSelection = true;

	sceKernelDelayThread(100000);
}

void Browser::options(){
	// Run the system menu with the available browser options
	this->pEntryIndex = 0;
	this->optionsMenu();

	switch (pEntryIndex){
	case NO_MODE:                                 break;
	case COPY:        this->copy();               break;
	case CUT:         this->cut();                break;
	case PASTE:       this->paste();              break;
	case DELETE:      this->removeSelection();    break;
	case RENAME:      this->rename();             break;
	case MKDIR:       this->makedir();            break;
	case SEND:        this->send();               break;
	case RECEIVE:     this->recv();               break;
	}
	
	if (pEntryIndex != NO_MODE && pEntryIndex != COPY && pEntryIndex != CUT){
		this->refreshDirs();
		if (!strncmp(cwd.c_str(), "ms0:/PSP/GAME/", 14) || !strncmp(cwd.c_str(), "ms0:/ISO/", 9))
			GameManager::update_game_list = true; // if something has been done in ms0:/PSP/GAME or ms0:/ISO, tell GameManager
	}
}
		
void Browser::control(){
	// Control the menu through user input
	
	Controller pad;
	while (true){
		
		pad.update();

		if (pad.up()){
			this->up();
			continue;
		}
		else if (pad.down()){
			this->down();
			continue;
		}
		
		else if (pad.accept())
			this->update();
		else if (pad.decline()){
			common::playMenuSound();
			break;
		}
		else if (pad.square()){
			common::playMenuSound();
			this->select();
		}
		else if (pad.triangle()){
			common::playMenuSound();
			this->options();
		}
	}
	
	sceKernelDelayThread(100000);
}
