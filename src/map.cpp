#include <filesystem>
#include <map>
#include <algorithm>
#include <iterator>
#include <math.h>
#include <GL/glew.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"
#include "globals.h"
#include "map.h"
#include "rooms.h"
#include "sprites.h"
#include "cluts.h"
#include "tiles.h"
#include "utils.h"
#include "compression.h"
#include "mips.h"
#include "log.h"





/**
 * Lerp.
 *
 * @param a: Starting value
 * @param b: Ending value
 * @param c: Progress between the two points
 *
 * @return Interpolate value between the two points
 */
static inline byte lerp(byte a, byte b, float c) {
    return (byte)((float)a + c * (float)(b - a));
}





/**
 * Loads a map file and updates the calling map object with the processed data.
 *
 * @param filename: Filename of the map to load
 *
 */
void Map::LoadMapFile(const char* filename) {

    // Create a new framebuffer
    glGenFramebuffers(1, &fbo);

    // Bind to the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Set the map ID name
    map_id = std::filesystem::path(filename).stem().string();

    // Temporary buffer for optimized reads
    byte* map_data;

    // Open the file
    FILE* fp = fopen(filename, "rb");

    // Read file to the temporary buffer
    fseek(fp, 0, SEEK_END);
    uint num_bytes = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    map_data = (byte*)calloc(num_bytes, sizeof(byte));
    fread(map_data, sizeof(byte), num_bytes, fp);
    fclose(fp);

    // Get the function addresses
    update_entities_func = *(uint*)(map_data) - MAP_BIN_OFFSET;
    process_entity_collision_func = *(uint*)(map_data + 0x04) - MAP_BIN_OFFSET;
    spawn_onscreen_entities_func = *(uint*)(map_data + 0x08) - MAP_BIN_OFFSET;
    init_room_entities_func = *(uint*)(map_data + 0x0C) - MAP_BIN_OFFSET;
    pause_entities_func = *(uint*)(map_data + 0x28) - MAP_BIN_OFFSET;

    // Get the data pointers
    uint room_list_addr = *(uint*)(map_data + 0x10);
    uint sprite_banks_addr = *(uint*)(map_data + 0x14);
    uint cluts_addr = *(uint*)(map_data + 0x18);
    uint entity_layouts_addr = *(uint*)(map_data + 0x1C);
    uint tile_layers_addr = *(uint*)(map_data + 0x20);
    uint entity_graphics_addr = *(uint*)(map_data + 0x24);

    // Modify relative function addresses as needed
    room_list_addr -= (room_list_addr > 0 ? MAP_BIN_OFFSET : 0);
    sprite_banks_addr -= (sprite_banks_addr > 0 ? MAP_BIN_OFFSET : 0);
    cluts_addr -= (cluts_addr > 0 ? MAP_BIN_OFFSET : 0);
    entity_layouts_addr -= (entity_layouts_addr > 0 ? MAP_BIN_OFFSET : 0);
    tile_layers_addr -= (tile_layers_addr > 0 ? MAP_BIN_OFFSET : 0);
    entity_graphics_addr -= (entity_graphics_addr > 0 ? MAP_BIN_OFFSET : 0);

    // Get backup entity layout address (usually referenced from the InitRoomEntities() function)
    if (entity_layouts_addr == 0) {
        entity_layouts_addr = *(uint *)(map_data + init_room_entities_func + 0x1C) & 0x0000FFFF;
        // Fallback to zero if this was invalid
        if (entity_layouts_addr == 1) {
            entity_layouts_addr = 0;
        }
    }

    // Entity function list address
    uint entity_functions_addr = 0;

    // Check if entity layout address was defined
    if (entity_layouts_addr > 0) {
        // Set entity function list address to immediately after the X/Y entity layouts
        entity_functions_addr = entity_layouts_addr + (4 * 53) + (4 * 52);
    }

    // Otherwise search for it the hard way
    else {

        // Get the location of the function in memory
        unsigned char* match_location = std::search(
            map_data,
            map_data + num_bytes,
            std::begin(entity_create_search),
            std::end(entity_create_search)
        );

        // Found a match
        if (match_location < map_data + num_bytes)
        {
            entity_functions_addr = *(ushort*)(match_location + 0x40);
        }
    }




// -- Rooms ----------------------------------------------------------------------------------------------------

    // Read the room list
    load_status_msg = "Reading Room Data ...";
    if (room_list_addr > 0) {
        uint i = 0;
        while (*(uint *)(map_data + room_list_addr + i) != 0x00000040) {

            // Create a new room
            Room room;

            // Allocate VRAM for the room
            byte* tmp = (byte*)calloc(512 * 256 * 4, sizeof(byte));
            room.vram = Utils::CreateTexture(tmp, 512, 256);
            free(tmp);

            // Read the room's properties
            room.x_start = *(map_data + room_list_addr + i++);
            room.y_start = *(map_data + room_list_addr + i++);
            room.x_end = *(map_data + room_list_addr + i++);
            room.y_end = *(map_data + room_list_addr + i++);
            room.tile_layer_id = *(map_data + room_list_addr + i++);
            room.load_flags = *(map_data + room_list_addr + i++);
            room.entity_graphics_id = *(map_data + room_list_addr + i++);
            room.entity_layout_id = *(map_data + room_list_addr + i++);

            // Check if entity graphics ID needs to be fixed up
            if (room.entity_graphics_id == 0) {
                room.entity_graphics_id++;
            }

            // Calculate room dimensions
            room.width = ((room.x_end + 1) - room.x_start);
            room.height = ((room.y_end + 1) - room.y_start);

            // Add the room to the room list
            rooms.push_back(room);
        }

        Log::Info("Rooms Loaded! Total rooms: %zu\n", rooms.size());
    }




// -- Sprite Banks ---------------------------------------------------------------------------------------------

    // Check if any sprite banks were defined
    load_status_msg = "Reading Sprite Banks ...";
    if (sprite_banks_addr > 0) {

        // Read the sprite banks
        sprite_banks = Sprite::ReadSpriteBanks(map_data, sprite_banks_addr, MAP_BIN_OFFSET);

        Log::Info("Sprites loaded! Total banks: %d\n", sprite_banks.size());
    }




// -- CLUTs ----------------------------------------------------------------------------------------------------

    // Get the address of the CLUT data
    load_status_msg = "Reading CLUT Data ...";
    if (cluts_addr > 0) {
        uint clut_list_addr = *(uint*)(map_data + cluts_addr) - MAP_BIN_OFFSET;

        // Skip the first 4 bytes (0x00000005)
        clut_list_addr += 4;

        // Loop through each CLUT entry
        uint i = 0;
        while (*(int*)(map_data + clut_list_addr + i) != -1) {

            // Create a CLUT entry
            ClutEntry clut_entry;

            // Get the data for each entry in the CLUT list
            clut_entry.offset = *(int*)(map_data + clut_list_addr + i) * 2;
            clut_entry.count = *(int*)(map_data + clut_list_addr + i + 4) * 2;
            uint clut_addr = *(uint*)(map_data + clut_list_addr + i + 8) - MAP_BIN_OFFSET;

            // Allocate data for the CLUT
            clut_entry.clut_data = (byte*)calloc(clut_entry.count, sizeof(byte));

            // Get the CLUT data
            memcpy(clut_entry.clut_data, (map_data + clut_addr), clut_entry.count);

            /*
             * NOTE:
             * Offset >> 3 = CLUT index offset
             * Offset << 1 = Offset from CLUT base address
             */

            // Append the entry to the CLUT list
            entity_cluts.push_back(clut_entry);

            // Go to the next entry
            i += 12;
        }

        Log::Info("CLUTs loaded! Total CLUTs: %d\n", entity_cluts.size());
    }




// -- Entity Layouts -------------------------------------------------------------------------------------------

    // Loop through all 53 entries
    load_status_msg = "Reading Entity Layout Data ...";
    if (entity_layouts_addr > 0) {
        for (int i = 0; i < 53; i++) {

            // Get the address of the entity layout
            uint entity_list_addr = *(uint*)(map_data + entity_layouts_addr + (i * 4)) - MAP_BIN_OFFSET;

            // Initialize a new list of entity init data
            std::vector<EntityInitData> entity_init_list;

            // Get the X/Y coords of the first entity (should both be -2)
            short x_coord = *(short*)(map_data + entity_list_addr);
            short y_coord = *(short*)(map_data + entity_list_addr + 2);

            // Collect entity init data until X/Y both equal -1
            int k = 0;
            while (x_coord != -1 && y_coord != -1) {

                // Create a new entity init data struct
                EntityInitData init_data;

                // Collect the init data
                init_data.x_coord = x_coord;
                init_data.y_coord = y_coord;
                init_data.entity_id = *(ushort*)(map_data + entity_list_addr + (sizeof(EntityInitData) * k) + 4);
                init_data.slot = *(ushort*)(map_data + entity_list_addr + (sizeof(EntityInitData) * k) + 6);
                init_data.initial_state = *(ushort*)(map_data + entity_list_addr + (sizeof(EntityInitData) * k) + 8);

                // Add the entity init data to the list
                entity_init_list.push_back(init_data);

                // Get the next entry
                k++;
                x_coord = *(short*)(map_data + entity_list_addr + (sizeof(EntityInitData) * k));
                y_coord = *(short*)(map_data + entity_list_addr + (sizeof(EntityInitData) * k) + 2);
            }

            // Add the init data list to the layout list
            entity_layouts.push_back(entity_init_list);
        }

        Log::Info("Entity layouts loaded! Total layouts: %zu\n", entity_layouts.size());
    }




// -- Tile Layers ----------------------------------------------------------------------------------------------

    // Check if tile layers exist
    load_status_msg = "Reading Tile Layer Data ...";
    if (tile_layers_addr > 0) {

        // Loop through layers until no more exist
        uint i = 0;
        uint layer_pair_addr = *(uint*)(map_data + tile_layers_addr + (i * 8));
        while (layer_pair_addr > 0x80180000 && layer_pair_addr < RAM_MAX_OFFSET) {

            // Create a new tile layer pair
            std::pair<TileLayer, TileLayer> tile_pair;

            // Process layers in pairs
            for (int k = 0; k < 2; k++) {

                // Create a new layer
                TileLayer* cur_layer;

                // Check which layer to process
                if (k == 0) {
                    cur_layer = &tile_pair.first;
                }
                else {
                    cur_layer = &tile_pair.second;
                }

                // Get the address of the layer data
                uint layer_addr = *(uint*)(map_data + tile_layers_addr + (i * 8) + (k * 4)) - MAP_BIN_OFFSET;

                // Get the address of the tile indices
                cur_layer->tile_indices_addr = *(uint*)(map_data + layer_addr);

                // Skip null pointers
                if (cur_layer->tile_indices_addr == 0) {
                    continue;
                }

                // Adjust tile index address
                cur_layer->tile_indices_addr -= MAP_BIN_OFFSET;

                // Get the layer dimensions (3-byte value, ignores highest byte)
                uint dimension_data = *(uint*)(map_data + layer_addr + 8);
                cur_layer->x_start = dimension_data & 0x3F;
                cur_layer->y_start = (dimension_data >> 6) & 0x3F;
                cur_layer->x_end = (dimension_data >> 12) & 0x3F;
                cur_layer->y_end = (dimension_data >> 18) & 0x3F;

                // Calculate dimensions in terms of tiles
                cur_layer->width = (cur_layer->x_end - cur_layer->x_start + 1) * 16;
                cur_layer->height = (cur_layer->y_end - cur_layer->y_start + 1) * 16;

                // Get the rest of the non-pointer data
                cur_layer->load_flags = *(byte*)(map_data + layer_addr + 11);
                cur_layer->z_index = *(ushort*)(map_data + layer_addr + 12);
                cur_layer->drawing_flags = *(ushort*)(map_data + layer_addr + 14);

                // Calculate width and height of the layer in tiles
                uint num_tiles = cur_layer->width * cur_layer->height;

                // Read the tile indices into a newly-allocated buffer
                cur_layer->tile_indices = (ushort*)calloc(num_tiles, sizeof(ushort));
                memcpy(cur_layer->tile_indices, map_data + cur_layer->tile_indices_addr, num_tiles * sizeof(ushort));

                // Get the address of the tile data
                cur_layer->tile_data_addr = *(uint*)(map_data + layer_addr + 4) - MAP_BIN_OFFSET;

                // Check if the tile data pointer is not currently in the map
                if (tile_data_pointers.count(cur_layer->tile_data_addr) == 0) {

                    // Create a new tile data object
                    TileData tile_data;

                    // Get the addresses of all tile data elements
                    uint tile_ids_addr = *(uint*)(map_data + cur_layer->tile_data_addr) - MAP_BIN_OFFSET;
                    uint tile_positions_addr = *(uint*)(map_data + cur_layer->tile_data_addr + 4) - MAP_BIN_OFFSET;
                    uint tile_cluts_addr = *(uint*)(map_data + cur_layer->tile_data_addr + 8) - MAP_BIN_OFFSET;
                    uint tile_collision_addr = *(uint*)(map_data + cur_layer->tile_data_addr + 12) - MAP_BIN_OFFSET;

                    // Allocate memory for all elements
                    tile_data.tileset_ids = (byte*)calloc(4096, sizeof(byte));
                    tile_data.tile_positions = (byte*)calloc(4096, sizeof(byte));
                    tile_data.clut_ids = (byte*)calloc(4096, sizeof(byte));
                    tile_data.collision_ids = (byte*)calloc(4096, sizeof(byte));

                    // Read all element data
                    memcpy(tile_data.tileset_ids, map_data + tile_ids_addr, 4096);
                    memcpy(tile_data.tile_positions, map_data + tile_positions_addr, 4096);
                    memcpy(tile_data.clut_ids, map_data + tile_cluts_addr, 4096);
                    memcpy(tile_data.collision_ids, map_data + tile_collision_addr, 4096);

                    // Add the tile data to the tile data map using the address of the data as the key
                    tile_data_pointers[cur_layer->tile_data_addr] = tile_data;
                }

                // Set the tile data pointer for the layer
                cur_layer->tile_data = tile_data_pointers.at(cur_layer->tile_data_addr);
            }

            // Move to the next layer pair
            i++;
            layer_pair_addr = *(uint*)(map_data + tile_layers_addr + (i * 8));

            // Add the layer pair to the list of layers
            tile_layers.push_back(tile_pair);
        }

        Log::Info("Tile layers loaded! Total layers: %zu\n", tile_layers.size());
    }




// -- Entity Graphics ------------------------------------------------------------------------------------------

    // Check if entity graphics exist
    load_status_msg = "Reading Entity Graphics Data ...";
    if (entity_graphics_addr > 0) {

        // Loop until the beginning of the Entity Layouts section is hit (indicating end of graphics section)
        uint i = 0;
        while (entity_graphics_addr + (i * 4) != entity_layouts_addr) {

            // Get the address for the next list of entity graphics entries
            uint graphics_list_addr = *(uint*)(map_data + entity_graphics_addr + (i * 4));

            // Initialize a new list of entity init data
            std::vector<EntityGraphicsData> entity_graphics_data_list;

            // Skip any null entries
            if (graphics_list_addr == 0) {
                entity_graphics.push_back(entity_graphics_data_list);
                i++;
                continue;
            }

            // Adjust graphics list address
            graphics_list_addr -= MAP_BIN_OFFSET;

            // Get the first identifier
            int marker = *(int*)(map_data + graphics_list_addr);

            // Only process if this wasn't an ending marker
            if (marker > 0) {

                // Skip the first 4 bytes
                graphics_list_addr += 4;

                // Loop through each entry until the end of the list (-1) was encountered
                uint k = 0;
                while (marker != -1) {

                    // Create a new entity graphic entry
                    EntityGraphicsData graphics_data;

                    // Read the data
                    graphics_data.vram_y = *(ushort*)(map_data + graphics_list_addr + (k * sizeof(EntityGraphicsData)));
                    graphics_data.vram_x = *(ushort*)(map_data + graphics_list_addr + (k * sizeof(EntityGraphicsData)) + 2);
                    graphics_data.height = *(ushort*)(map_data + graphics_list_addr + (k * sizeof(EntityGraphicsData)) + 4);
                    graphics_data.width = *(ushort*)(map_data + graphics_list_addr + (k * sizeof(EntityGraphicsData)) + 6);
                    graphics_data.compressed_graphics_addr = *(uint*)(map_data + graphics_list_addr + (k * sizeof(EntityGraphicsData)) + 8);

                    // Add the entry to the entity graphics list
                    entity_graphics_data_list.push_back(graphics_data);

                    // Move to the next entry
                    k++;
                    marker = *(int*)(map_data + graphics_list_addr + (k * sizeof(EntityGraphicsData)));
                }
            }

            // Add the entity graphics data to the graphics list
            entity_graphics.push_back(entity_graphics_data_list);

            // Move to the next list address
            i++;
        }

        Log::Info("Entity graphics loaded! Total entries: %zu\n", entity_graphics.size());
    }




// -- Populate Room Data ---------------------------------------------------------------------------------------

    // Loop through each room
    load_status_msg = "Populating Room Data ...";
    for (auto& room : rooms) {

        Room* cur_room = &room;

        // Get the layer data only if this is not a transition room
        if (cur_room->load_flags == 0xFF) {
            continue;
        }

        // Get the layer data for the room
        cur_room->fg_layer = tile_layers[cur_room->tile_layer_id].first;
        cur_room->bg_layer = tile_layers[cur_room->tile_layer_id].second;

        // Check whether to modify entity graphics ID
        uint gfx_id = cur_room->entity_graphics_id;
        if (gfx_id > 0) {
            gfx_id -= 1;
        }

        // Get the list of entity graphics for the room
        std::vector<EntityGraphicsData> entity_graphics_list = entity_graphics[gfx_id];

        // Loop through each graphics list entry
        for (auto graphics_data : entity_graphics_list) {

            // Skip this entry if no data address was defined
            if (graphics_data.compressed_graphics_addr == 0) {
                continue;
            }

            // Adjust compressed graphics address
            graphics_data.compressed_graphics_addr -= MAP_BIN_OFFSET;

            // Determine the size of the decompressed data (index bytes [1], not pixels [4])
            uint data_size = graphics_data.width * graphics_data.height;

            // Allocate space for decompression and decompress the data
            byte* tileset_data = (byte*)calloc(data_size, sizeof(byte));
            Compression::Decompress(tileset_data, (map_data + graphics_data.compressed_graphics_addr));

            // Get RGBA bytes from the tileset data
            byte* pixel_data = Utils::Indexed_to_RGBA(tileset_data, data_size / 4);

            // Create a texture from the data
            GLuint entity_tileset_texture = Utils::CreateTexture(pixel_data, graphics_data.width / 4, graphics_data.height);

            // Write the texture data to the room's VRAM
            glBindTexture(GL_TEXTURE_2D, cur_room->vram);
            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                graphics_data.vram_x,
                graphics_data.vram_y - 256,
                graphics_data.width / 4,
                graphics_data.height,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixel_data
            );
            glBindTexture(GL_TEXTURE_2D, 0);

            // Free data
            free(tileset_data);
            free(pixel_data);

            // Calculate chunk coords
            uint chunk_x = graphics_data.vram_x >> 6;
            uint chunk_y = 3 - (graphics_data.vram_y >> 8);

            // Convert chunk coords to VRAM index
            uint vram_idx = (((chunk_y * 8) + chunk_x) << 2);

            // Calculate subchunk
            vram_idx |= ((graphics_data.vram_x << 2) & 0x80) >> 7;
            vram_idx |= (graphics_data.vram_y & 0x80) >> 6;

            // Set entity tileset ID
            cur_room->entity_tilesets[vram_idx] = entity_tileset_texture;

            // Calculate texture page
            /*
            uint tpage_x = graphics_data.vram_x / 64;
            uint tpage_y = graphics_data.vram_y / 256;
            uint tpage_idx = (tpage_y << 4) + tpage_x;
            cur_room->texture_pages[tpage_idx] = entity_tileset_texture;
            */
        }

        // Attach to the target texture and read the pixels into VRAM
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cur_room->vram, 0);

        // Convert VRAM to texture pages
        for (int k = 0; k < 8; k++) {

            byte* pixels = Utils::GetPixels(cur_room->vram, 64 * k, 0, 64, 256);

            // Create page texture from VRAM section
            GLuint page_texture = Utils::CreateTexture(pixels, 64, 256);
            cur_room->texture_pages.push_back(page_texture);
            free(pixels);
        }

        // Expand VRAM additions to better show
        const byte greyscale_clut[16 * 4] = {
            0x00, 0x00, 0x00, 0x00,
            0x11, 0x11, 0x11, 0xFF,
            0x22, 0x22, 0x22, 0xFF,
            0x33, 0x33, 0x33, 0xFF,
            0x44, 0x44, 0x44, 0xFF,
            0x55, 0x55, 0x55, 0xFF,
            0x66, 0x66, 0x66, 0xFF,
            0x77, 0x77, 0x77, 0xFF,
            0x88, 0x88, 0x88, 0xFF,
            0x99, 0x99, 0x99, 0xFF,
            0xAA, 0xAA, 0xAA, 0xFF,
            0xBB, 0xBB, 0xBB, 0xFF,
            0xCC, 0xCC, 0xCC, 0xFF,
            0xDD, 0xDD, 0xDD, 0xFF,
            0xEE, 0xEE, 0xEE, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF
        };
        byte *pixels = Utils::GetPixels(cur_room->vram, 0, 0, 512, 256);
        byte *rgba_pixels = (byte *) calloc(2048 * 256 * 4, sizeof(byte));
        Utils::VRAM_to_RGBA(pixels, greyscale_clut, 512, 256, rgba_pixels);
        cur_room->expanded_vram = Utils::CreateTexture(rgba_pixels, 2048, 256);
        free(pixels);
        free(rgba_pixels);
    }




