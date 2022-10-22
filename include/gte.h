#ifndef SOTN_EDITOR_GTE
#define SOTN_EDITOR_GTE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "common.h"

// GTE function type declarations
typedef void (*gte_fn)(uint, uint, uint, uint, uint);

// RGBC data
typedef struct gte_rgbc {
    byte r = 0;
    byte g = 0;
    byte b = 0;
    byte code = 0;
} gte_rgbc;

// Screen coordinates
typedef struct gte_sxy {
    short x = 0;
    short y = 0;
} gte_sxy;

// General use 3-int vector
typedef struct gte_vec3i {
    union { int x, r; };
    union { int y, g; };
    union { int z, b; };
} gte_vec3i;

// General use 3-short vector
typedef struct gte_vec3s {
    union { short x, r; };
    union { short y, g; };
    union { short z, b; };
} gte_vec3s;

// General use 4-byte vector
typedef struct gte_vec4b {
    union { byte x, r; };
    union { byte y, g; };
    union { byte z, b; };
    union { byte w, cd; };
} gte_vec4b;

// Divisor table for GTE division
static const byte DIV_TABLE[] = {
    0xFF, 0xFD, 0xFB, 0xF9, 0xF7, 0xF5, 0xF3, 0xF1, 0xEF, 0xEE, 0xEC, 0xEA, 0xE8, 0xE6, 0xE4, 0xE3,
    0xE1, 0xDF, 0xDD, 0xDC, 0xDA, 0xD8, 0xD6, 0xD5, 0xD3, 0xD1, 0xD0, 0xCE, 0xCD, 0xCB, 0xC9, 0xC8,
    0xC6, 0xC5, 0xC3, 0xC1, 0xC0, 0xBE, 0xBD, 0xBB, 0xBA, 0xB8, 0xB7, 0xB5, 0xB4, 0xB2, 0xB1, 0xB0,
    0xAE, 0xAD, 0xAB, 0xAA, 0xA9, 0xA7, 0xA6, 0xA4, 0xA3, 0xA2, 0xA0, 0x9F, 0x9E, 0x9C, 0x9B, 0x9A,
    0x99, 0x97, 0x96, 0x95, 0x94, 0x92, 0x91, 0x90, 0x8F, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x87, 0x86,
    0x85, 0x84, 0x83, 0x82, 0x81, 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x75, 0x74,
    0x73, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64,
    0x63, 0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D, 0x5D, 0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57, 0x56, 0x55,
    0x54, 0x53, 0x53, 0x52, 0x51, 0x50, 0x4F, 0x4E, 0x4D, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 0x48, 0x48,
    0x47, 0x46, 0x45, 0x44, 0x43, 0x43, 0x42, 0x41, 0x40, 0x3F, 0x3F, 0x3E, 0x3D, 0x3C, 0x3C, 0x3B,
    0x3A, 0x39, 0x39, 0x38, 0x37, 0x36, 0x36, 0x35, 0x34, 0x33, 0x33, 0x32, 0x31, 0x31, 0x30, 0x2F,
    0x2E, 0x2E, 0x2D, 0x2C, 0x2C, 0x2B, 0x2A, 0x2A, 0x29, 0x28, 0x28, 0x27, 0x26, 0x26, 0x25, 0x24,
    0x24, 0x23, 0x22, 0x22, 0x21, 0x20, 0x20, 0x1F, 0x1E, 0x1E, 0x1D, 0x1D, 0x1C, 0x1B, 0x1B, 0x1A,
    0x19, 0x19, 0x18, 0x18, 0x17, 0x16, 0x16, 0x15, 0x15, 0x14, 0x14, 0x13, 0x12, 0x12, 0x11, 0x11,
    0x10, 0x0F, 0x0F, 0x0E, 0x0E, 0x0D, 0x0D, 0x0C, 0x0C, 0x0B, 0x0A, 0x0A, 0x09, 0x09, 0x08, 0x08,
    0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00,
    0x00
};

class GteEmulator {

    public:

        // Whether to print debug information or not
        static bool debug;


        // Data registers
        static short VX[3][3];
        static gte_vec4b RGBC;
        static ushort OTZ;
        static short IR[4];
        static gte_sxy SXY_FIFO[4];
        static ushort SZ_FIFO[4];
        static gte_vec4b RGBC_FIFO[3];
        static byte RES1[4];
        static int MAC[4];
        static ushort IRGB;
        static ushort ORGB;
        static int LZCS;
        static int LZCR;


        // Control registers
        static short ROT_MTX[3][3];
        static gte_vec3i TRANS_VEC;
        static short LIGHT_MTX[3][3];
        static gte_vec3i BG_COLOR_VEC;
        static short LIGHT_COLOR_MTX[3][3];
        static gte_vec3i FAR_COLOR_VEC;
        static int OFX;
        static int OFY;
        static ushort H;
        static short DQA;
        static int DQB;
        static short ZSF3;
        static short ZSF4;
        static uint FLAG;


        // GTE emulator functions
        static void Initialize();
        static void ProcessOpcode(uint opcode);
        static uint ReadDataRegister(uint num);
        static void WriteDataRegister(uint num, uint value);
        static uint ReadControlRegister(uint num);
        static void WriteControlRegister(uint num, uint value);

        // GTE helper functions
        static uint CountLeadingZeros(uint value, uint num_bits);


        // GTE commands
        static void gte_nop(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_RTPS(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCLIP(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_OP(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_DPCS(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_INTPL(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_MVMVA(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCDS(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_CDP(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCDT(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCCS(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_CC(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCS(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCT(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_SQR(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_DCPL(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_DPCT(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_AVSZ3(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_AVSZ4(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_RTPT(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_GPF(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_GPL(uint lm, uint tv, uint mv, uint mm, uint sf);
        static void gte_NCCT(uint lm, uint tv, uint mv, uint mm, uint sf);

        // GTE command list
        const static constexpr gte_fn gte_funcs[64] = {
            gte_nop,
            gte_RTPS,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_NCLIP,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_OP,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_DPCS,
            gte_INTPL,
            gte_MVMVA,
            gte_NCDS,
            gte_CDP,
            gte_nop,
            gte_NCDT,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_NCCS,
            gte_CC,
            gte_nop,
            gte_NCS,
            gte_nop,
            gte_NCT,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_SQR,
            gte_DCPL,
            gte_DPCT,
            gte_nop,
            gte_nop,
            gte_AVSZ3,
            gte_AVSZ4,
            gte_nop,
            gte_RTPT,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_nop,
            gte_GPF,
            gte_GPL,
            gte_NCCT
        };



    private:

};

#endif //SOTN_EDITOR_CLION_GTE_H
