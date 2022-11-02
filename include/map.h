#ifndef SOTN_EDITOR_MAP
#define SOTN_EDITOR_MAP

#include <string>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include <vector>
#include "common.h"
#include "rooms.h"
#include "entities.h"
#include "sprites.h"
#include "tiles.h"
#include "cluts.h"



// First 5 instructions of EntityCreate
const byte entity_create_search[] = {
        0xE0, 0xFF, 0xBD, 0x27,
        0x10, 0x00, 0xB0, 0xAF,
        0x21, 0x80, 0x80, 0x00,
        0x14, 0x00, 0xB1, 0xAF,
        0x18, 0x00, 0xBF, 0xAF
};



// Class for map data
class Map {

    public:

        // Whether the map has been loaded or not
        bool loaded;

        // Current loading status of the map to display to the user
        std::string load_status_msg;

        // Name of the map
        std::string map_id;

        // Function address definitions
        uint update_entities_func;
        uint process_entity_collision_func;
        uint spawn_onscreen_entities_func;
        uint init_room_entities_func;
        uint pause_entities_func;


        // List of rooms
        std::vector<Room> rooms;

        // List of sprite banks
        std::vector<std::vector<Sprite>> sprite_banks;

        // List of entity CLUTs (256 16-color RGB1555 CLUTs)
        std::vector<ClutEntry> entity_cluts;
        GLuint entity_cluts_texture;

        // List of entity layout data
        std::vector<std::vector<EntityInitData>> entity_layouts;

        // List of tile layers
        std::vector<std::pair<TileLayer, TileLayer>> tile_layers;

        // List of entity graphic data
        std::vector<std::vector<EntityGraphicsData>> entity_graphics;



        // Textures for the map (located in ST/<X>/F_<X>.BIN)
        std::vector<GLuint> map_textures;



        // List of map tile CLUTs (256 16-color RGB1555 CLUTs)
        byte map_tile_cluts[256][16 * 2];



        // Map VRAM
        GLuint map_vram;

        // Map tilesets
        std::vector<GLuint> map_tilesets;

        // Entity functions
        std::vector<uint> entity_functions;

        // Map of tile data objects to prevent unnecessary allocations
        std::map<uint, TileData> tile_data_pointers;

        // Dimensions
        uint width;
        uint height;



        void LoadMapFile(const char* filename);
        void LoadMapGraphics(const char* filename);
        void LoadMapEntities();
        void Cleanup();


    private:
    
};

#endif //SOTN_EDITOR_MAP