// -- Entity Functions -----------------------------------------------------------------------------------------

    // Make sure entity functions exist
    load_status_msg = "Reading Entity Functions ...";
    if (entity_functions_addr > 0) {

        // Get the first function pointer (should point to dummy data)
        uint func_addr = *(uint*)(map_data + entity_functions_addr);

        // Loop until a non-pointer is found
        uint i = 1;
        while (func_addr >= MAP_BIN_OFFSET && func_addr < RAM_MAX_OFFSET) {
            entity_functions.push_back(func_addr - MAP_BIN_OFFSET);
            func_addr = *(uint*)(map_data + entity_functions_addr + (i++ * 4));
        }
    }

    Log::Info("Entity functions: %zu\n", entity_functions.size());




    // Free the temporary memory
    free(map_data);
}





/**
 * Loads the graphics for a given map file.
 *
 * @param filename: Filename of the map to load graphics data from
 *
 */
void Map::LoadMapGraphics(const char* filename) {

    // Bind to the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Open the graphics file (F_*.BIN)
    FILE* fp = fopen(filename, "rb");

    // Get the file size
    fseek(fp, 0, SEEK_END);
    uint num_bytes = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Allocate space for the data
    byte* file_data = (byte*)calloc(num_bytes, sizeof(byte));

    // Read the data and close the file
    fread(file_data, sizeof(byte), num_bytes, fp);
    fclose(fp);





    // Convert file data to RGBA pixels
    byte* pixel_data = Utils::Indexed_to_RGBA(file_data, num_bytes / 2);




    /*
    // Create a new framebuffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    // Bind to the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    */





    // Loop through each slice of the data
    for (int i = 0; i < (num_bytes) / 8192; i++) {

        GLuint chunk_texture = Utils::CreateTexture(pixel_data + (i * 8192 * 2), 32, 128);

        // Add the texture pointer to the list of map textures
        map_textures.push_back(chunk_texture);
    }







    load_status_msg = "Reading Map Textures Into VRAM ...";

    // Allocate data for a full VRAM texture
    byte* vram_data = (byte*)calloc(num_bytes * 4 * 2, sizeof(byte));

    // Keep track of offset into VRAM
    uint cur_offset = 0;

    // Loop through each line of all 32 textures
    for (int k = 0; k < 128; k++) {
        for (int i = 0; i < 32; i++) {

            // Only process if this is within the TOP half of VRAM
            if (i % 4 < 2) {

                // Attach to the target texture and read the pixels into VRAM
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, map_textures[i], 0);
                glReadPixels(0, k, 32, 1, GL_RGBA, GL_UNSIGNED_BYTE, vram_data + cur_offset);
                cur_offset += 32 * 4;
            }
        }
    }

    // Loop through each line of all 32 textures
    for (int k = 0; k < 128; k++) {
        for (int i = 0; i < 32; i++) {

            // Only process if this is within the BOTTOM half of VRAM
            if (i % 4 > 1) {

                // Attach to the target texture and read the pixels into VRAM
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, map_textures[i], 0);
                glReadPixels(0, k, 32, 1, GL_RGBA, GL_UNSIGNED_BYTE, vram_data + cur_offset);
                cur_offset += 32 * 4;

            }
        }
    }






    // Create texture from VRAM data
    GLuint vram_texture = Utils::CreateTexture(vram_data, 512, 256);

    // Set the map VRAM
    map_vram = vram_texture;


    // Expand VRAM additions to better show
    const byte greyscale_clut[16 * 4] = {
        0x00, 0x00, 0x00, 0x00,
        0x11, 0x11, 0x11, 0xFF,
        0x22, 0x22, 0x22, 0xFF,
        0x33, 0x33, 0x33, 0xFF,
        0x44, 0x44, 0x44, 0xFF,
        0x55, 0x55, 0x55, 0xFF,
        0x66, 0x66, 0x66, 0xFF,
        0x77, 0x77, 0x77, 0xFF,
        0x88, 0x88, 0x88, 0xFF,
        0x99, 0x99, 0x99, 0xFF,
        0xAA, 0xAA, 0xAA, 0xFF,
        0xBB, 0xBB, 0xBB, 0xFF,
        0xCC, 0xCC, 0xCC, 0xFF,
        0xDD, 0xDD, 0xDD, 0xFF,
        0xEE, 0xEE, 0xEE, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF
    };
    byte *rgba_pixels = (byte *) calloc(2048 * 256 * 4, sizeof(byte));
    Utils::VRAM_to_RGBA(vram_data, greyscale_clut, 512, 256, rgba_pixels);
    expanded_map_vram = Utils::CreateTexture(rgba_pixels, 2048, 256);
    free(rgba_pixels);






    load_status_msg = "Loading Map Tile CLUTs ...";

    // Bind to the VRAM texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vram_texture, 0);

    // Read all of the CLUT data from the VRAM texture to skip any further processing
    byte* map_rgba_cluts = (byte*)calloc(256 * 16 * 4, sizeof(byte));
    glReadPixels(0, 240, 256, 16, GL_RGBA, GL_UNSIGNED_BYTE, map_rgba_cluts);
    byte* indexed_cluts = Utils::RGBA_to_Indexed(map_rgba_cluts, 256 * 16);
    memcpy(map_tile_cluts[0], indexed_cluts, 256 * 16 * 2);
    free(map_rgba_cluts);
    free(indexed_cluts);

    // Collect all of the tilesets
    load_status_msg = "Creating Tileset Textures ...";
    for (int i = 0; i < 8; i++) {

        // Read a block of pixels from VRAM
        byte* tileset_data = (byte*)calloc(64 * 256 * 4, sizeof(byte));
        glReadPixels(i * 64, 0, 64, 256, GL_RGBA, GL_UNSIGNED_BYTE, tileset_data);

        // Create a texture from each VRAM block
        GLuint tileset_texture = Utils::CreateTexture(tileset_data, 64, 256);

        // Free the tileset data
        free(tileset_data);

        // Add the tileset to the list of the map's tilesets
        map_tilesets.push_back(tileset_texture);
    }






    // Pad the tilesets from 0x08 - 0x10
    for (int i = 0; i < 8; i++) {
        map_tilesets.push_back(map_tilesets[0]);
    }

    // Add each F_GAME.BIN tileset to the map tileset list
    for (int i = 0; i < 8; i++) {
        map_tilesets.push_back(fgame_textures[i]);
    }






    // Loop through all of the layers
    load_status_msg = "Creating Tile Textures ...";
    for (size_t i = 0; i < tile_layers.size(); i++) {

        load_status_msg = Utils::FormatString("Creating Tile Textures ( %zu / %zu ) ...", i, tile_layers.size());

        // Loop through each FG/BG layer
        for (int k = 0; k < 2; k++) {

            // Get the current FG or BG tile layer
            TileLayer* cur_layer = (k == 0 ? &tile_layers[i].first : &tile_layers[i].second);

            // Loop through each tile
            for (int idx = 0; idx < cur_layer->width * cur_layer->height; idx++) {

                // Get the tile
                ushort tile_idx = cur_layer->tile_indices[idx];
                byte tileset_id = cur_layer->tile_data.tileset_ids[tile_idx];
                byte tile_position = cur_layer->tile_data.tile_positions[tile_idx];
                byte clut_id = cur_layer->tile_data.clut_ids[tile_idx];
                byte tile_collision = cur_layer->tile_data.collision_ids[tile_idx];

                // Get the tileset to use for the lookup
                GLuint tileset = map_tilesets[tileset_id];

                // Get the X/Y offset of the tile within the tileset
                uint offset_x = (tile_position & 0xF) * 16;
                uint offset_y = ((tile_position >> 4) & 0xF) * 16;

                // Adjust the offsets as needed for room types 0x20 and 0x40
                if ((cur_layer->load_flags & 0x20) == 0x20) {
                    offset_x %= 128;
                    offset_y -= 16 * (offset_y % 32 != 0);
                }

                // Attach to the tileset
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileset, 0);

                // Get the pixels from the tileset
                uint tile_width = 16;
                uint tile_height = 16;
                byte* pixels = (byte*)calloc((tile_width / 4) * tile_height * 4, sizeof(byte));
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glReadPixels(offset_x / 4, offset_y, tile_width / 4, tile_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

                // Allocate space for the tile
                byte* tile_output = (byte*)calloc(tile_width * tile_height * 4, sizeof(byte));

                // Select the appropriate CLUT
                uint num_pixels = 0;
                if ((cur_layer->drawing_flags & 0x200) == 0x200) {
                    uint clut_offset = (clut_id % 256) * 16 * 4;
                    num_pixels = Utils::VRAM_to_RGBA(pixels, generic_rgba_cluts + clut_offset, tile_width / 4, tile_height, tile_output);
                }
                else {
                    byte* clut = Utils::Indexed_to_RGBA(map_tile_cluts[clut_id], 32);
                    num_pixels = Utils::VRAM_to_RGBA(pixels, clut, tile_width / 4, tile_height, tile_output);
                    free(clut);
                }

                // Create a new tile object for management
                Tile tile;

                // Set the tile's texture
                tile.texture = Utils::CreateTexture(tile_output, 16, 16);

                // Check if tile was empty
                if (num_pixels == 0) {
                    tile.empty = true;
                }

                // Add the tile to the layer
                /*
                if (k == 0) {
                    tile_layers[i].first.tiles.push_back(tile);
                }
                else {
                    tile_layers[i].second.tiles.push_back(tile);
                }
                */
                cur_layer->tiles.push_back(tile);

                // Free the data
                free(tile_output);
                free(pixels);
            }
        }
    }

    // Set each tile layer for each room
    for (size_t i = 0; i < rooms.size(); i++) {

        Room* cur_room = &rooms[i];

        // Skip transition rooms
        if (cur_room->load_flags == 0xFF) {
            continue;
        }

        // Get the current layer ID
        uint layer_id = cur_room->tile_layer_id;
        if (layer_id >= tile_layers.size()) {
            Log::Error("Room %d has tile layer ID %d (only %zu layers exist)\n", i, layer_id, tile_layers.size());
            continue;
        }
        cur_room->fg_layer = tile_layers[layer_id].first;
        cur_room->bg_layer = tile_layers[layer_id].second;
    }



    // Loop through each room to create a single texture from the tile layers
    for (size_t i = 0; i < rooms.size(); i++) {

        load_status_msg = Utils::FormatString("Converting Layers to Textures ( %zu / %zu ) ...", i, rooms.size());

        Room* cur_room = &rooms[i];

        byte* bg_blank = (byte*)calloc(cur_room->bg_layer.width * 16 * cur_room->bg_layer.height * 16 * 4, sizeof(byte));
        byte* fg_blank = (byte*)calloc(cur_room->fg_layer.width * 16 * cur_room->fg_layer.height * 16 * 4, sizeof(byte));
        cur_room->bg_texture = Utils::CreateTexture(bg_blank, cur_room->bg_layer.width * 16, cur_room->bg_layer.height * 16);
        cur_room->fg_texture = Utils::CreateTexture(fg_blank, cur_room->fg_layer.width * 16, cur_room->fg_layer.height * 16);

        // Check if BG exists
        if (cur_room->bg_layer.tiles.size() > 0) {

            // Loop through each BG tile row
            for (uint y = 0; y < cur_room->bg_layer.height; y++) {

                // Loop through each tile
                for (uint x = 0; x < cur_room->bg_layer.width; x++) {

                    // Get the index of the tile
                    uint idx = (y * cur_room->bg_layer.width) + x;

                    GLuint cur_tex = cur_room->bg_layer.tiles[idx].texture;
                    byte* pixels = Utils::GetPixels(cur_tex, 0, 0, 16, 16);

                    glBindTexture(GL_TEXTURE_2D, cur_room->bg_texture);
                    glTexSubImage2D(
                        GL_TEXTURE_2D,
                        0,
                        x * 16,
                        y * 16,
                        16,
                        16,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        pixels
                    );
                    glBindTexture(GL_TEXTURE_2D, 0);
                    free(pixels);

                }
            }
        }

        // Check if FG exists
        if (cur_room->fg_layer.tiles.size() > 0) {

            // Loop through each FG tile row
            for (uint y = 0; y < cur_room->fg_layer.height; y++) {

                // Loop through each tile
                for (uint x = 0; x < cur_room->fg_layer.width; x++) {

                    // Get the index of the tile
                    uint idx = (y * cur_room->fg_layer.width) + x;

                    GLuint cur_tex = cur_room->fg_layer.tiles[idx].texture;
                    byte* pixels = Utils::GetPixels(cur_tex, 0, 0, 16, 16);

                    glBindTexture(GL_TEXTURE_2D, cur_room->fg_texture);
                    glTexSubImage2D(
                        GL_TEXTURE_2D,
                        0,
                        x * 16,
                        y * 16,
                        16,
                        16,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        pixels
                    );
                    glBindTexture(GL_TEXTURE_2D, 0);
                    free(pixels);

                }
            }
        }
        free(bg_blank);
        free(fg_blank);
    }







    // Reset the framebuffer target to the main window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /*
    // Delete the framebuffer
    glDeleteFramebuffers(1, &fbo);
    */


    // Free the file data
    free(file_data);

    // Free the pixels
    free(pixel_data);

    // Free VRAM data
    free(vram_data);




    // Find the upper and lower bounds of the room
    uint x_min = 42069;
    uint x_max = 0;
    uint y_min = 42069;
    uint y_max = 0;

    // Loop through each room
    for (size_t i = 0; i < rooms.size(); i++) {

        // Check if this was lower than the current minimum X/Y coords
        if (rooms[i].x_start < x_min) {
            x_min = rooms[i].x_start;
        }
        if (rooms[i].y_start < y_min) {
            y_min = rooms[i].y_start;
        }
        if (rooms[i].x_start + rooms[i].width > x_max) {
            x_max = rooms[i].x_start + rooms[i].width;
        }
        if (rooms[i].y_start + rooms[i].height > y_max) {
            y_max = rooms[i].y_start + rooms[i].height;
        }
    }

    // Map dimensions
    uint map_width = (x_max - x_min);
    uint map_height = (y_max - y_min);

    // Vector to hold the map block coordinates
    std::vector<uint> seen_coords;
    std::vector<uint> overlaps;

    for (auto & room : rooms) {

        Room* cur_room = &room;

        // Loop through each Y block
        for (uint y = (cur_room->y_start - y_min); y <= (cur_room->y_end - y_min); y++) {

            // Loop through each X block
            for (uint x = (cur_room->x_start - x_min); x <= (cur_room->x_end - x_min); x++) {

                // Get the current coordinate offset
                uint offset = (y * map_width) + x;

                // Check if this was an overlap
                if (std::find(seen_coords.begin(), seen_coords.end(), offset) != seen_coords.end()) {
                    overlaps.push_back(offset);
                }
                // This wasn't an overlap, mark as seen
                else {
                    seen_coords.push_back(offset);
                }
            }
        }
    }
}





