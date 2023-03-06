#ifndef SOTN_EDITOR_ENTITIES
#define SOTN_EDITOR_ENTITIES

#include <string>
#include <vector>
#include <map>
#include <variant>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"



// Address of entities in RAM
const uint ENTITY_LIST_START = 0x000733D8;

// Sprite blend modes
const uint BLEND_MODE_NORMAL = 0;
const uint BLEND_MODE_LIGHTEN = 0x20;
const uint BLEND_MODE_UNK70 = 0x70;

// Entity transform flags
enum ENTITY_TRANSFORM_FLAGS {
    EntityTransform_X = 1 << 0,
    EntityTransform_Y = 1 << 1,
    EntityTransform_Rotate = 1 << 2,
    EntityTransform_Fill = 1 << 3,
    EntityTransform_Red = 1 << 4,
    EntityTransform_Green = 1 << 5,
    EntityTransform_Blue = 1 << 6,
    EntityTransform_Flicker = 1 << 7
};

// Entity data
typedef struct EntityData {
    short pos_x_sub = 0;                // Horizontal subpixels
    short pos_x = 0;                    // DAI::801C3164 -> x
    short pos_y_sub = 0;                // Vertical subpixels
    short pos_y = 0;                    // DAI::801C3164 -> y
    int acceleration_x = 0;
    int acceleration_y = 0;
    short hitbox_offset_x = 0;
    short hitbox_offset_y = 0;
    ushort facing = 0;
    ushort clut_index = 0;              // RNO3::801C5850 -> color

    byte blend_mode = 0;                // . . . . . . . 1     ???
                                        // . . . . . . 1 .     ???
                                        // . . . . . 1 . .     ???
                                        // . . . . 1 . . .     ???
                                        // . . . 1 . . . .     Dark
                                        // . . 1 . . . . .     Light
                                        // . 1 . . . . . .     Reverse
                                        // 1 . . . . . . .     Pale

    byte transform_flags = 0;           // . . . . . . . 1     Enable X Transformations
                                        // . . . . . . 1 .     Enable Y Transformations
                                        // . . . . . 1 . .     Enable Rotation
                                        // . . . . 1 . . .     Color Fill
                                        // . . . 1 . . . .     Red Overlay
                                        // . . 1 . . . . .     Green Overlay
                                        // . 1 . . . . . .     Blue Overlay
                                        // 1 . . . . . . .     Flicker

    // These are used when a wall is coming down, etc.
    short scale_x = 0;
    short scale_y = 0;

    // Rotation amount (clockwise)
    short rotation = 0;

    // X target coordinate of Alucard's outline for spells / MP recovery
    short translate_x = 0;

    // Y target coordinate of Alucard's outline for spells / MP recovery
    short translate_y = 0;
    short z_depth = 0;                  // NO1::801B4CC4 -> pri
    ushort object_id = 0;
    uint update_function = 0;
    ushort current_state = 0;           // LIB::801BDAE4 -> step
    short current_substate = 0;         // LIB::801BDAE4 -> step_s

    // This is also the item ID for items, relic ID for relics, and pickup type for pickups
    ushort initial_state = 0;           // RDAI::801B1C34 -> set

    // The slot in the room (incremental) being occupied
    ushort room_slot = 0;

    // POLY_GT4 flags
    // unk34 & 0x100 checks for object destruction flag (if thing can be destroyed)
    //
    // 0x00020000 = ???
    // 0x00040000 = Move twice as fast?
    // 0x00800000 = Use polygon_id to free polygon when deloading
    // 0x08000000 = Update X/Y coords
    //
    int unk34 = 0;
    short unk38 = 0;

    // Index into the enemy data array (enemy_data_addr + info_idx * 0x28)
    short info_idx = 0;

    // ??? (Hitbox auto-toggle?)
    // Toggles whether statue in DAI can be destroyed or not
    // bit 0 = Checks if collision with player should be processed
    // bit 1 = Checks if entity can be damaged by player
    short hitbox_type = 0;

    // Entity's HP
    short hit_points = 0;

    // Damage dealt by the enemy
    short attack_damage = 0;

    // Determines the type/element of damage being done
    //    0000 = None
    //    0001, 02, 04, 08 = Normal damage
    //    0010 = x8 damage, tossed
    //    0020 = Normal damage (x16 when paired with 0x10)
    //    0040 = Bloody damage
    //    0080 = Poison damage
    //    0100 = Guard
    //    0200 = Stone
    //    0800 = Steam
    //    1000 = Holy damage
    //    2000 = Freeze damage
    //    4000 = Electric damage
    //    8000 = Fire damage
    short damage_type = 0;              // RBO8::80197B1C -> attack

    // ???
    ushort unk44 = 0;
    byte hitbox_width = 0;
    byte hitbox_height = 0;
    // Affected when being hit
    byte unk48 = 0;
    byte unk49 = 0;                     // Color when being hit
                                        // 00 = Black <-> Multicolor
                                        // 01 = Black <-> Red
                                        // 02 = Red <-> Orange
                                        // 03 = Black <-> Blue
                                        // 04 = Black <-> Yellow
                                        // 05 = Green <-> Yellow
                                        // 06 = Black <-> Green
                                        // 07 = Black <-> Grey
    short unk4A = 0;
    int unk4C = 0;
    // Sprite frame 0x00 = Repeat animation
    // Sprite frame 0xFF = End animation
    ushort frame_index = 0;             // DRA::80110DF8 -> pl_pose
    ushort frame_duration = 0;
    ushort sprite_bank = 0;
    ushort sprite_image = 0;            // DRA::801119C4 -> charal
    short unk58 = 0;
    ushort tileset = 0;
    int segment_root = 0;               // Root of a segmented entity
    int segment_next = 0;               // Next part of a segmented entity
    int polygon_id = 0;
    short unk68 = 0;
    short unk6A = 0;
    byte unk6C = 0;
    char unk6D = 0;
    // Affected when being hit
    short unk6E = 0;
    int unk70 = 0;
    int unk74 = 0;
    int unk78 = 0;

    // Affects Y-coordinate acceleration up and down for bats (sine wave?)
    // Affects animation frame used for Warg (0 vs. 1)
    // (potential temp var?)
    // Definitely SOTN_POLYGON
    uint unk7C = 0;

    // Current attack state timer? (Higher values freezes the entity during animation until it hits zero)
    short unk80 = 0;                    // DRA::80126ECC -> light_timer
                                        // NO2::801B5FB8 -> timer

    // Current attack substate timer? (Repeats the current state until zero is hit)
    short unk82 = 0;                    // ARE::801C3844 -> check / kosuu (quantity?)
                                        // LIB::801D2274 -> w

    // Reset X coordinate for entity? (navigates to this location after attacking)
    short unk84 = 0;

    /*
     * LIB::801BD268
     * unk84 = eff_step
     * unk85 = eff_timer
     */

    // Action cooldown timer? (time taken until entity can attack again)
    short unk86 = 0;

    // Death flames - X coord
    ushort unk88 = 0;                   // NO1::801BF3F4 -> timer
                                        // RNZ1::801A9994 -> damage

    // Death flames - Y coord
    // Mod X?
    // Used to determine scale of sprite (higher = smaller)
    ushort unk8A = 0;                   // LIB::801D2274 -> hz

    ushort unk8C = 0;                   // RNO4::801D5E90 -> angle

    // Mod Y?
    ushort unk8E = 0;

    // Relic sine wave duration
    int unk90 = 0;

    // Relic sine wave offset
    int unk94 = 0;                      // DRA::80119588 -> str_y
                                        // RNO3::801C35F8 -> rest_time

    int unk98 = 0;
    // Used to determine rotation around the X axis
    short unk9C = 0;                    // LIB::801D2274 -> x
    short unk9E = 0;                    // LIB::801D2274 -> y
    short unkA0 = 0;                    // LIB::801D2274 -> z
    short unkA2 = 0;
    int unkA4 = 0;
    int unkA8 = 0;
    int unkAC = 0;
    int unkB0 = 0;

    // Probably pickup ID, used to set the flag in the pickup flag array
    ushort pickup_flag = 0;
    ushort unkB6 = 0;

    // Possible callback? (Used as a function pointer to call functions)
    int callback = 0;
} EntityData;

