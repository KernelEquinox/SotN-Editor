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
        GLuint texture;

        // Whether the tile is selected
        bool selected = false;

        // Whether the tile is fully transparent (empty) or not
        bool empty = false;

        // Whether the tile should be drawn or not (mostly for skipping overlapping foreground tiles)
        bool draw = true;



    private:
};



/*
 * FREE WHEN DONE
 */
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
        ushort* tile_indices;

        // Pointer to a tile data object (to prevent unnecessary allocations)
        TileData tile_data;

        // Layer dimensions
        byte x_start;
        byte y_start;
        byte x_end;
        byte y_end;

        // Layer dimensions in tiles
        uint width;
        uint height;

        // Force load tileset/room if >= 0x80, otherwise flag on 0x10, 0x20, 0x40
        //
        // Note:
        //     This also allows Alucard's X/Y position to be retained in the next room
        //     if the values are less than 0x80.
        //
        byte load_flags;

        // Z-index of the layer
        ushort z_index;

        // Drawing flags
        ushort drawing_flags;

        // List of tile objects
        std::vector<Tile> tiles;



    private:

};

#endif //SOTN_EDITOR_TILES