/**
 * Loads and processes any entities present in the map.
 */
void Map::LoadMapEntities() {

    /*
    // Create a new framebuffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    */

    // Bind to the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Process all entity functions
    for (auto & room : rooms) {

        // Get the current room
        Room* cur_room = &room;

        // Get the entity init data
        uint layout_id = cur_room->entity_layout_id;
        std::vector<EntityInitData> init_data_list = entity_layouts[layout_id];

        // Reset the emulator
        //MipsEmulator::Reset();
        MipsEmulator::LoadState();
        MipsEmulator::ClearEntities();

        // Populate CLUT stuff
        //load_status_msg = "Populating CLUT Data in MIPS RAM ...";
        MipsEmulator::ProcessFunction(0x000EAD7C);

        // Loop through each entry in the init list
        load_status_msg = "Copying Entity Data to MIPS RAM ...";
        for (auto init_data : init_data_list) {

            // Create initial entities
            EntityData entity_data;

            // Populate initial entity data
            entity_data.object_id = init_data.entity_id & 0x03FF;
            entity_data.update_function = entity_functions[entity_data.object_id] + MAP_BIN_OFFSET;
            entity_data.pos_x = init_data.x_coord;
            entity_data.pos_y = init_data.y_coord;
            entity_data.initial_state = init_data.initial_state;
            entity_data.room_slot = init_data.slot >> 8;
            entity_data.unk68 = (init_data.slot >> 10) & 7;

            // Fetch entity function address
            uint entity_func_id = init_data.entity_id & 0x03FF;
            uint entity_func = entity_functions[entity_func_id] + MAP_BIN_OFFSET - RAM_BASE_OFFSET;

            // Determine entity location in RAM
            uint entity_ram_location = ENTITY_ALLOCATION_START + ((init_data.slot & 0xFF) * sizeof(entity_data));

            // Copy the entity structure into the emulator's RAM
            MipsEmulator::CopyToRAM(entity_ram_location, &entity_data, sizeof(entity_data));
        }

        // Write current room dimensions to RAM
        MipsEmulator::WriteIntToRAM(ROOM_WIDTH_ADDR, cur_room->width);
        MipsEmulator::WriteIntToRAM(ROOM_HEIGHT_ADDR, cur_room->width);

        // Write current room coordinates to RAM
        MipsEmulator::WriteIntToRAM(ROOM_X_COORD_START_ADDR, cur_room->x_start);
        MipsEmulator::WriteIntToRAM(ROOM_Y_COORD_START_ADDR, cur_room->y_start);
        MipsEmulator::WriteIntToRAM(ROOM_X_COORD_END_ADDR, cur_room->x_end);
        MipsEmulator::WriteIntToRAM(ROOM_Y_COORD_END_ADDR, cur_room->y_end);

        // Write tile layout to RAM
        MipsEmulator::WriteIntToRAM(ROOM_TILE_INDICES_ADDR, cur_room->fg_layer.tile_indices_addr + MAP_RAM_OFFSET);
        MipsEmulator::WriteIntToRAM(ROOM_TILE_DATA_ADDR, cur_room->fg_layer.tile_data_addr + MAP_RAM_OFFSET);

        // Process all of the entities in RAM
        load_status_msg = "Running Entity Functions ...";
        std::vector<Entity> entities = MipsEmulator::ProcessEntities();

        // Commit any framebuffer changes
        load_status_msg = "Committing Framebuffer Changes ...";
        byte* fb_pixels = Utils::GetPixels(MipsEmulator::framebuffer, 0, 240, 768, 16);
        byte* indexed_pixels = Utils::RGBA_to_Indexed(fb_pixels, 768 * 16);
        free(fb_pixels);
        for (int k = 0; k < 768 * 16 * 2; k++) {
            byte val = indexed_pixels[k];
            if (val > 0) {
                *(byte*)(MipsEmulator::ram + CLUT_BASE_ADDR + k) = val;
            }
        }
        free(indexed_pixels);

        // Regenerate entity CLUT textures just in case an entity modified the CLUTs
        byte* entity_rgba_cluts = (byte*)calloc(0x6000 * 4, sizeof(byte));
        Utils::CLUT_to_RGBA(MipsEmulator::ram + CLUT_BASE_ADDR + 0x4000, entity_rgba_cluts, 256, false);
        entity_cluts_texture = Utils::CreateTexture(entity_rgba_cluts, 256, 16);
        free(entity_rgba_cluts);

        // Associate entities with the room
        cur_room->entities = entities;

        load_status_msg = "Processing Entity Graphics ...";
        for (auto& entity_entry : entities) {

            // Get the current entity
            Entity* entity = &entity_entry;

            // Check if the entity has a SOTN_POLYGON struct
            if (entity->data.unk7C != 0) {
                //if ((entity->data.unk34 & 0xFFFF) == 0x2000) {
                if ((entity->data.unk34 & 0x800000) == 0x800000) {

                    // Set the current polygon address
                    uint polygon_addr = entity->data.unk7C - RAM_BASE_OFFSET;

                    // Loop through all polygons
                    while (true) {

                        // Bail if polygon address is invalid
                        if (polygon_addr > 0x00200000) {
                            break;
                        }

                        // Create a new entity sprite
                        EntitySpritePart entity_subsprite;

                        // Create a SOTN_POLYGON struct from memory
                        SOTN_POLYGON polygon;
                        MipsEmulator::CopyFromRAM(polygon_addr, &polygon, sizeof(SOTN_POLYGON));

                        // Get the polygon type from the code
                        byte cur_code = polygon.code;
                        byte polygon_type = SOTN_PRIM_TYPES.at(cur_code);

                        // Set semi-transparent flag
                        if ((polygon.pad3 & 1) == 1) {
                            entity_subsprite.semi_transparent = true;
                        }

                        // Set texture shade flag
                        if (polygon.pad3 > 0) {
                            entity_subsprite.shade_texture = true;
                        }

                        // Determine ordering table index
                        entity_subsprite.ot_layer = polygon.pad2;




                        // Check if this was a sprite primitive
                        if (polygon_type == PRIM_TYPE_SPRT) {

                            // Calculate the texture page
                            uint tpage = polygon.tpage + (polygon.sprt_tpage & 0x60);

                            // Get the width and height of the texture
                            uint tex_width = polygon.sprt_width;
                            uint tex_height = polygon.sprt_height;

                            // Get the entity texture via the texture page
                            GLuint texture = 0;
                            if (tpage < 0x10) {
                                if (tpage >= 8) {
                                    texture = map_tilesets[tpage - 8];
                                }
                                else {
                                    texture = map_tilesets[tpage];
                                }
                            }
                            else {
                                texture = cur_room->texture_pages[tpage - 0x10];
                            }

                            byte *pixels = Utils::GetPixels(texture, polygon.u0 / 4, polygon.v0, tex_width / 4, tex_height);

                            // Default to generic CLUT
                            byte* clut;

                            // Get the CLUT's location in VRAM
                            uint clut_vram_addr = *(ushort*)(MipsEmulator::ram + CLUT_INDEX_ADDR + (polygon.clut * 2)) << 5;
                            uint clut_y = (clut_vram_addr / 2048) - 240;
                            uint clut_x = (clut_vram_addr % 2048) / 32;

                            // Check which CLUT to use
                            if (clut_x < 0x10) {
                                clut = Utils::GetPixels(generic_cluts_texture, clut_x * 16, clut_y, 16, 1);
                            }
                            else if (clut_x < 0x20) {
                                clut_x = (polygon.clut & 0x0F);
                                clut = Utils::GetPixels(entity_cluts_texture, clut_x * 16, clut_y, 16, 1);
                            }
                            else {
                                clut_x = (polygon.clut & 0x0F);
                                clut = Utils::GetPixels(map_vram, clut_x * 16, clut_y + 240, 16, 1);
                            }

                            // Allocate space for the RGBA image
                            byte *rgba_pixels = (byte *)calloc(tex_width * tex_height * 4, sizeof(byte));

                            // Expand all pixels by their CLUT components
                            Utils::VRAM_to_RGBA(pixels, clut, tex_width / 4, tex_height, rgba_pixels);
                            Utils::HexDump(pixels, tex_width * tex_height * 4);
                            free(pixels);
                            free(clut);

                            // Create texture from thingo
                            GLuint part_texture = Utils::CreateTexture(rgba_pixels, tex_width, tex_height);
                            entity_subsprite.texture = part_texture;
                            free(rgba_pixels);

                            // Determine polygon offsets
                            entity_subsprite.offset_x = polygon.x0;
                            entity_subsprite.offset_y = polygon.y0;

                            // Determine any texture flipping
                            entity_subsprite.flip_x = (polygon.x0 > polygon.x1) ^ (polygon.u0 > polygon.u1);
                            entity_subsprite.flip_y = (polygon.y0 > polygon.y2) ^ (polygon.v0 > polygon.v2);

                            // Get width and height
                            entity_subsprite.width = polygon.u1;
                            entity_subsprite.height = polygon.v1;
                        }




                        // Check if this was a tile primitive
                        else if (polygon_type == PRIM_TYPE_TILE) {

                            // Get width and height
                            entity_subsprite.width = polygon.tile_width;
                            entity_subsprite.height = polygon.tile_height;

                            // Allocate data for the colored rectangle
                            uint data_len = entity_subsprite.width * entity_subsprite.height * 4;
                            byte* rgba_pixels = (byte*)calloc(data_len, sizeof(byte));

                            // Fill pixel array with color values
                            byte alpha = 0xFF;
                            if (entity_subsprite.semi_transparent) {
                                alpha = 0x80;
                            }
                            for (int idx = 0; idx < data_len; idx += 4) {
                                rgba_pixels[idx] = polygon.r0;
                                rgba_pixels[idx + 1] = polygon.g0;
                                rgba_pixels[idx + 2] = polygon.b0;
                                rgba_pixels[idx + 3] = alpha;
                            }

                            // Create texture from pixel data
                            entity_subsprite.texture = Utils::CreateTexture(rgba_pixels, entity_subsprite.width, entity_subsprite.height);
                            free(rgba_pixels);

                            // Determine polygon offsets
                            entity_subsprite.offset_x = polygon.x0;
                            entity_subsprite.offset_y = polygon.y0;
                        }





                        // Check if this is a 4-sided primitive
                        else if (polygon_type == PRIM_TYPE_POLYG4 || polygon_type == PRIM_TYPE_POLYGT4) {

                            // Get the leftmost coord
                            uint left = std::min(polygon.u0, polygon.u1);
                            uint top = std::min(polygon.v0, polygon.v2);

                            // Get the pixels to write
                            uint tex_width = (uint)ceil(abs(polygon.u0 - polygon.u3) / 4) * 4;
                            uint tex_height = (uint)ceil(abs(polygon.v0 - polygon.v3) / 4) * 4;

                            // If U/V isn't set, get the dimensions from X/Y coords
                            if (tex_width == 0 && tex_height == 0) {
                                tex_width = (uint)(ceil(abs(polygon.x0 - polygon.x3) / 4) * 4);
                                tex_height = (uint)(ceil(abs(polygon.y0 - polygon.y3) / 4) * 4);
                            }

                            // Get the leftmost coord
                            if (tex_width == 0 && tex_height == 0 /*|| (polygon.u2 == polygon.u3 && polygon.v2 == polygon.v3)*/) {
                                break;
                            }

                            // Set entity sprite part data
                            entity_subsprite.offset_x = std::min(polygon.x0, polygon.x1);
                            entity_subsprite.offset_y = std::min(polygon.y0, polygon.y2);

                            // Get width from X/Y coords if polygon has defined X/Y coords
                            if (entity_subsprite.offset_x != 0 || entity_subsprite.offset_y != 0) {
                                entity_subsprite.override_coords = true;
                                entity_subsprite.width = abs(polygon.x1 - polygon.x0);
                                entity_subsprite.height = abs(polygon.y2 - polygon.y0);
                            }
                            // Otherwise get it from U/V coords
                            else {
                                entity_subsprite.width = abs(polygon.u1 - polygon.u0);
                                entity_subsprite.height = abs(polygon.v2 - polygon.v0);

                                // Update X/Y to be placed relative to the entity's position
                                entity_subsprite.offset_x += entity->data.pos_x;
                                entity_subsprite.offset_y += entity->data.pos_y;
                            }

                            // Check if this is a POLY_GT4 primitive
                            if (polygon_type == PRIM_TYPE_POLYGT4) {

                                // Get the entity texture via the texture page
                                GLuint texture = 0;
                                if (polygon.tpage < 0x10) {
                                    if (polygon.tpage >= 8) {
                                        texture = map_tilesets[polygon.tpage - 8];
                                    }
                                    else {
                                        texture = map_tilesets[polygon.tpage];
                                    }
                                }
                                else {
                                    texture = cur_room->texture_pages[polygon.tpage - 0x10];
                                }

                                byte *pixels = Utils::GetPixels(texture, floor(left / 4), top, tex_width / 4, tex_height);

                                // Default to generic CLUT
                                byte* clut;

                                // Get the CLUT's location in VRAM
                                uint clut_vram_addr = *(ushort*)(MipsEmulator::ram + CLUT_INDEX_ADDR + (polygon.clut * 2)) << 5;
                                uint clut_y = (clut_vram_addr / 2048) - 240;
                                uint clut_x = (clut_vram_addr % 2048) / 32;

                                // Check which CLUT to use
                                if (clut_x < 0x10) {
                                    clut = Utils::GetPixels(generic_cluts_texture, clut_x * 16, clut_y, 16, 1);
                                }
                                else if (clut_x < 0x20) {
                                    clut_x = (polygon.clut & 0x0F);
                                    clut = Utils::GetPixels(entity_cluts_texture, clut_x * 16, clut_y, 16, 1);
                                }
                                else {
                                    clut_x = (polygon.clut & 0x0F);
                                    clut = Utils::GetPixels(map_vram, clut_x * 16, clut_y + 240, 16, 1);
                                }

                                // Allocate space for the RGBA image
                                byte *rgba_pixels = (byte *) calloc(tex_width * tex_height * 4, sizeof(byte));

                                // Expand all pixels by their CLUT components
                                Utils::VRAM_to_RGBA(pixels, clut, tex_width / 4, tex_height, rgba_pixels);
                                free(pixels);
                                free(clut);

                                // Create texture from thingo
                                GLuint part_texture = Utils::CreateTexture(rgba_pixels, tex_width, tex_height);
                                entity_subsprite.texture = part_texture;
                                free(rgba_pixels);
                            }

                            // Otherwise this was a POLY_G4 primitive
                            else {

                                // Allocate data for the colored rectangle
                                uint data_len = entity_subsprite.width * entity_subsprite.height * 4;
                                byte* rgba_pixels = (byte*)calloc(data_len, sizeof(byte));

                                // Fill pixel array with color values
                                byte alpha = 0xFF;
                                if (entity_subsprite.semi_transparent) {
                                    alpha = 0x80;
                                }

                                // Loop through each pixel
                                for (int y = 0; y < entity_subsprite.height; y++) {
                                    for (int x = 0; x < entity_subsprite.width; x++) {

                                        // Get the current offset within the image
                                        uint pix_offset = (y * entity_subsprite.width * 4) + (x * 4);

                                        // Get the progress between each axis
                                        float vert_progress = (float)y / (float)entity_subsprite.height;
                                        float horiz_progress = (float)x / (float)entity_subsprite.width;

                                        // Find the progress between each color within the rectangle
                                        byte r1 = lerp(polygon.r0, polygon.r1, horiz_progress);
                                        byte r2 = lerp(polygon.r3, polygon.r2, horiz_progress);
                                        byte avg_r = lerp(r1, r2, vert_progress);
                                        byte g1 = lerp(polygon.g0, polygon.g1, horiz_progress);
                                        byte g2 = lerp(polygon.g3, polygon.g2, horiz_progress);
                                        byte avg_g = lerp(g1, g2, vert_progress);
                                        byte b1 = lerp(polygon.b0, polygon.b1, horiz_progress);
                                        byte b2 = lerp(polygon.b3, polygon.b2, horiz_progress);
                                        byte avg_b = lerp(b1, b2, vert_progress);

                                        // Set the pixel values
                                        rgba_pixels[pix_offset + 0] = avg_r;
                                        rgba_pixels[pix_offset + 1] = avg_g;
                                        rgba_pixels[pix_offset + 2] = avg_b;
                                        rgba_pixels[pix_offset + 3] = alpha;
                                    }
                                }

                                /*
                                for (int idx = 0; idx < data_len; idx += 4) {
                                    rgba_pixels[idx] = 0xFF;
                                    rgba_pixels[idx + 1] = 0xFF;
                                    rgba_pixels[idx + 2] = 0xFF;
                                    rgba_pixels[idx + 3] = alpha;
                                }
                                */

                                // Create texture from pixel data
                                entity_subsprite.texture = Utils::CreateTexture(rgba_pixels, entity_subsprite.width, entity_subsprite.height);
                                free(rgba_pixels);
                            }

                            // Determine any texture flipping
                            entity_subsprite.flip_x = (polygon.x0 > polygon.x1) ^ (polygon.u0 > polygon.u1);
                            entity_subsprite.flip_y = (polygon.y0 > polygon.y2) ^ (polygon.v0 > polygon.v2);

                            // Check if polygon should be skewed
                            if (polygon.x0 != polygon.x2) {
                                entity_subsprite.bottom_left.x = polygon.x2 - polygon.x0;
                            }
                            if (polygon.y0 != polygon.y1) {
                                entity_subsprite.top_right.y = polygon.y1 - polygon.y0;
                            }
                            if (polygon.x1 != polygon.x3) {
                                entity_subsprite.bottom_right.x = polygon.x3 - polygon.x1;
                            }
                            if (polygon.y2 != polygon.y3) {
                                entity_subsprite.bottom_right.y = polygon.y3 - polygon.y2;
                            }
                            if (polygon.x0 != polygon.x2 || polygon.y0 != polygon.y1 || polygon.x1 != polygon.x3 || polygon.y2 != polygon.y3) {
                                entity_subsprite.skew = true;
                            }
                        }




                        // Check if this was a DR_ENV thing
                        // TODO: Do something with the drawenv
                        else if (polygon_type == PRIM_TYPE_DRENV) {

                            // Get the address of the DR_ENV structure
                            uint addr = (polygon.b1 << 16) | (polygon.g1 << 8) | polygon.r1;

                            // Grab DR_ENV data from RAM
                            DR_ENV* dr_env = (DR_ENV*)(MipsEmulator::ram + (polygon.drenv_addr - RAM_BASE_OFFSET));

                            // Allocate a new DRAWENV
                            DRAWENV drawenv;

                            // Loop through each code
                            for (int idx = 0; idx < 15; idx++) {

                                // Get the current DRAWENV code
                                uint code = dr_env->code[idx];

                                // Get the code type from the code
                                byte code_type = (code >> 24) & 0xFF;

                                switch (code_type) {
                                    case 0xE1:
                                        drawenv.tpage = code & 0x1FF;
                                        drawenv.dfe = ((code & 0x400) == 0x400 ? 1 : 0);
                                        drawenv.dtd = ((code & 0x200) == 0x200 ? 1 : 0);
                                        break;
                                    case 0xE2:
                                        drawenv.tw.w = (code & 0x1F) << 3;
                                        drawenv.tw.h = ((code >> 5) & 0x1F) << 3;
                                        drawenv.tw.x = ((code >> 10) & 0x1F) << 3;
                                        drawenv.tw.y = ((code >> 15) & 0x1F) << 3;
                                        break;
                                    case 0xE3:
                                        drawenv.clip.x = code & 0xFFF;
                                        drawenv.clip.y = (code >> 0xC) & 0xFFF;
                                        break;
                                    case 0xE4:
                                        drawenv.clip.w = (code & 0xFFF) - drawenv.clip.x + 1;
                                        drawenv.clip.h = ((code >> 0xC) & 0xFFF) - drawenv.clip.y + 1;
                                        break;
                                    case 0xE5:
                                        drawenv.ofs[0] = code & 0xFFF;
                                        drawenv.ofs[1] = (code >> 0xC) & 0xFFF;
                                        break;
                                    case 0xE6:
                                    default:
                                        break;
                                }
                            }
                        }




                        // Check if this was a 3-sided primitive
                        else if (polygon_type == PRIM_TYPE_POLYGT3) {

                            // Get the leftmost coord
                            byte left = std::min(polygon.u0, polygon.u1);
                            left = std::min(left, polygon.u2);
                            byte top = std::min(polygon.v0, polygon.v1);
                            top = std::min(top, polygon.v2);
                            byte right = std::max(polygon.u0, polygon.u1);
                            right = std::max(right, polygon.u2);
                            byte bottom = std::max(polygon.v0, polygon.v1);
                            bottom = std::max(bottom, polygon.v2);

                            // Get the pixels to write
                            uint tex_width = (uint)ceil(abs(left - right) / 4) * 4;
                            uint tex_height = (uint)ceil(abs(top - bottom) / 4) * 4;

                            // Set entity sprite part data
                            short min_x = std::min(polygon.x0, polygon.x1);
                            min_x = std::min(min_x, polygon.x2);
                            short min_y = std::min(polygon.y0, polygon.y1);
                            min_y = std::min(min_y, polygon.y2);
                            short max_x = std::max(polygon.x0, polygon.x1);
                            max_x = std::max(max_x, polygon.x2);
                            short max_y = std::max(polygon.y0, polygon.y1);
                            max_y = std::max(max_y, polygon.y2);

                            // If U/V isn't set, get the dimensions from X/Y coords
                            if (tex_width == 0 && tex_height == 0) {
                                tex_width = (uint)(ceil(abs(max_x - min_x) / 4) * 4);
                                tex_height = (uint)(ceil(abs(max_y - min_y) / 4) * 4);
                            }

                            // Get the leftmost coord
                            if (tex_width == 0 && tex_height == 0 /*|| (polygon.u2 == polygon.u3 && polygon.v2 == polygon.v3)*/) {
                                break;
                            }

                            entity_subsprite.offset_x = min_x;
                            entity_subsprite.offset_y = min_y;

                            // Get width from X/Y coords if polygon has defined X/Y coords
                            if (entity_subsprite.offset_x != 0 || entity_subsprite.offset_y != 0) {
                                entity_subsprite.override_coords = true;
                                entity_subsprite.width = abs(max_x - min_x);
                                entity_subsprite.height = abs(max_y - min_y);
                            }
                                // Otherwise get it from U/V coords
                            else {
                                entity_subsprite.width = abs(left - right);
                                entity_subsprite.height = abs(top - bottom);

                                // Update X/Y to be placed relative to the entity's position
                                entity_subsprite.offset_x += entity->data.pos_x;
                                entity_subsprite.offset_y += entity->data.pos_y;
                            }

                            // Get the entity texture via the texture page
                            GLuint texture = 0;
                            if (polygon.tpage < 0x10) {
                                if (polygon.tpage >= 8) {
                                    texture = map_tilesets[polygon.tpage - 8];
                                }
                                else {
                                    texture = map_tilesets[polygon.tpage];
                                }
                            }
                            else {
                                texture = cur_room->texture_pages[polygon.tpage - 0x10];
                            }

                            byte *pixels = Utils::GetPixels(texture, floor(left / 4), top, right / 4, bottom);

                            // Default to generic CLUT
                            byte* clut;

                            // Get the CLUT's location in VRAM
                            uint clut_vram_addr = *(ushort*)(MipsEmulator::ram + CLUT_INDEX_ADDR + (polygon.clut * 2)) << 5;
                            uint clut_y = (clut_vram_addr / 2048) - 240;
                            uint clut_x = (clut_vram_addr % 2048) / 32;

                            // Check which CLUT to use
                            if (clut_x < 0x10) {
                                clut = Utils::GetPixels(generic_cluts_texture, clut_x * 16, clut_y, 16, 1);
                            }
                            else if (clut_x < 0x20) {
                                clut_x = (polygon.clut & 0x0F);
                                clut = Utils::GetPixels(entity_cluts_texture, clut_x * 16, clut_y, 16, 1);
                            }
                            else {
                                clut_x = (polygon.clut & 0x0F);
                                clut = Utils::GetPixels(map_vram, clut_x * 16, clut_y + 240, 16, 1);
                            }

                            // Allocate space for the RGBA image
                            byte *rgba_pixels = (byte *) calloc(tex_width * tex_height * 4, sizeof(byte));

                            // Expand all pixels by their CLUT components
                            Utils::VRAM_to_RGBA(pixels, clut, tex_width / 4, tex_height, rgba_pixels);
                            free(pixels);
                            free(clut);

                            // Create texture from thingo
                            GLuint part_texture = Utils::CreateTexture(rgba_pixels, tex_width, tex_height);
                            entity_subsprite.texture = part_texture;
                            free(rgba_pixels);
                        }




                        // Check if this was a line primitive
                        else if (polygon_type == PRIM_TYPE_LINEG2) {

                            // Do nothing
                        }





                        // Set sprite's absolute coords
                        entity_subsprite.x = entity_subsprite.offset_x;
                        entity_subsprite.y = entity_subsprite.offset_y;

                        // Set anchor point for transformations
                        uint hitbox_width = (entity->data.hitbox_width > 0 ? entity->data.hitbox_width : 16);
                        uint hitbox_height = (entity->data.hitbox_height > 0 ? entity->data.hitbox_height : 16);
                        entity_subsprite.anchor_x = entity->data.pos_x + (hitbox_width / 2);
                        entity_subsprite.anchor_y = entity->data.pos_y + (hitbox_height / 2);

                        // Determine transformations
                        entity_subsprite.rotate = entity->data.rotation;

                        // Determine blend mode
                        if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                            entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                        }
                        else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                            entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                        }





                        // Add the polygon to the sprite
                        entity_subsprite.polygon = polygon;

                        // Add the sprite part to the list of entity sprites
                        entity->sprites.push_back(entity_subsprite);

                        // Bail if tag is zero
                        if (polygon.tag == 0) {
                            break;
                        }

                        // Otherwise set the tag to the new target address
                        polygon_addr = polygon.tag - RAM_BASE_OFFSET;
                    }

                    // Reverse the textures
                    std::reverse(entity->sprites.begin(), entity->sprites.end());
                }
            }

            // Process item entities
            if (entity->data.sprite_bank == 3) {

                // Create an image with default dimensions
                SpritePart image;
                image.height = 16;
                image.width = 16;
                image.offset_x = -8;
                image.offset_y = -8;

// -- Candles --------------------------------------------------------------------------------------------------

                // Check if this was a candle
                if (entity->data.object_id == 1 && entity->data.blend_mode == 0x70) {

                    // Create a new entity sprite
                    EntitySpritePart entity_subsprite;

                    // Set candle image data
                    image.height = 24;
                    image.width = 24;
                    image.offset_x = -12;
                    image.offset_y = -12;

                    // Attach to the tileset
                    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fgame_textures[6], 0);

                    // Set entity sprite part data
                    entity_subsprite.offset_x = image.offset_x;
                    entity_subsprite.offset_y = image.offset_y;
                    entity_subsprite.width = image.width;
                    entity_subsprite.height = image.height;

                    // Set sprite's absolute coords
                    entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                    entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                    // Read the candle image from VRAM
                    byte* pixels = Utils::GetPixels(fgame_textures[6], 0x80 / 4, 0x80, image.width / 4, image.height);
                    /*
                    byte* pixels = (byte*)calloc((image.width / 4) * image.height * 4, sizeof(byte));
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    glReadPixels(
                            0x80 / 4, 0x80,
                            image.width / 4, image.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels
                    );
                    */

                    // Get the CLUT offset
                    uint clut_offset = CANDLE_CLUT * 16 * 4;

                    // Get the appropriate RGBA CLUT
                    byte* rgba_clut = generic_rgba_cluts + clut_offset;

                    // Allocate space for the RGBA image
                    byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                    // Expand all pixels by their CLUT components
                    Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                    free(pixels);

                    // Check if pixels have any transparency
                    for (int z = 0; z < image.width * image.height; z++) {
                        byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                        // Flag the sprite as being blendable
                        if (alpha == 0x80) {
                            entity_subsprite.blend = true;
                        }
                    }

                    // Create texture from thingo
                    GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                    entity_subsprite.texture = part_texture;
                    free(rgba_pixels);

                    // Determine any texture flipping
                    entity_subsprite.flip_y = ((image.flags & 1) == 1);
                    entity_subsprite.flip_x = ((image.flags & 2) == 2);

                    // Determine transformations
                    entity_subsprite.rotate = entity->data.rotation;

                    // Determine blend mode
                    if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                        entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                    }
                    else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                        entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                    }

                    // Determine ordering table index
                    entity_subsprite.ot_layer = entity->data.z_depth;

                    // Add the sprite part to the list of entity sprites
                    entity->sprites.push_back(entity_subsprite);
                }

