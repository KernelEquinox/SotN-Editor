#ifndef SOTN_EDITOR_CLUTS
#define SOTN_EDITOR_CLUTS

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"



typedef struct ClutEntry {
	int offset;
	int count;
	byte* clut_data;
} ClutEntry;



// Class for general utilities
class Clut {

	public:

	private:

};

#endif //SOTN_EDITOR_CLUTS