#ifndef SOTN_EDITOR_COMMON
#define SOTN_EDITOR_COMMON
#include <stdint.h>
#include <map>
// These are just common things for the sake of formatting and clarity across source files

typedef unsigned long       ulong;
typedef unsigned int        uint;
typedef unsigned short      ushort;
typedef unsigned char       byte;

// Mostly used for GTE
typedef int64_t int64;
typedef uint8_t uint8;

// DRA.BIN address offsets
const uint WEAPON_NAMES_DESC_ADDR = 0x000A4B04;
const uint EQUIP_NAMES_DESC_ADDR = 0x000A7718;
const uint RELIC_TABLE_ADDR = 0x000A8720;
const uint ITEM_NAMES_DESC_ADDR = 0x000DD18C;
const uint SPELLS_DATA_ADDR = 0x000DFF50;
const uint ENEMY_NAMES_ADDR = 0x000E05D8;
const uint ENEMY_DATA_ADDR = 0x000A8900;
const uint MAIN_FUNC_ADDR = 0x000E3988;                 // Main function in DRA.BIN
const uint MAIN_FUNC_LOOP_ADDR = 0x000E4100;            // do...while(true) loop in main function

// Alucard entity address
const uint ALUCARD_ENTITY_ADDR = 0x000733D8;
const uint WEAPON_ATTACK_ENTITY_ADDR = 0x00073F98;

// Graphical data addresses
const uint GENERIC_SPRITE_BANKS_ADDR = 0x000A3B70;
const uint GENERIC_CLUTS_ADDR = 0x000D6914;
const uint COMPRESSED_GENERIC_POWERUP_TILESET_ADDR = 0x000C217C;
const uint COMPRESSED_GENERIC_SAVEROOM_TILESET_ADDR = 0x000C3560;
const uint COMPRESSED_GENERIC_LOADROOM_TILESET_ADDR = 0x000C27B0;
const uint ITEM_SPRITES_ADDR = 0x000C5324;
// Item CLUTs are also used for relics
const uint ITEM_CLUTS_ADDR = 0x000D88D4;

// Entity ID type values
const uint TYPE_RELIC = 0x000B;
const uint TYPE_PICKUP = 0x000C;

// Pickup flag offset (keeps track of which pickups/items were obtained)
const uint PICKUP_FLAG_ADDR = 0x0003BEEC;

// Base address of CLUT data for VRAM
const uint CLUT_BASE_ADDR = 0x0006CBCC;

// Address of CLUT indices (VRAM address >> 5)
const uint CLUT_INDEX_ADDR = 0x0003C104;

// Address of destination pointer table in SotN binary (DRA.BIN)
const uint SOTN_PTR_TBL_ADDR = 0x0003C774;

// Palette number for candles and pickups
const uint CANDLE_CLUT = 144;
const uint LIFE_MAX_UP_CLUT = 128;
const uint HEART_MAX_UP_CLUT = 145;

// States for Life Max Up and Heart Max Up
const uint LIFE_MAX_UP_ID = 0x8017;
const uint HEART_MAX_UP_ID = 0x800C;


// Binary offsets
const uint PSX_RAM_OFFSET = 0x00010000 - 0x800;
const uint SOTN_RAM_OFFSET = 0x000A0000;
const uint MAP_RAM_OFFSET = 0x00180000;
const uint PSX_BIN_OFFSET = 0x80010000 - 0x800;
const uint SOTN_BIN_OFFSET = 0x800A0000;
const uint MAP_BIN_OFFSET = 0x80180000;
const uint RAM_BASE_OFFSET = 0x80000000;
const uint RAM_MAX_OFFSET = 0x80200000;


// Polygon array offset
const uint POLYGT4_LIST_ADDR = 0x00086FEC;


// Ordering table offset
const uint OT_OFFSET = 0x00054770;

// Room data
const uint ROOM_TILE_INDICES_ADDR = 0x00073084;
const uint ROOM_TILE_DATA_ADDR = 0x00073088;

// Addresses holding the current room's dimensions
const uint ROOM_WIDTH_ADDR = 0x000730A4;
const uint ROOM_HEIGHT_ADDR = 0x000730A8;

// Addresses holding the current screen offset
const uint SCREEN_X_OFFSET = 0x0007308E;
const uint SCREEN_Y_OFFSET = 0x00073092;

