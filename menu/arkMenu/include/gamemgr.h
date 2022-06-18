#ifndef GAMEMGR_H
#define GAMEMGR_H

#include <cstdlib>
#include <dirent.h>
#include <cstdio>
#include <string>
#include <cstring>
#include "menu.h"
#include "controller.h"
#include "gfx.h"
#include "animations.h"
#include "browser.h"
#include "vshmenu.h"

#define MAX_CATEGORIES 3

using namespace std;

class GameManager{

	private:
	
		/* Array of game menus */
		CatMenu* categories[MAX_CATEGORIES];
		
		/* Selected game menu */
		int selectedCategory;
		
		/* Instance of the animations that are drawn on the menu */
		Anim* animations[ANIM_COUNT];
		
		/* Instance of the file browser */
		Browser* browser;
		
		/* Instance of the VSH Menu */
		VSHMenu* vshmenu;
		
		// is VSH Menu running?
		bool drawVSH;
		
		/* Multithreading variables */
		SceUID iconThread; // UID's of the icon thread
		SceUID iconSema; // semaphore to lock the thread when sleeping
		bool dynamicIconRunning;
		/* Control the icon threads */
		void pauseIcons();
		void resumeIcons();
		bool waitIconsLoad(bool forceQuit=false);
		
		/* Screen drawing thread data */
		SceUID drawThreadID; // ID of the thread
		SceUID drawSema; // semaphore to lock the thread when sleeping
		bool keepDrawing; // this tells the draw thread to continue drawing or to terminate
		bool hasLoaded; // whether the main thread has finished loading or not, if not then only draw the background and animation
		int drawMode; // 0 for Game, 1 for FTP (draws itself), 2 for Browser
		
		void endAllThreads();
		
		/* State of the options Menu drawing function and animation state
			optionsDrawState has 4 possible values
			-1: don't draw
			0: draw popup animation
			1: draw menu
			2: draw popout animation
		*/
		int optionsDrawState;
		int optionsAnimState; // state of the animation
		int optionsTextAnim; // -1 for no animation, other for animation
	
		// options menu entries possition of the entries
		int pEntryIndex;
		
		// Entry animation
		void animAppear();
		void animDisappear();
		
		/* find all available menu entries
			ms0:/PSP/GAME for eboots
			ms0:/ISO for ISOs
			ms0:/PSP/SAVEDATA for both
		 */
		void findEntries();
		void findEboots();
		void findISOs();
		void findSaveEntries();

		/* move the menu in the specified direction */
		void moveLeft();
		void moveRight();
		void moveUp();
		void moveDown();
		void stopFastScroll();
		
		int getNextCategory(int current);
		int getPreviousCategory(int current);
		
		void execApp();
		void extractHomebrew();
	
	public:
	
		GameManager();
		~GameManager();
		
		/* thread to load icon0 in the background */
		static int loadIcons(SceSize _args, void *_argp);
		
		/* thread that draws the screen */
		static int drawThread(SceSize _args, void *_argp);
		
		void pauseDraw();
		
		void resumeDraw();
		
		/* obtain the currently selected entry */
		Entry* getEntry();
		
		/* draw all three menus */
		void draw();
		/* clears the screen and updates it with the menus and text */
		void updateScreen();
		/* draw the options menu based on its state */
		void drawOptionsMenu();
		void drawOptionsMenuCommon();
		/* draw the battery state */
		void drawBattery();
		
		/* Popup Menu */
		void MenuPopup();
		
		/* get a specific category menu */
		CatMenu* getMenu(EntryType t);
		
		/* control the menus */
		void run();
		
		static void installPlugin(string path);
		
		static bool update_game_list;
		
};

#endif
