#ifndef SOTN_EDITOR_COMMON
#define SOTN_EDITOR_COMMON
#include <stdint.h>
// These are just common things for the sake of formatting and clarity across source files

typedef unsigned long       ulong;
typedef unsigned int        uint;
typedef unsigned short      ushort;
typedef unsigned char       byte;

// Mostly used for GTE
typedef uint64_t uint64;
typedef int64_t int64;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint8_t uint8;
typedef int8_t int8;

// DRA.BIN address offsets
const uint WEAPON_NAMES_DESC_ADDR = 0x000A4B04;
const uint EQUIP_NAMES_DESC_ADDR = 0x000A7718;
const uint RELIC_TABLE_ADDR = 0x000A8720;
const uint ITEM_NAMES_DESC_ADDR = 0x000DD18C;
const uint SPELLS_DATA_ADDR = 0x000DFF50;
const uint ENEMY_NAMES_ADDR = 0x000E05D8;
const uint ENEMY_DATA_ADDR = 0x000A8900;

// Graphical data addresses
const uint GENERIC_SPRITE_BANKS_ADDR = 0x000A3B70;
const uint GENERIC_CLUTS_ADDR = 0x000D6914;
const uint COMPRESSED_GENERIC_POWERUP_TILESET_ADDR = 0x000C217C;
const uint COMPRESSED_GENERIC_SAVEROOM_TILESET_ADDR = 0x000C3560;
const uint COMPRESSED_GENERIC_LOADROOM_TILESET_ADDR = 0x000C27B0;
const uint ITEM_SPRITES_ADDR = 0x000C5324;
const uint ITEM_CLUTS_ADDR = 0x000D88D4;
const uint RELIC_CLUTS_ADDR = 0x000D68D0;

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


// Polygon array offset
const uint POLYGT4_LIST_ADDR = 0x00086FEC;


// Ordering table offset
const uint OT_OFFSET = 0x00054770;

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

// RECT struct for image operations
typedef struct RECT {
    short x = 0;
    short y = 0;
    short w = 0;
    short h = 0;
} RECT;

#endif //SOTN_EDITOR_COMMON