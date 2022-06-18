#ifndef BROWSER_H
#define BROWSER_H

#include <vector>
#include "entry.h"
#include "common.h"
#include "gfx.h"
#include "optionsMenu.h"

using namespace std;

enum{
	NO_MODE,
	COPY,
	CUT,
	PASTE,
	DELETE,
	RENAME,
	MKDIR,
	SEND,
	RECEIVE
};

class Browser{

	public:
		Browser();
		~Browser();
		
		void setDrawThreadID(SceUID id);
		
		bool drawThread();
		
		void run();
	
	private:
	
		string cwd; // Current Working Directory
		
		vector<Entry*>* entries; // entries in the current directory
		
		vector<string>* selectedBuffer; // currently selected items
		
		int pasteMode; // COPY or CUT
		
		/* menu control variables */
		int index;  // index of currently selected item
		int start; // where to start drawing the menu
		bool animating; // animate the menu transition?
		
		/* Screen drawing thread data */
		SceUID drawThreadID;
		bool enableDraw; // whether to draw the screen or not
		bool drawThreadWaiting; // whether the draw thread is waiting for enableDraw or not
		bool keepDrawing; // this tells the draw thread to continue drawing or to terminate
		bool draw_progress;
		int progress;
		int max_progress;
		string progress_desc[5]; // the fifth one is left for the actual progress
		
		/* Options Menu instance, will be drawn by the draw thread if it's different from null */
		OptionsMenu* optionsmenu;
		
		
		/* Options menu variables */
		int optionsDrawState;
		int optionsAnimState; // state of the animation
		// options menu entries possition of the entries
		int pEntryIndex;
		
		/* Common browser images */
		Image* checkBox;
		Image* uncheckBox;
		Image* folderIcon;
		Image* fileIcon;
		
		/* Highlight the currently selected item? */
		bool enableSelection;
	
		void moveDirUp();
		
		void update();
		
		void refreshDirs();
		
		void drawScreen();
		
		void drawOptionsMenu();
		
		void drawProgress();
		
		void pauseDraw();
		void resumeDraw();
		void stopDraw();
		
		string formatText(string text);
		
		void select();
		
		void fillSelectedBuffer();
		
		Entry* get();
		
		void moveDown();
		void moveUp();
		
		void down();
		void up();
		
		void deleteFolder(string path);
		void deleteFile(string path);
		void copyFolder(string path);
		int copy_folder_recursive(const char * source, const char * destination);
		void copyFile(string path);
		void copyFile(string path, string destination);
		
		void extractArchive(int type);
		
		void send();
		void recv();
		
		void copy();
		void cut();
		void paste();
		
		void makedir();
		
		void rename();
		
		void removeSelection();
		
		void optionsMenu();
		void options();
		
		void control();
};

#endif