// -- Pickups --------------------------------------------------------------------------------------------------

                // Check if this was a pickup
                else if (entity->data.object_id == TYPE_PICKUP) {

                    // Create a new entity sprite
                    EntitySpritePart entity_subsprite;

                    // Check if this was a Life Max Up or Heart Max Up
                    if (entity->data.initial_state == LIFE_MAX_UP_ID || entity->data.initial_state == HEART_MAX_UP_ID) {

                        // X/Y coords in the tileset
                        uint x_coord = 0;
                        uint y_coord = 0;
                        uint clut = 0;

                        // Life Max Up
                        if (entity->data.initial_state == LIFE_MAX_UP_ID) {
                            x_coord = 112;
                            y_coord = 16;
                            clut = LIFE_MAX_UP_CLUT;
                            entity->name = "Life Max Up";
                        }
                        // Heart Max Up
                        else {
                            x_coord = 0;
                            y_coord = 48;
                            clut = HEART_MAX_UP_CLUT;
                            entity->name = "Heart Max Up";
                        }

                        byte* pixels = Utils::GetPixels(
                            fgame_textures[6],
                            (0x80 + x_coord) / 4, (0x80 + y_coord),
                            image.width / 4, image.height
                        );

                        // Set entity sprite part data
                        entity_subsprite.offset_x = image.offset_x;
                        entity_subsprite.offset_y = image.offset_y;
                        entity_subsprite.width = image.width;
                        entity_subsprite.height = image.height;

                        // Set sprite's absolute coords
                        entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                        entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                        // Get the CLUT offset
                        uint clut_offset = clut * 16 * 4;

                        // Get the appropriate RGBA CLUT
                        byte* rgba_clut = generic_rgba_cluts + clut_offset;

                        // Allocate space for the RGBA image
                        byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                        // Expand all pixels by their CLUT components
                        Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                        free(pixels);

                        // Check if pixels have any transparency
                        for (int z = 0; z < image.width * image.height; z++) {
                            byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                            // Flag the sprite as being blendable
                            if (alpha == 0x80) {
                                entity_subsprite.blend = true;
                            }
                        }

                        // Create texture from thingo
                        GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                        entity_subsprite.texture = part_texture;
                        free(rgba_pixels);

                        // Determine any texture flipping
                        entity_subsprite.flip_y = ((image.flags & 1) == 1);
                        entity_subsprite.flip_x = ((image.flags & 2) == 2);

                        // Determine transformations
                        entity_subsprite.rotate = entity->data.rotation;

                        // Determine blend mode
                        if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                            entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                        }
                        else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                            entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                        }

                        // Determine ordering table index
                        entity_subsprite.ot_layer = entity->data.z_depth;

                        // Add the sprite part to the list of entity sprites
                        entity->sprites.push_back(entity_subsprite);
                    }