// Addresses holding the current room's coordinates
const uint ROOM_X_COORD_START_ADDR = 0x000730B0;
const uint ROOM_Y_COORD_START_ADDR = 0x000730B4;
const uint ROOM_X_COORD_END_ADDR = 0x000730B8;
const uint ROOM_Y_COORD_END_ADDR = 0x000730BC;

// Index of tile data in the OT
const uint OT_FG_TILE_LAYER = 0x60;
const uint OT_BG_TILE_LAYER = 0x20;

// POLY_GT4 struct for polygonal mapping stuff
typedef struct POLY_GT4 {
    uint tag = 0;
    byte r0 = 0;
    byte g0 = 0;
    byte b0 = 0;
    byte code = -1;
    // Top-left
    short x0 = 0;
    short y0 = 0;
    byte u0 = 0;
    byte v0 = 0;
    ushort clut = 0;
    byte r1 = 0;
    byte g1 = 0;
    byte b1 = 0;
    byte p1 = 0;
    // Top-right
    short x1 = 0;
    short y1 = 0;
    byte u1 = 0;
    byte v1 = 0;
    ushort tpage = 0;
    byte r2 = 0;
    byte g2 = 0;
    byte b2 = 0;
    byte p2 = 0;
    // Bottom-left
    short x2 = 0;
    short y2 = 0;
    byte u2 = 0;
    byte v2 = 0;
    ushort pad2 = 0;
    byte r3 = 0;
    byte g3 = 0;
    byte b3 = 0;
    byte p3 = 0;
    // Bottom-right
    short x3 = 0;
    short y3 = 0;
    byte u3 = 0;
    byte v3 = 0;
    ushort pad3 = 0;
} POLY_GT4;





// Polygon struct for polygonal mapping stuff
typedef struct SOTN_POLYGON {
    constexpr SOTN_POLYGON() {};
    uint tag = 0;
    byte r0 = 0;
    byte g0 = 0;
    byte b0 = 0;
    byte code = -1;
    // Top-left
    short x0 = 0;
    short y0 = 0;
    union {
        byte u0, tile_width = 0;
    };
    union {
        byte v0, tile_height = 0;
    };
    ushort clut = 0;
    union {
        uint drenv_addr = 0;
        struct {
            byte r1;
            byte g1;
            byte b1;
            byte p1;
        };
    };
    /*
    byte r1 = 0;
    byte g1 = 0;
    byte b1 = 0;
    byte p1 = 0;
     */
    // Top-right
    short x1 = 0;
    short y1 = 0;
    union {
        byte u1, sprt_width = 0;
    };
    union {
        byte v1, sprt_height = 0;
    };
    ushort tpage = 0;
    byte r2 = 0;
    byte g2 = 0;
    byte b2 = 0;
    byte p2 = 0;
    // Bottom-left
    short x2 = 0;
    short y2 = 0;
    byte u2 = 0;
    byte v2 = 0;
    union {
        ushort pad2, ot_layer = 0;
    };
    byte r3 = 0;
    byte g3 = 0;
    byte b3 = 0;
    byte p3 = 0;
    // Bottom-right
    short x3 = 0;
    short y3 = 0;
    byte u3 = 0;
    byte v3 = 0;
    union {
        ushort pad3, sprt_tpage = 0;
    };
} SOTN_POLYGON;

// RECT struct for image operations
typedef struct RECT {
    short x = 0;
    short y = 0;
    short w = 0;
    short h = 0;
} RECT;

// DR_ENV struct for polygon stuff
typedef struct DR_ENV {
    uint tag = 0;
    uint code[15] = {0};
} DR_ENV;

// DRAWENV struct for polygon stuff too as well also
typedef struct DRAWENV {
    RECT clip = {0};
    short ofs[2] = {0};
    RECT tw = {0};
    ushort tpage = 0;
    byte dtd = 0;
    byte dfe = 0;
    byte isbg = 0;
    byte r0 = 0;
    byte g0 = 0;
    byte b0 = 0;
} DRAWENV;

