#ifndef SOTN_EDITOR_TILES
#define SOTN_EDITOR_TILES

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"



// Tile class
class Tile {

    public:

        // Texture for the tile
        GLuint texture = -1;

        // Whether the tile is selected
        bool selected = false;

        // Whether the tile is fully transparent (empty) or not
        bool empty = false;

        // Whether the tile should be drawn or not (mostly for skipping overlapping foreground tiles)
        bool draw = true;



    private:
};


// Tile data class (to prevent allocations for every tile layer)
class TileData {

    public:

        // Tile data to pull from
        byte* tileset_ids;
        byte* tile_positions;
        byte* clut_ids;
        byte* collision_ids;



    private:
};



// Tile layer class
class TileLayer {

    public:

        // Indices used for pulling data from the tile definition arrays
        ushort* tile_indices = {};

        // Pointer to a tile data object (to prevent unnecessary allocations)
        TileData tile_data = {};

        // Address of tile indices and tile data (used for entity processing)
        uint tile_indices_addr = 0;
        uint tile_data_addr = 0;

        // Layer dimensions
        byte x_start = 0;
        byte y_start = 0;
        byte x_end = 0;
        byte y_end = 0;

        // Layer dimensions in tiles
        uint width = 0;
        uint height = 0;

        // Force load tileset/room if >= 0x80, otherwise flag on 0x10, 0x20, 0x40
        //
        // Note:
        //     This also allows Alucard's X/Y position to be retained in the next room
        //     if the values are less than 0x80.
        //
        byte load_flags = 0;

        // Z-index of the layer
        ushort z_index = 0;

        // Drawing flags
        ushort drawing_flags = 0;

        // 800730A0 flags:
        //     01 00 = Show FG
        //     80 00 = Transparency
        //     00 01 = Use different tiles?
        //     00 02 = Use different CLUTs?

        // List of tile objects
        std::vector<Tile> tiles = {};



    private:

};

#endif //SOTN_EDITOR_TILES