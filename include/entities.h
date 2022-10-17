#ifndef SOTN_EDITOR_ENTITIES
#define SOTN_EDITOR_ENTITIES

#include <stdbool.h>
#include <string>
#include <vector>
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

// Entity data
typedef struct EntityData {
    short pos_x_accel = 0;              // Determines X position by adding acceleration
    short pos_x = 0;
    short pos_y_accel = 0;              // Determines Y position by adding acceleration
    short pos_y = 0;
    int acceleration_x = 0;
    int acceleration_y = 0;
    short offset_x = 0;
    short offset_y = 0;
    ushort facing = 0;
    ushort clut_index = 0;

    char blend_mode = 0;                // . . . . . . . 1     ???
                                        // . . . . . . 1 .     ???
                                        // . . . . . 1 . .     ???
                                        // . . . . 1 . . .     ???
                                        // . . . 1 . . . .     Semi-Transparent
                                        // . . 1 . . . . .     Add
                                        // . 1 . . . . . .     Divide
                                        // 1 . . . . . . .     ???

    char color_flags = 0;               // . . . . . . . 1     ???
                                        // . . . . . . 1 .     ???
                                        // . . . . . 1 . .     ???
                                        // . . . . 1 . . .     Color Fill
                                        // . . . 1 . . . .     Red Overlay
                                        // . . 1 . . . . .     Green Overlay
                                        // . 1 . . . . . .     Blue Overlay
                                        // 1 . . . . . . .     Flicker?

    // These are used when a wall is coming down, etc.
    short fx_width = 0;
    short fx_height = 0;

    // General purpose progress measurement
    // Examples:
    //     The platform you stand on to lower the wall in NO3:R10
    //     How long red Merman recoil for after a fireball attack
    short fx_timer = 0;

    // X target coordinate of Alucard's outline for spells / MP recovery
    short fx_coord_x = 0;

    // Y target coordinate of Alucard's outline for spells / MP recovery
    short fx_coord_y = 0;
    short z_depth = 0;
    ushort object_id = 0;
    uint update_function = 0;
    ushort current_state = 0;
    short current_substate = 0;

    // This is also the item ID for items, relic ID for relics, and pickup type for pickups
    ushort initial_state = 0;

    // The slot in the room (incremental) being occupied
    ushort room_slot = 0;

    // Lowest byte goes down from xF to x0 when the enemy is hit, also 0x00800000 for relics
    int unk34 = 0;
    short unk38 = 0;

    // Index into the enemy data array (enemy_data_addr + info_idx * 0x28)
    short info_idx = 0;

    // ??? (Hitbox auto-toggle?)
    short unk3C = 0;

    // Entity's HP
    short hit_points = 0;

    // Damage dealt by the enemy
    short attack_damage = 0;

    // Determines the type/element of damage being done
    short damage_type = 0;

    // ???
    ushort unk44 = 0;
    byte hitbox_width = 0;
    byte hitbox_height = 0;
    // Affected when being hit
    byte unk48 = 0;
    byte unk49 = 0;
    short unk4A = 0;
    int unk4C = 0;
    // Sprite frame 0x00 = Repeat animatino
    // Sprite frame 0xFF = End animation
    ushort frame_index = 0;
    ushort frame_duration = 0;
    ushort sprite_bank = 0;
    ushort sprite_image = 0;
    short unk58 = 0;
    ushort tileset = 0;
    int unk5C = 0;
    int unk60 = 0;
    int polygt4_id = 0;
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
    // Definitely POLY_GT4
    uint unk7C = 0;

    // Current attack state timer? (Higher values freezes the entity during animation until it hits zero)
    short unk80 = 0;

    // Current attack substate timer? (Repeats the current state until zero is hit)
    short unk82 = 0;

    // Reset X coordinate for entity? (navigates to this location after attacking)
    short unk84 = 0;

    // Action cooldown timer? (time taken until entity can attack again)
    short unk86 = 0;

    // Death flames - X coord
    ushort unk88 = 0;

    // Death flames - Y coord
    // Mod X?
    ushort unk8A = 0;

    ushort unk8C = 0;

    // Mod Y?
    ushort unk8E = 0;

    // Relic sine wave duration
    int unk90 = 0;

    // Relic sine wave offset
    int unk94 = 0;

    int unk98 = 0;
    int unk9C = 0;
    int unkA0 = 0;
    int unkA4 = 0;
    int unkA8 = 0;
    int unkAC = 0;
    int unkB0 = 0;

    // Probably pickup ID, used to set the flag in the pickup flag array
    int pickup_flag = 0;

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
    GLuint texture;
    bool flip_x = false;
    bool flip_y = false;
    bool blend = false;
    uint blend_mode = 0;
    bool override_coords = false;       // Used for POLY_GT4 primitives
    int ot_layer = -1;                  // Entity's entry in the OT
    int x = 0;                          // Absolute X coord in the room
    int y = 0;                          // Absolute Y coord in the room

    // Polygon-specific data for POLY_GT4 structs
    PolygonCoords top_left;
    PolygonCoords top_right;
    PolygonCoords bottom_left;
    PolygonCoords bottom_right;
    bool skew = false;
    POLY_GT4 polygon;
} EntitySpritePart;

// Address of allocatable entities in RAM (first 64 entity slots are reserved
const uint ENTITY_ALLOCATION_START = ENTITY_LIST_START + (sizeof(EntityData) * 0x40);



// Class for compression utilities
class Entity {
	
    public:

        // Entity data
    	EntityData data;

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

        // Address of the sprite
        uint sprite_address = 0;

        // Check if entity has a POLY_GT4 sprite and is in the background
        bool is_bg_entity = false;



	private:

};

#endif //SOTN_EDITOR_ENTITIES