// Texture page struct for tpage values, e.g. in getTPage()
typedef struct TexturePage {
    uint tex_page_x = 0;                // . . . . . . . . . . . . X X X X          X = n * 64
    uint tex_page_y = 0;                // . . . . . . . . . . . X . . . .          Y = n * 256
    uint blend_rate = 0;                // . . . . . . . . . X X . . . . .
                                        // 0: (0.5 * Back) + (0.5 * Forward)
                                        // 1: Back + Forward
                                        // 2: Back - Forward
                                        // 3: Back + (0.25 * Forward)
    uint tex_mode = 0;                  // . . . . . . . X X . . . . . . .
                                        // 0: 4-bit CLUT
                                        // 1: 8-bit CLUT
                                        // 2: 16-bit Direct
    bool dither = false;                // . . . . . . X . . . . . . . . .          Dither flag
    bool draw_display = false;          // . . . . . X . . . . . . . . . .          Flag for drawing to the displayed area
    bool tex_disable = false;           // . . . . X . . . . . . . . . . .          Texture disable flag
    bool x_flip = false;                // . . . X . . . . . . . . . . . .          Horizontal flip flag
    bool y_flip = false;                // . . X . . . . . . . . . . . . .          Vertical flip flag
} TexturePage;



// Polygon flags
enum POLYGON_FLAGS {
    PolygonFlag_Unk0            = 1 << 0,
    PolygonFlag_Unk1            = 1 << 1,
    PolygonFlag_Unk2            = 1 << 2,
    PolygonFlag_Unk3            = 1 << 3,
    PolygonFlag_Unk4            = 1 << 4,
    PolygonFlag_Unk5            = 1 << 5,
    PolygonFlag_Unk6            = 1 << 6,
    PolygonFlag_Unk7            = 1 << 7,
    PolygonFlag_Unk8            = 1 << 8,
    PolygonFlag_Unk9            = 1 << 9,
    PolygonFlag_Unk10           = 1 << 10,
    PolygonFlag_Unk11           = 1 << 11,
    PolygonFlag_Unk12           = 1 << 12,
    PolygonFlag_Unk13           = 1 << 13,
    PolygonFlag_Unk14           = 1 << 14,
    PolygonFlag_Unk15           = 1 << 15,
    PolygonFlag_Unk16           = 1 << 16,
    PolygonFlag_Unk17           = 1 << 17,
    PolygonFlag_Unk18           = 1 << 18,
    PolygonFlag_Unk19           = 1 << 19,
    PolygonFlag_Unk20           = 1 << 20,
    PolygonFlag_Unk21           = 1 << 21,
    PolygonFlag_Unk22           = 1 << 22,
    PolygonFlag_Unk23           = 1 << 23,
    PolygonFlag_Unk24           = 1 << 24,
    PolygonFlag_Unk25           = 1 << 25,
    PolygonFlag_Unk26           = 1 << 26,
    PolygonFlag_MoveIn3dSpace   = 1 << 27,
    PolygonFlag_PlayAnimation   = 1 << 28,
    PolygonFlag_PauseAnimation  = 1 << 29,
    PolygonFlag_Unk30           = 1 << 30,
    PolygonFlag_DestroySelf     = 1 << 31
};

// PSX GPU primitive types
const uint PRIM_TYPE_POLYGT3 = 0x34;
const uint PRIM_TYPE_POLYG4 = 0x38;
const uint PRIM_TYPE_POLYGT4 = 0x3C;
const uint PRIM_TYPE_LINEG2 = 0x50;
const uint PRIM_TYPE_TILE = 0x60;
const uint PRIM_TYPE_SPRT = 0x64;
const uint PRIM_TYPE_DRENV = 0x69;

// Primitive types as identified by the SotN polygon's code property
const std::map<byte, byte> SOTN_PRIM_TYPES = {
    {   1, PRIM_TYPE_TILE},
    {0x11, PRIM_TYPE_TILE},
    {   2, PRIM_TYPE_LINEG2},
    {0x12, PRIM_TYPE_LINEG2},
    {   3, PRIM_TYPE_POLYG4},
    {0x13, PRIM_TYPE_POLYG4},
    {   4, PRIM_TYPE_POLYGT4},
    {   5, PRIM_TYPE_POLYGT3},
    {   6, PRIM_TYPE_SPRT},
    {0x16, PRIM_TYPE_SPRT},
    {   7, PRIM_TYPE_DRENV}
};

// Collision types
enum COLLISION_TYPES {