// -- Items ----------------------------------------------------------------------------------------------------

                    // Process the data as an item
                    else {

                        // The item index is determined by the entity's initial state
                        uint item_entry = entity->data.initial_state & 0x7FFF;

                        // Address of item data
                        uint data_addr = 0;

                        // Size of item entry (weapon vs. equipment)
                        uint data_size = 0;

                        // Check if this was a weapon item
                        if (item_entry < 0xA9) {
                            data_addr = WEAPON_NAMES_DESC_ADDR + (item_entry * 0x34);
                            data_size = 0x2C;
                        }
                        // Otherwise this was an equipment item
                        else {
                            data_addr = EQUIP_NAMES_DESC_ADDR + ((item_entry - 0xA9) * 0x20);
                            data_size = 0x18;
                        }

                        // Get the address of the name and description text
                        uint name_addr = MipsEmulator::ReadIntFromRAM(data_addr) - RAM_BASE_OFFSET;
                        uint desc_addr = MipsEmulator::ReadIntFromRAM(data_addr + 4) - RAM_BASE_OFFSET;

                        // Format the name correctly
                        entity->name = Utils::ReadSotnString(MipsEmulator::ram + name_addr);
                        entity->desc = std::string((char*)MipsEmulator::ram + desc_addr);
                        Utils::SJIS_to_ASCII(&entity->desc);

                        // Get the item ID and CLUT
                        uint data = MipsEmulator::ReadIntFromRAM(data_addr + data_size);
                        uint clut_id = (data >> 16) & 0xFFFF;
                        uint item_id = data & 0xFFFF;

                        byte* pixels = Utils::GetPixels(
                            item_textures[item_id],
                            0, 0,
                            image.width / 4, image.height
                        );

                        // Set entity sprite part data
                        entity_subsprite.offset_x = image.offset_x;
                        entity_subsprite.offset_y = image.offset_y;
                        entity_subsprite.width = image.width;
                        entity_subsprite.height = image.height;

                        // Set sprite's absolute coords
                        entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                        entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                        // Get the CLUT offset
                        uint clut_offset = clut_id * 16 * 4;

                        // Get the appropriate RGBA CLUT
                        byte* rgba_clut = item_cluts + clut_offset;

                        // Allocate space for the RGBA image
                        byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                        // Expand all pixels by their CLUT components
                        Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                        free(pixels);

                        // Check if pixels have any transparency
                        for (int z = 0; z < image.width * image.height; z++) {
                            byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                            // Flag the sprite as being blendable
                            if (alpha == 0x80) {
                                entity_subsprite.blend = true;
                            }
                        }

                        // Create texture from thingo
                        GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                        entity_subsprite.texture = part_texture;
                        free(rgba_pixels);

                        // Determine any texture flipping
                        entity_subsprite.flip_y = ((image.flags & 1) == 1);
                        entity_subsprite.flip_x = ((image.flags & 2) == 2);

                        // Determine transformations
                        entity_subsprite.rotate = entity->data.rotation;

                        // Determine blend mode
                        if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                            entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                        }
                        else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                            entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                        }

                        // Determine ordering table index
                        entity_subsprite.ot_layer = entity->data.z_depth;

                        // Add the sprite part to the list of entity sprites
                        entity->sprites.push_back(entity_subsprite);
                    }
                }

