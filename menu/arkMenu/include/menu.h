#ifndef MENU_H
#define MENU_H

#include <vector>
#include <cstdlib>
#include "iso.h"
#include "cso.h"
#include "eboot.h"
#include "gfx.h"
#include "plugin.h"

#define MAX_DRAW_ENTRIES 5
#define MAX_LOADED_ICONS 11

typedef enum {
	GAME = 0,
	HOMEBREW = 1,
	POPS = 2
}EntryType;

using namespace std;

class CatMenu{

	private:
		EntryType type;
		int index;
		int threadIndex;
		int animating;
		float animState;
		int fastScroll;
		bool fastScrolling;
		bool animDelay;
		bool initLoad;
		bool stopLoading;
		vector<Entry*>* entries;
		
		void freeIcons();
		bool checkIconsNeeded(bool isSelected);
		
	public:
		CatMenu(EntryType t);
		~CatMenu();
		
		void draw(bool selected);
		
		void loadIconsDynamic(bool isSelected);
		
		bool waitIconsLoad(bool isSelected, bool forceQuit=false);
		
		void resumeIconLoading();
		
		void addEntry(Entry* e);
		Entry* getEntry();
		Entry* getEntry(int index);
		void clearEntries();
		size_t getVectorSize();
		vector<Entry*>* getVector();
		
		bool empty();
		
		void animStart(int direction);
		void moveUp();
		void moveDown();
		void stopFastScroll();
};
		
#endif