    // General tiles
    CollisionType_Air = 0x00,
    CollisionType_Slope_End = 0x01,
    CollisionType_Solid = 0x03,

    // 1-tile wide slopes
    CollisionType_Slope_Floor_Right_45 = 0x80,
    CollisionType_Slope_Floor_Right_45_X,
    CollisionType_Slope_Floor_Left_45_X,
    CollisionType_Slope_Floor_Left_45,
    CollisionType_Slope_Ceiling_Right_45,
    CollisionType_Slope_Ceiling_Right_45_X,
    CollisionType_Slope_Ceiling_Left_45_X,
    CollisionType_Slope_Ceiling_Left_45,

    // 2-tile wide slopes
    CollisionType_Slope_Floor_Right_Half_Low,
    CollisionType_Slope_Floor_Right_Half_High,
    CollisionType_Slope_Floor_Right_Half_X,
    CollisionType_Slope_Floor_Left_Half_X,
    CollisionType_Slope_Floor_Left_Half_High,
    CollisionType_Slope_Floor_Left_Half_Low,
    CollisionType_Slope_Ceiling_Right_Half_Low,
    CollisionType_Slope_Ceiling_Right_Half_High,
    CollisionType_Slope_Ceiling_Right_Half_X,
    CollisionType_Slope_Ceiling_Left_Half_X,
    CollisionType_Slope_Ceiling_Left_Half_High,
    CollisionType_Slope_Ceiling_Left_Half_Low,

    // 4-tile wide slopes
    CollisionType_Slope_Floor_Right_Quarter_Lowest,
    CollisionType_Slope_Floor_Right_Quarter_Lower,
    CollisionType_Slope_Floor_Right_Quarter_Higher,
    CollisionType_Slope_Floor_Right_Quarter_Highest,
    CollisionType_Slope_Floor_Right_Quarter_X,
    CollisionType_Slope_Floor_Left_Quarter_X,
    CollisionType_Slope_Floor_Left_Quarter_Highest,
    CollisionType_Slope_Floor_Left_Quarter_Higher,
    CollisionType_Slope_Floor_Left_Quarter_Lower,
    CollisionType_Slope_Floor_Left_Quarter_Lowest,
    CollisionType_Slope_Ceiling_Right_Quarter_Lowest,
    CollisionType_Slope_Ceiling_Right_Quarter_Lower,
    CollisionType_Slope_Ceiling_Right_Quarter_Higher,
    CollisionType_Slope_Ceiling_Right_Quarter_Highest,
    CollisionType_Slope_Ceiling_Right_Quarter_X,
    CollisionType_Slope_Ceiling_Left_Quarter_X,
    CollisionType_Slope_Ceiling_Left_Quarter_Highest,
    CollisionType_Slope_Ceiling_Left_Quarter_Higher,
    CollisionType_Slope_Ceiling_Left_Quarter_Lower,
    CollisionType_Slope_Ceiling_Left_Quarter_Lowest,

    // Other tiles
    CollisionType_Half_Upper_StopJump = 0xE5,
    CollisionType_Half_Lower_StopJump = 0xE6,
    CollisionType_Half_Upper_FallThrough = 0xE7,
    CollisionType_Half_Lower_FallThrough = 0xE8,
    CollisionType_UNUSED_Half_Upper_SlowFall = 0xE9,
    CollisionType_Water_Half_Upper = 0xEA,
    CollisionType_DAI_Breakable_Statue = 0xEB,
    CollisionType_Water = 0xED,
    CollisionType_Spikes = 0xF4,
    CollisionType_Mist_Bars = 0xF8,
    CollisionType_Water_Half_Lower = 0xF9,
    CollisionType_UNUSED_SlowFall = 0xFA,
    CollisionType_UNUSED_SlowerFall = 0xFB,
    CollisionType_Half_Upper_Slope = 0xFC,
    CollisionType_Half_Lower_Slope = 0xFD,
    CollisionType_Half_Upper_Solid = 0xFE,
    CollisionType_Half_Lower_Solid = 0xFF
};

// Blend modes
enum BLEND_MODES {
    BlendMode_Dark = 0x10,
    BlendMode_Light = 0x20,
    BlendMode_Reverse = 0x40,
    BlendMode_Pale = 0x60
};


#endif //SOTN_EDITOR_COMMON