// -- Relics ---------------------------------------------------------------------------------------------------

                else if (entity->data.object_id == TYPE_RELIC) {

                    // Create a new entity sprite
                    EntitySpritePart entity_subsprite;

                    // The relic index is determined by the entity's initial state
                    uint relic_index = entity->data.initial_state & 0x7FFF;

                    // Address of relic data
                    uint data_addr = RELIC_TABLE_ADDR + (relic_index * 16);

                    // Get the address of the name and description text
                    uint name_addr = MipsEmulator::ReadIntFromRAM(data_addr) - RAM_BASE_OFFSET;
                    uint desc_addr = MipsEmulator::ReadIntFromRAM(data_addr + 4) - RAM_BASE_OFFSET;

                    // Format the name correctly
                    entity->name = std::string((char*)MipsEmulator::ram + name_addr);
                    entity->desc = std::string((char*)MipsEmulator::ram + desc_addr);
                    Utils::SJIS_to_ASCII(&entity->name);
                    Utils::SJIS_to_ASCII(&entity->desc);
                    if (entity->name[0] == ' ') {
                        entity->name.assign((char*)&entity->name.c_str()[1]);
                    }

                    // Get the relic ID and CLUT (skip the name and desc offsets)
                    uint data = MipsEmulator::ReadIntFromRAM(data_addr + 8);
                    uint clut_id = (data >> 16) & 0xFFFF;
                    uint relic_id = data & 0xFFFF;

                    // Attach to the tileset
                    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, item_textures[relic_id], 0);

                    // Set entity sprite part data
                    entity_subsprite.offset_x = image.offset_x;
                    entity_subsprite.offset_y = image.offset_y;
                    entity_subsprite.width = image.width;
                    entity_subsprite.height = image.height;

                    // Set sprite's absolute coords
                    entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                    entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                    // Read the candle image from VRAM
                    byte* pixels = Utils::GetPixels(item_textures[relic_id], 0, 0, image.width / 4, image.height);
                    /*
                    byte* pixels = (byte*)calloc((image.width / 4) * image.height * 4, sizeof(byte));
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    glReadPixels(
                            0, 0,
                            image.width / 4, image.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels
                    );
                    */

                    // Get the CLUT offset
                    uint clut_offset = clut_id * 16 * 4;

                    // Get the appropriate RGBA CLUT
                    byte* rgba_clut = item_cluts + clut_offset;

                    // Allocate space for the RGBA image
                    byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                    // Expand all pixels by their CLUT components
                    Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                    free(pixels);

                    // Check if pixels have any transparency
                    for (int z = 0; z < image.width * image.height; z++) {
                        byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                        // Flag the sprite as being blendable
                        if (alpha == 0x80) {
                            entity_subsprite.blend = true;
                        }
                    }

                    // Create texture from thingo
                    GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                    entity_subsprite.texture = part_texture;
                    free(rgba_pixels);

                    // Determine any texture flipping
                    entity_subsprite.flip_y = ((image.flags & 1) == 1);
                    entity_subsprite.flip_x = ((image.flags & 2) == 2);

                    // Determine transformations
                    entity_subsprite.rotate = entity->data.rotation;

                    // Determine blend mode
                    if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                        entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                    }
                    else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                        entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                    }

                    // Determine ordering table index
                    entity_subsprite.ot_layer = entity->data.z_depth;

                    // Add the sprite part to the list of entity sprites
                    entity->sprites.push_back(entity_subsprite);
                }
            }

