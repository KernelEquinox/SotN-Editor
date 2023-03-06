#ifndef SOTN_EDITOR_ROOMS
#define SOTN_EDITOR_ROOMS

#include <map>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"
#include "entities.h"
#include "tiles.h"



// Class for room data
class Room {

	public:

		// Room header values
		byte x_start;
		byte y_start;
		byte x_end;
		byte y_end;
		byte tile_layer_id;
		// If flag is 0xFF, use tile_layer_id as an index into the lookup table
		// at 0x800A2464 (each entry being 5 16-bit integers)
		byte load_flags;
		byte entity_layout_id;
		byte entity_graphics_id;

		// Entity list
		std::vector<Entity> entities;

		// Entity graphics
        std::map<uint, GLuint> entity_tilesets;

        // Entity texture pages
        std::vector<GLuint> texture_pages;

		// Tile layers
		TileLayer fg_layer;
		TileLayer bg_layer;

        // Layer textures
        GLuint fg_texture;
        GLuint bg_texture;

		// Room dimensions (measured in cells)
		uint width;
		uint height;

        // Ordering tables for drawing stuff
        std::map<uint, std::vector<EntitySpritePart>> bg_ordering_table;
        std::map<uint, std::vector<EntitySpritePart>> mid_ordering_table;
        std::map<uint, std::vector<EntitySpritePart>> fg_ordering_table;

        // Dedicated 1/4 VRAM chunk for each room (512 x 256)
        GLuint vram;
        GLuint expanded_vram;


		void LoadEntityTilesets();


	private:
    
};

#endif //SOTN_EDITOR_ROOMS