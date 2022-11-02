#ifndef SOTN_EDITOR_SPRITES
#define SOTN_EDITOR_SPRITES

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"
#include <vector>



// A part of a sprite that composes the full sprite
typedef struct SpritePart {
    // 0000 0110 (06) = Subtract 1 from width, add 1 to destination X coord
    // 0000 1001 (09) = Subtract 1 from height, add 1 to destination Y coord
    // 0001 0010 (12) = Subtract 1 from width, add 1 to destination X coord
    // 0010 0001 (21) = Subtract 1 from height, add 1 to destination Y coord
    // 0000 0100 (04) = Subtract 1 from texture end X coord
    // 0000 1000 (08) = Subtract 1 from texture end Y coord
    // 0001 0000 (10) = Add 1 to texture start X coord
    // 0010 0000 (20) = Add 1 to texture start Y coord
	ushort flags = 0;
	short offset_x = 0;
	short offset_y = 0;
	ushort width = 0;
	ushort height = 0;
	ushort clut_offset = 0;
	ushort texture_page = 0;
	ushort texture_start_x = 0;
	ushort texture_start_y = 0;
	ushort texture_end_x = 0;
	ushort texture_end_y = 0;
} SpritePart;



// A placeholder sprite for later processing
typedef struct NullSpritePart {
    ushort idx = 0;
    short offset_x = 0;
    short offset_y = 0;
} NullSpritePart;



// Class for general utilities
class Sprite {

	public:

        // Type of sprite being processed
        bool is_null = false;

        // Vector of sprite parts
		std::vector<SpritePart> parts;
        std::vector<NullSpritePart> null_parts;

        // Address of the sprite data
        uint address = 0;

        // Helper function to read sprites from a given buffer
        static std::vector<std::vector<Sprite>> ReadSpriteBanks(const byte* buf, const uint offset, const uint base);


	private:
    
};

#endif //SOTN_EDITOR_SPRITES