// -- Non-Items --------------------------------------------------------------------------------------------------

            // Process non-item entities
            else if (entity->data.sprite_bank != 0) {

                // Address of entity data
                uint data_addr = ENEMY_DATA_ADDR + (entity->data.info_idx * 0x28);

                // Get the address of the name and description text
                uint name_addr = MipsEmulator::ReadIntFromRAM(data_addr) - RAM_BASE_OFFSET;

                // Format the name correctly
                entity->name = Utils::ReadSotnString(MipsEmulator::ram + name_addr);

                // Sprite banks > 0x8000 indicate to use the map's sprite banks
                if (entity->data.sprite_bank > 0x8000) {
                    uint bank_idx = entity->data.sprite_bank & 0x7FFF;
                    std::vector<Sprite> sprite_bank = sprite_banks[bank_idx];

                    Sprite sprite;

                    // Failsafe for 0xFF sprite image in RNO3
                    if (entity->data.sprite_image == 0xFF) {
                        sprite = sprite_bank[1];
                    }
                    else {
                        // Failsafe for rocks in WRP
                        if (entity->data.sprite_image < sprite_bank.size()) {
                            sprite = sprite_bank[entity->data.sprite_image];
                        }
                    }

                    // Store the address of the sprite
                    entity->sprite_address = sprite.address;

                    // Use map tilesets if dedicated tileset does not exist
                    if (entity->data.tileset == 0) {

                        // Loop through each sprite part
                        for (int m = 0; m < sprite.parts.size(); m++) {

                            // Create a new entity sprite
                            EntitySpritePart entity_subsprite;

                            // Get the current sprite part
                            SpritePart image = sprite.parts[m];

                            // Get the tileset index
                            uint tileset_idx = (image.tileset_offset % 0x20) / 4;

                            // Get the entity tileset (accounting for texture page)
                            GLuint entity_tileset = map_tilesets[tileset_idx];

                            // Attach to the tileset
                            //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entity_tileset, 0);

                            // Set entity sprite part data
                            entity_subsprite.offset_x = image.offset_x;
                            entity_subsprite.offset_y = image.offset_y;
                            entity_subsprite.width = image.width;
                            entity_subsprite.height = image.height;

                            // Set sprite's absolute coords
                            entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                            entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                            // Read the image from VRAM

                            byte* pixels = Utils::GetPixels(entity_tileset, image.texture_start_x / 4, image.texture_start_y, image.width / 4, image.height);
                            /*
                            byte* pixels = (byte*)calloc((image.width / 4) * image.height * 4, sizeof(byte));
                            glReadBuffer(GL_COLOR_ATTACHMENT0);
                            glReadPixels(
                                    image.texture_start_x / 4, image.texture_start_y,
                                    image.width / 4, image.height,
                                    GL_RGBA, GL_UNSIGNED_BYTE, pixels
                            );
                            */

                            // Calculate the CLUT coordinates in VRAM
                            uint clut_offset_x = (image.clut_offset & 0x0F) * 16;
                            uint clut_offset_y = (image.clut_offset >> 4) & 0x0F;

                            // Directly pull the RGBA CLUT data from VRAM
                            byte* rgba_clut = Utils::GetPixels(map_vram, clut_offset_x, 240 + clut_offset_y, 16, 1);
                            /*
                            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, map_vram, 0);
                            byte* rgba_clut = (byte*)calloc(16 * 4, sizeof(byte));
                            glReadBuffer(GL_COLOR_ATTACHMENT0);
                            glReadPixels(
                                    clut_offset_x, 240 + clut_offset_y,
                                    16, 1,
                                    GL_RGBA, GL_UNSIGNED_BYTE, rgba_clut
                            );
                            */


                            // Allocate space for the RGBA image
                            byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                            // Expand all pixels by their CLUT components
                            Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                            free(pixels);
                            free(rgba_clut);

                            // Check if pixels should be filled
                            bool r_fill, g_fill, b_fill;
                            if (entity->data.transform_flags & EntityTransform_Fill) {
                                // Get RGB values
                                r_fill = (entity->data.transform_flags & EntityTransform_Red) == EntityTransform_Red;
                                g_fill = (entity->data.transform_flags & EntityTransform_Green) == EntityTransform_Green;
                                b_fill = (entity->data.transform_flags & EntityTransform_Blue) == EntityTransform_Blue;
                            }

                            // Check if pixels have any transparency or fill
                            for (int z = 0; z < image.width * image.height; z++) {
                                byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                                // Flag the sprite as being blendable
                                if (alpha == 0x80 && entity->data.blend_mode > 0) {
                                    entity_subsprite.blend = true;
                                }
                            }

                            // Loop through each pixel
                            for (int z = 0; z < image.width * image.height; z++) {
                                // Destroy any partial alpha values if no blending is enabled
                                if (!entity_subsprite.blend) {
                                    if (*(byte*)(rgba_pixels + (z * 4) + 3) == 0x80) {
                                        *(byte*)(rgba_pixels + (z * 4) + 3) = 0xFF;
                                    }
                                }
                                /*
                                // Otherwise apply fill data if any exists
                                else if ((entity->data.transform_flags & EntityTransform_Fill) == EntityTransform_Fill) {
                                    *(byte*)(rgba_pixels + (z * 4) + 0) = r_fill * 0xFF;
                                    *(byte*)(rgba_pixels + (z * 4) + 1) = g_fill * 0xFF;
                                    *(byte*)(rgba_pixels + (z * 4) + 2) = b_fill * 0xFF;
                                }
                                */
                            }

                            // Create texture from thingo
                            GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                            entity_subsprite.texture = part_texture;
                            free(rgba_pixels);

                            // Determine any texture flipping
                            entity_subsprite.flip_y = ((image.flags & 1) == 1);
                            entity_subsprite.flip_x = ((image.flags & 2) == 2);

                            // Determine transformations
                            entity_subsprite.rotate = entity->data.rotation;

                            // Determine blend mode
                            if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                                entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                            }
                            else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                                entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                            }

                            // Determine ordering table index
                            entity_subsprite.ot_layer = entity->data.z_depth;

                            // Add the sprite part to the list of entity sprites
                            entity->sprites.push_back(entity_subsprite);
                        }

                        // Reverse the textures
                        std::reverse(entity->sprites.begin(), entity->sprites.end());
                    }

                    // Check if entity tileset exists (e.g. enemies)
                    else {

                        // Loop through each sprite part
                        for (int m = 0; m < sprite.parts.size(); m++) {

                            // Create a new entity sprite
                            EntitySpritePart entity_subsprite;

                            // Get the current sprite part
                            SpritePart image = sprite.parts[m];

                            // Get the entity tileset (accounting for texture page)
                            GLuint entity_tileset = cur_room->entity_tilesets[entity->data.tileset + image.tileset_offset];

                            // Attach to the tileset
                            //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entity_tileset, 0);

                            // Set entity sprite part data
                            entity_subsprite.offset_x = image.offset_x;
                            entity_subsprite.offset_y = image.offset_y;
                            entity_subsprite.width = image.width;
                            entity_subsprite.height = image.height;

                            // Set sprite's absolute coords
                            entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                            entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                            // Read the image from VRAM
                            byte* pixels = Utils::GetPixels(entity_tileset, image.texture_start_x / 4, image.texture_start_y, image.width / 4, image.height);
                            /*
                            byte* pixels = (byte*)calloc((image.width / 4) * image.height * 4, sizeof(byte));
                            glReadBuffer(GL_COLOR_ATTACHMENT0);
                            glReadPixels(
                                image.texture_start_x / 4, image.texture_start_y,
                                image.width / 4, image.height,
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels
                            );
                            */

                            // Select the appropriate CLUT
                            uint clut_offset = entity->data.clut_index;
                            if (clut_offset < 0x8000) {
                                clut_offset += image.clut_offset;
                            }
                            else {
                                clut_offset &= 0x7FFF;
                            }
                            byte* clut = (byte*)calloc(32, sizeof(byte));
                            MipsEmulator::CopyFromRAM(CLUT_BASE_ADDR + (clut_offset * 32), clut, 32);

                            // Allocate space for the RGBA image
                            byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                            // Allocate space for RGBA CLUT
                            byte* rgba_clut = (byte*)calloc(16 * 4, sizeof(byte));

                            // Convert RGB1555 CLUT to RGBA CLUT
                            Utils::CLUT_to_RGBA(clut, rgba_clut, 1, true);
                            free(clut);

                            // Expand all pixels by their CLUT components
                            Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                            free(pixels);
                            free(rgba_clut);

                            // Check if pixels should be filled
                            bool r_fill, g_fill, b_fill;
                            if (entity->data.transform_flags & EntityTransform_Fill) {
                                // Get RGB values
                                r_fill = (entity->data.transform_flags & EntityTransform_Red) == EntityTransform_Red;
                                g_fill = (entity->data.transform_flags & EntityTransform_Green) == EntityTransform_Green;
                                b_fill = (entity->data.transform_flags & EntityTransform_Blue) == EntityTransform_Blue;
                            }

                            // Check if pixels have any transparency or fill
                            for (int z = 0; z < image.width * image.height; z++) {
                                byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                                // Flag the sprite as being blendable
                                if (alpha == 0x80 && entity->data.blend_mode > 0) {
                                    entity_subsprite.blend = true;
                                }
                            }

                            // Loop through each pixel
                            for (int z = 0; z < image.width * image.height; z++) {
                                // Destroy any partial alpha values if no blending is enabled
                                if (!entity_subsprite.blend) {
                                    if (*(byte*)(rgba_pixels + (z * 4) + 3) == 0x80) {
                                        *(byte*)(rgba_pixels + (z * 4) + 3) = 0xFF;
                                    }
                                }
                                /*
                                // Otherwise apply fill data if any exists
                                else if ((entity->data.transform_flags & EntityTransform_Fill) == EntityTransform_Fill) {
                                    *(byte*)(rgba_pixels + (z * 4) + 0) = r_fill * 0xFF;
                                    *(byte*)(rgba_pixels + (z * 4) + 1) = g_fill * 0xFF;
                                    *(byte*)(rgba_pixels + (z * 4) + 2) = b_fill * 0xFF;
                                }
                                */
                            }

                            // Create texture from thingo
                            GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                            entity_subsprite.texture = part_texture;
                            free(rgba_pixels);

                            // Determine any texture flipping
                            entity_subsprite.flip_y = ((image.flags & 1) == 1);
                            entity_subsprite.flip_x = ((image.flags & 2) == 2);

                            // Determine transformations
                            entity_subsprite.rotate = entity->data.rotation;

                            // Determine blend mode
                            if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                                entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                            }
                            else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                                entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                            }

                            // Determine ordering table index
                            entity_subsprite.ot_layer = entity->data.z_depth;

                            // Add the sprite part to the list of entity sprites
                            entity->sprites.push_back(entity_subsprite);
                        }

                        // Reverse the textures
                        std::reverse(entity->sprites.begin(), entity->sprites.end());
                    }
                }

