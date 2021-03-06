#ifndef __DISPLAY_LISTENER__
#define __DISPLAY_LISTENER__

#include "core/display.h"

class DisplayListener
{
	public:
		virtual update_display(Display*) = 0;

		~DisplayListener() { }
	protected:
		DisplayListener() { }
};

#endif