// Entity initialization data
typedef struct EntityInitData {
    short x_coord = 0;
    short y_coord = 0;
    ushort entity_id = 0;
    ushort slot = 0;
    ushort initial_state = 0;
} EntityInitData;

// Entity graphic data
typedef struct EntityGraphicsData {
    ushort vram_y = 0;
    ushort vram_x = 0;
    ushort height = 0;
    ushort width = 0;
    uint compressed_graphics_addr = 0;
} EntityGraphicsData;

// Polygon coordinates
typedef struct PolygonCoords {
    int x = 0;
    int y = 0;
} PolygonCoords;

// Entity sprite data
typedef struct EntitySpritePart {
    uint flags = 0;
    short offset_x = 0;
    short offset_y = 0;
    ushort width = 0;
    ushort height = 0;
    GLuint texture = 0;
    bool flip_x = false;
    bool flip_y = false;
    bool blend = false;
    uint blend_mode = 0;

    // General properties
    bool override_coords = false;       // Used for SOTN_POLYGON primitives
    int ot_layer = -1;                  // Entity's entry in the OT
    int x = 0;                          // Absolute X coord in the room
    int y = 0;                          // Absolute Y coord in the room


    // Polygon-specific data for SOTN_POLYGON structs
    PolygonCoords top_left;
    PolygonCoords top_right;
    PolygonCoords bottom_left;
    PolygonCoords bottom_right;
    SOTN_POLYGON polygon;
    bool semi_transparent = false;      // Code & 1
    bool shade_texture = false;         // Code & 2

    // Sprite transformations
    bool skew = false;
    int rotate = 0;
    int anchor_x = 0;
    int anchor_y = 0;
} EntitySpritePart;

// Entity data for displaying and copying
typedef struct EntityDataEntry {
    int offset;
    std::string name;
    std::variant<char, byte, short, ushort, int, uint> value;
} EntityDataEntry;

// Address of allocatable entities in RAM (first 64 entity slots are reserved
const uint ENTITY_ALLOCATION_START = ENTITY_LIST_START + (sizeof(EntityData) * 0x40);



// Class for compression utilities
class Entity {
	
    public:

        // Entity data
    	EntityData data;

        // Entity data (as a map)
        std::map<std::string, std::variant<char, byte, short, ushort, int, uint>> data_map;
        std::vector<EntityDataEntry> data_vec;

        // Entity ID
        uint id = 0;

        // Entity slot (which entity slot should be occupied)
        uint slot = 0;

        // Entity name (if one exists)
        std::string name;

        // Entity description (if one exists)
        std::string desc;

        // Entity texture parts
        std::vector<EntitySpritePart> sprites;

        // Complete entity texture
        GLuint texture = 0;
        uint texture_width = 0;
        uint texture_height = 0;

        // Address of the entity in RAM
        uint address;

        // Address of the sprite in RAM
        uint sprite_address = 0;

        // Check if entity has a SOTN_POLYGON sprite and is in the background
        bool is_bg_entity = false;

        // Keep track of polygon data
        uint num_polygons = 0;
        std::vector<SOTN_POLYGON> polygons;

        // Functions
        void PopulateDataMap();



	private:

};

#endif //SOTN_EDITOR_ENTITIES