// -- Default Sprite Banks ---------------------------------------------------------------------------------------

                // Sprite banks < 0x8000 indicate to use the default sprite banks
                else if (entity->data.sprite_bank < 0x8000) {

                    std::vector<Sprite> sprite_bank;
                    Sprite sprite;

                    // Check if this is a door
                    if (entity->data.object_id == 5) {

                        sprite_bank = generic_sprite_banks[7];
                        sprite = sprite_bank[1];
                    }

                    else {

                        uint bank_idx = entity->data.sprite_bank & 0x7FFF;

                        // Use the entity's frame index as an index into the bank instead of the bank index
                        sprite_bank = generic_sprite_banks[bank_idx];
                        sprite = sprite_bank[entity->data.sprite_image];
                    }

                    // Store the address of the sprite
                    entity->sprite_address = sprite.address;

                    // Loop through each sprite part
                    for (int m = 0; m < sprite.parts.size(); m++) {

                        // Create a new entity sprite
                        EntitySpritePart entity_subsprite;

                        // Get the current sprite part
                        SpritePart image = sprite.parts[m];

                        // Set the default tileset to the load room texture
                        GLuint entity_tileset = generic_loadroom_texture;

                        // Set the texture start/end addresses
                        uint tex_start_x = image.texture_start_x;
                        uint tex_start_y = image.texture_start_y;

                        // Check if powerup texture should be used
                        if ((cur_room->fg_layer.load_flags & 0x60) == 0x60) {
                            entity_tileset = generic_powerup_texture;
                            tex_start_x %= 0x80;
                        }

                        // Check if save room texture should be used
                        else if ((cur_room->fg_layer.load_flags & 0x20) == 0x20) {
                            entity_tileset = generic_saveroom_texture;
                        }

                        // Use the 7th generic tileset for red doors
                        else if (entity->data.object_id == 5) {
                            entity_tileset = fgame_textures[7];
                        }

                        // Adjust texture offsets if generic stuff is being loaded
                        if ((cur_room->fg_layer.load_flags & 0x60) > 0) {
                            tex_start_x %= 0x80;
                            tex_start_y %= 0x80;
                        }

                        // Attach to the tileset
                        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entity_tileset, 0);

                        // Set entity sprite part data
                        entity_subsprite.offset_x = image.offset_x;
                        entity_subsprite.offset_y = image.offset_y;
                        entity_subsprite.width = image.width;
                        entity_subsprite.height = image.height;

                        // Set sprite's absolute coords
                        entity_subsprite.x = entity_subsprite.offset_x + entity->data.pos_x;
                        entity_subsprite.y = entity_subsprite.offset_y + entity->data.pos_y;

                        // Read the image from VRAM
                        byte* pixels = Utils::GetPixels(entity_tileset, tex_start_x / 4, tex_start_y, image.width / 4, image.height);
                        /*
                        byte* pixels = (byte*)calloc((image.width / 4) * image.height * 4, sizeof(byte));
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                        glReadPixels(
                            tex_start_x / 4, tex_start_y,
                            image.width / 4, image.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels
                        );
                        */

                        // Get the CLUT offset from the generic clut address
                        uint clut_offset = (image.clut_offset % 256) * 16 * 4;

                        // Get the appropriate RGBA CLUT
                        byte* rgba_clut = generic_rgba_cluts + clut_offset;

                        // Allocate space for the RGBA image
                        byte* rgba_pixels = (byte*)calloc(image.width * image.height * 4, sizeof(byte));

                        // Expand all pixels by their CLUT components
                        Utils::VRAM_to_RGBA(pixels, rgba_clut, image.width / 4, image.height, rgba_pixels);
                        free(pixels);

                        // Check if pixels should be filled
                        bool r_fill, g_fill, b_fill;
                        if (entity->data.transform_flags & EntityTransform_Fill) {
                            // Get RGB values
                            r_fill = (entity->data.transform_flags & EntityTransform_Red) == EntityTransform_Red;
                            g_fill = (entity->data.transform_flags & EntityTransform_Green) == EntityTransform_Green;
                            b_fill = (entity->data.transform_flags & EntityTransform_Blue) == EntityTransform_Blue;
                        }

                        // Check if pixels have any transparency
                        for (int z = 0; z < image.width * image.height; z++) {
                            byte alpha = *(byte*)(rgba_pixels + (z * 4) + 3);
                            // Flag the sprite as being blendable
                            if (alpha == 0x80) {
                                entity_subsprite.blend = true;
                            }
                        }

                        // Loop through each pixel
                        for (int z = 0; z < image.width * image.height; z++) {
                            // Destroy any partial alpha values if no blending is enabled
                            if (!entity_subsprite.blend) {
                                if (*(byte*)(rgba_pixels + (z * 4) + 3) == 0x80) {
                                    *(byte*)(rgba_pixels + (z * 4) + 3) = 0xFF;
                                }
                            }
                            /*
                            // Otherwise apply fill data if any exists
                            else if ((entity->data.transform_flags & EntityTransform_Fill) == EntityTransform_Fill) {
                                *(byte*)(rgba_pixels + (z * 4) + 0) = r_fill * 0xFF;
                                *(byte*)(rgba_pixels + (z * 4) + 1) = g_fill * 0xFF;
                                *(byte*)(rgba_pixels + (z * 4) + 2) = b_fill * 0xFF;
                            }
                            */
                        }

                        // Create texture from thingo
                        GLuint part_texture = Utils::CreateTexture(rgba_pixels, image.width, image.height);
                        entity_subsprite.texture = part_texture;
                        free(rgba_pixels);

                        // Determine any texture flipping
                        entity_subsprite.flip_y = ((image.flags & 1) == 1);
                        entity_subsprite.flip_x = ((image.flags & 2) == 2);

                        // Determine transformations
                        entity_subsprite.rotate = entity->data.rotation;

                        // Determine blend mode
                        if ((entity->data.blend_mode & BLEND_MODE_UNK70) == BLEND_MODE_UNK70) {
                            entity_subsprite.blend_mode = BLEND_MODE_UNK70;
                        }
                        else if ((entity->data.blend_mode & BLEND_MODE_LIGHTEN) == BLEND_MODE_LIGHTEN) {
                            entity_subsprite.blend_mode = BLEND_MODE_LIGHTEN;
                        }

                        // Determine ordering table index
                        entity_subsprite.ot_layer = entity->data.z_depth;

                        // Add the sprite part to the list of entity sprites
                        entity->sprites.push_back(entity_subsprite);
                    }

                    // Reverse the textures
                    std::reverse(entity->sprites.begin(), entity->sprites.end());
                }
            }

            // Add the entity's sprites to the OT
            for (int k = 0; k < entity->sprites.size(); k++) {

                // Get the current sprite
                EntitySpritePart* cur_sprite = &entity->sprites[k];

                // Check if sprite should be added to the background ordering table
                if (cur_sprite->ot_layer < cur_room->bg_layer.z_index) {
                    cur_room->bg_ordering_table[cur_sprite->ot_layer].push_back(*cur_sprite);
                }
                // Check if sprite should be added to the middle ordering table
                else if (cur_sprite->ot_layer < cur_room->fg_layer.z_index) {
                    cur_room->mid_ordering_table[cur_sprite->ot_layer].push_back(*cur_sprite);
                }
                // Otherwise add the sprite to the foreground ordering table
                else {
                    cur_room->fg_ordering_table[cur_sprite->ot_layer].push_back(*cur_sprite);
                }
            }

            // Add the entity to the room's entity list
            cur_room->entities.push_back(*entity);
        }
        std::reverse(cur_room->entities.begin(), cur_room->entities.end());
    }

    // Reset the framebuffer target to the main window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /*
    // Delete the framebuffer
    glDeleteFramebuffers(1, &fbo);
    */
}



/**
 * Cleans up the object and frees allocated memory.
 */
void Map::Cleanup() {

    // Delete all room layer textures
    for (int i = 0; i < rooms.size(); i++) {
        Room* cur_room = &rooms[i];
        for(const auto& data : cur_room->entity_tilesets) {
            glDeleteTextures(1, &cur_room->entity_tilesets[data.first]);
        }
        glDeleteTextures(1, &cur_room->fg_texture);
        glDeleteTextures(1, &cur_room->bg_texture);

        // Delete all entity textures
        for (int k = 0; k < cur_room->entities.size(); k++) {
            Entity* cur_entity = &cur_room->entities[k];
            for (int m = 0; m < cur_entity->sprites.size(); m++) {
                glDeleteTextures(1, &cur_entity->sprites[m].texture);
            }
        }

        // Free all allocated layer memory
        free(cur_room->fg_layer.tile_indices);
        free(cur_room->bg_layer.tile_indices);

        // Delete VRAM stuff
        cur_room->texture_pages.clear();
        glDeleteTextures(1, &cur_room->vram);
        glDeleteTextures(1, &cur_room->expanded_vram);
    }

    // Free all tile data pointers
    for (auto const& entry : tile_data_pointers) {
        free(entry.second.tileset_ids);
        free(entry.second.tile_positions);
        free(entry.second.clut_ids);
        free(entry.second.collision_ids);
    }

    // Clear out the tile data pointers
    tile_data_pointers.clear();

    // Free entity CLUT data
    for (int i = 0; i < entity_cluts.size(); i++) {
        ClutEntry* clut_entry = &entity_cluts[i];
        free(clut_entry->clut_data);
    }

    // Delete all tile textures from all layers
    for (int i = 0; i < tile_layers.size(); i++) {

        // Clear out all foreground textures
        for (int k = 0; k < tile_layers[i].first.tiles.size(); k++) {
            glDeleteTextures(1, &tile_layers[i].first.tiles[k].texture);
        }

        // Clear out all background textures
        for (int k = 0; k < tile_layers[i].second.tiles.size(); k++) {
            glDeleteTextures(1, &tile_layers[i].second.tiles[k].texture);
        }
    }

    // Clear map ID
    map_id = "";

    // Reset function pointers
    update_entities_func = 0;
    process_entity_collision_func = 0;
    spawn_onscreen_entities_func = 0;
    init_room_entities_func = 0;
    pause_entities_func = 0;

    // Delete room data
    rooms.clear();

    // Clear out sprite banks
    for (uint i = 0; i < sprite_banks.size(); i++) {
        sprite_banks[i].clear();
    }
    sprite_banks.clear();

    // Delete entity CLUTs
    entity_cluts.clear();

    // Clear out entity layouts
    for (uint i = 0; i < entity_layouts.size(); i++) {
        entity_layouts[i].clear();
    }
    entity_layouts.clear();

    // Delete tile layers
    tile_layers.clear();

    // Clear out all entity graphics
    for (uint i = 0; i < entity_graphics.size(); i++) {
        entity_graphics[i].clear();
    }
    entity_graphics.clear();


    // Clear out map tile CLUTs
    memset(map_tile_cluts[0], 256 * 16 * 2, sizeof(byte));


    // Delete everything else
    map_textures.clear();
    map_tilesets.clear();
    entity_functions.clear();

    // Reset the framebuffer target to the main window
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Delete the framebuffer
    if (loaded) {
        glDeleteFramebuffers(1, &fbo);
    }
}