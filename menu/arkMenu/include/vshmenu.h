#ifndef VSHMENU_H
#define VSHMENU_H

#include "common.h"
#include "entry.h"
#include "pluginmgr.h"

class VSHMenu {

	private:
	
		PluginsManager* plugins;
	
		int animation;
		int w, h, x, y;
		int index;
		
		string* customText;
		int ntext;
		
		bool drawPluginsMGR;
	
	public:
	
		VSHMenu();
		~VSHMenu();
	
		void setCustomText(string text[], int n);
		void unsetCustomText();
	
		void draw();
		
		void control(Entry* e);
		
		PluginsManager* getPluginsManager();

};

#endif
