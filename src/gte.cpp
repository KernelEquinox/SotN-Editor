#include <algorithm>
#include "gte.h"



// Variables
uint GteEmulator::registers[64];
bool GteEmulator::debug;

// Data registers
short GteEmulator::VX[3][3];
gte_vec4b GteEmulator::RGBC;
ushort GteEmulator::OTZ;
short GteEmulator::IR[4];
gte_sxy GteEmulator::SXY_FIFO[4];
ushort GteEmulator::SZ_FIFO[4];
gte_vec4b GteEmulator::RGBC_FIFO[3];
byte GteEmulator::RES1[4];
int GteEmulator::MAC[4];
ushort GteEmulator::IRGB;
ushort GteEmulator::ORGB;
int GteEmulator::LZCS;
int GteEmulator::LZCR;


// Control registers
short GteEmulator::ROT_MTX[3][3];
gte_vec3i GteEmulator::TRANS_VEC;
short GteEmulator::LIGHT_MTX[3][3];
gte_vec3i GteEmulator::BG_COLOR_VEC;
short GteEmulator::LIGHT_COLOR_MTX[3][3];
gte_vec3i GteEmulator::FAR_COLOR_VEC;
int GteEmulator::OFX;
int GteEmulator::OFY;
ushort GteEmulator::H;
short GteEmulator::DQA;
int GteEmulator::DQB;
short GteEmulator::ZSF3;
short GteEmulator::ZSF4;
uint GteEmulator::FLAG;



// Initializes the GTE emulator
void GteEmulator::Initialize() {

    // Clear out the registers
    for (uint i = 0; i < 64; i++)
    {
        registers[i] = 0;
    }

    for (int y = 0; y < 3; y++) {
        RGBC_FIFO[y].r = 0;
        RGBC_FIFO[y].g = 0;
        RGBC_FIFO[y].b = 0;
        RGBC_FIFO[y].cd = 0;
        TRANS_VEC.x = TRANS_VEC.y = TRANS_VEC.z = 0;
        BG_COLOR_VEC.r = BG_COLOR_VEC.g = BG_COLOR_VEC.b = 0;
        FAR_COLOR_VEC.r = FAR_COLOR_VEC.g = FAR_COLOR_VEC.b = 0;
        for (int x = 0; x < 3; x++) {
            VX[y][x] = 0;
            ROT_MTX[y][x] = 0;
            LIGHT_MTX[y][x] = 0;
            LIGHT_COLOR_MTX[y][x] = 0;
        }
    }

    for (int i = 0; i < 4; i++) {
        IR[i] = 0;
        SXY_FIFO[i].x = 0;
        SXY_FIFO[i].y = 0;
        SZ_FIFO[i] = 0;
        MAC[i] = 0;
        RES1[i] = 0;    // verboden
    }

    RGBC.r = 0;
    RGBC.g = 0;
    RGBC.b = 0;
    RGBC.cd = 0;
    OTZ = 0;
    IRGB = 0;
    ORGB = 0;
    LZCS = 0;
    LZCR = 0;

    OFX = 0;
    OFY = 0;
    H = 0;
    DQA = 0;
    DQB = 0;
    ZSF3 = 0;
    ZSF4 = 0;
    FLAG = 0;
}



// Processes a GTE opcode
void GteEmulator::ProcessOpcode(uint opcode) {

    // GTE command
    uint command = opcode & 0x3F;

    // Bits 6-9 are ignored

    // Saturate IR[123] result (A1/2/3 limiter)
    uint lm = (opcode >> 10) & 1;

    // Bits 11-12 are ignored

    // MVMVA Translation Vector
    uint tv = (opcode >> 13) & 3;

    // MVMVA Multiply Vector
    uint mv = (opcode >> 15) & 3;

    // MVMVA Multiply Matrix
    uint mm = (opcode >> 17 & 3);

    // Scaling Format (for IR registers)
    //     0 = No fraction
    //     1 = 12-bit fraction
    uint sf = ((opcode >> 19) & 1) * 12;

    // Bits 20-24 are ignored
    // Bits 25-31 are ignored



    // Reset the FLAG register
    FLAG = 0;



    // Execute the opcode
    gte_funcs[command](lm, tv, mv, mm, sf);
}

uint GteEmulator::ReadDataRegister(uint num) {

    // Check which data register to read from
    switch (num) {
        case 0:
            return ((ushort)VX[0][0] | ((ushort)VX[0][1] << 16));
        case 1:
            return VX[0][2];
        case 2:
            return ((ushort)VX[1][0] | ((ushort)VX[1][1] << 16));
        case 3:
            return VX[1][2];
        case 4:
            return ((ushort)VX[2][0] | ((ushort)VX[2][1] << 16));
        case 5:
            return VX[2][2];
        case 6:
            return RGBC.r | (RGBC.g << 8) | (RGBC.b << 16) | (RGBC.cd << 24);
        case 7:
            return OTZ;
        case 8:
            return IR[0];
        case 9:
            return IR[1];
        case 10:
            return IR[2];
        case 11:
            return IR[3];
        case 12:
            return ((ushort)SXY_FIFO[0].x | ((ushort)SXY_FIFO[0].y << 16));
        case 13:
            return ((ushort)SXY_FIFO[1].x | ((ushort)SXY_FIFO[1].y << 16));
        case 14:
            return ((ushort)SXY_FIFO[2].x | ((ushort)SXY_FIFO[2].y << 16));
        case 15:
            return ((ushort)SXY_FIFO[3].x | ((ushort)SXY_FIFO[3].y << 16));
        case 16:
            return SZ_FIFO[0];
        case 17:
            return SZ_FIFO[1];
        case 18:
            return SZ_FIFO[2];
        case 19:
            return SZ_FIFO[3];
        case 20:
            return RGBC_FIFO[0].r | (RGBC_FIFO[0].g << 8) | (RGBC_FIFO[0].b << 16) | (RGBC_FIFO[0].cd << 24);
        case 21:
            return RGBC_FIFO[1].r | (RGBC_FIFO[1].g << 8) | (RGBC_FIFO[1].b << 16) | (RGBC_FIFO[1].cd << 24);
        case 22:
            return RGBC_FIFO[2].r | (RGBC_FIFO[2].g << 8) | (RGBC_FIFO[2].b << 16) | (RGBC_FIFO[2].cd << 24);
        case 23:
            printf("[!] Attempted to read RES1. Please do not do that.\n");
            return 0;
        case 24:
            return MAC[0];
        case 25:
            return MAC[1];
        case 26:
            return MAC[2];
        case 27:
            return MAC[3];
        case 28:
        case 29:
            return (
                std::clamp(IR[1] >> 7, 0, 0x1F) |
                (std::clamp(IR[2] >> 7, 0, 0x1F) << 5) |
                (std::clamp(IR[3] >> 7, 0, 0x1F) << 10)
            );
        case 30:
            return LZCS;
        case 31:
            return LZCR;
        default:
            return 0;
    }
}

void GteEmulator::WriteDataRegister(uint num, uint value) {

    // Check which data register to write to
    switch (num) {
        case 0:
            VX[0][0] = value;
            VX[0][1] = (value >> 16);
            break;
        case 1:
            VX[0][2] = value;
            break;
        case 2:
            VX[1][0] = value;
            VX[1][1] = (value >> 16);
            break;
        case 3:
            VX[1][2] = value;
            break;
        case 4:
            VX[2][0] = value;
            VX[2][1] = (value >> 16);
            break;
        case 5:
            VX[2][2] = value;
            break;
        case 6:
            RGBC.r = value;
            RGBC.g = value >> 8;
            RGBC.b = value >> 16;
            RGBC.cd = value >> 24;
            break;
        case 7:
            OTZ = value;
            break;
        case 8:
            IR[0] = value;
            break;
        case 9:
            IR[1] = value;
            break;
        case 10:
            IR[2] = value;
            break;
        case 11:
            IR[3] = value;
            break;
        case 12:
            SXY_FIFO[0].x = value;
            SXY_FIFO[0].y = (value >> 16);
            break;
        case 13:
            SXY_FIFO[1].x = value;
            SXY_FIFO[1].y = (value >> 16);
            break;
        case 14:
            SXY_FIFO[2].x = value;
            SXY_FIFO[2].y = (value >> 16);
            // SXYP mirrors SXY2
            SXY_FIFO[3].x = value;
            SXY_FIFO[3].y = (value >> 16);
            break;
        case 15:
            SXY_FIFO[3].x = value;
            SXY_FIFO[3].y = (value >> 16);
            // GTE Register Specification 5-37
            SXY_FIFO[0] = SXY_FIFO[1];
            SXY_FIFO[1] = SXY_FIFO[2];
            SXY_FIFO[2] = SXY_FIFO[3];
            break;
        case 16:
            SZ_FIFO[0] = value;
            break;
        case 17:
            SZ_FIFO[1] = value;
            break;
        case 18:
            SZ_FIFO[2] = value;
            break;
        case 19:
            SZ_FIFO[3] = value;
            break;
        case 20:
            RGBC_FIFO[0].r = value;
            RGBC_FIFO[0].g = value >> 8;
            RGBC_FIFO[0].b = value >> 16;
            RGBC_FIFO[0].cd = value >> 24;
            break;
        case 21:
            RGBC_FIFO[1].r = value;
            RGBC_FIFO[1].g = value >> 8;
            RGBC_FIFO[1].b = value >> 16;
            RGBC_FIFO[1].cd = value >> 24;
            break;
        case 22:
            RGBC_FIFO[2].r = value;
            RGBC_FIFO[2].g = value >> 8;
            RGBC_FIFO[2].b = value >> 16;
            RGBC_FIFO[2].cd = value >> 24;
            break;
        case 23:
            printf("[!] Attempted to write to RES1. Please do not do that.\n");
            break;
        case 24:
            MAC[0] = value;
            break;
        case 25:
            MAC[1] = value;
            break;
        case 26:
            MAC[2] = value;
            break;
        case 27:
            MAC[3] = value;
            break;
        case 28:
            IR[0] = (value & 0x1F) << 7;
            IR[1] = ((value >> 5) & 0x1F) << 7;
            IR[2] = ((value >> 10) & 0x1F) << 7;
            break;
        case 29:
            printf("[!] Attempted to write to ORGB. Please do not do that.\n");
            break;
        case 30:
            LZCS = value;
            // LZCR = 32 if LZCS is 0
            LZCR = 32;
            // LZCR = Leading zero count if LZCS is positive
            if (LZCS > 0) {
                LZCR = CountLeadingZeros(value, 32);
            }
            // LZCR = Leading one count if LZCS is negative
            else if (LZCS < 0) {
                LZCR = CountLeadingZeros(value ^ 0xFFFFFFFF, 32);
            }
            break;
        case 31:
            printf("[!] Attempted to write to LZCR. Please do not do that.\n");
        default:
            break;
    }
}

uint GteEmulator::ReadControlRegister(uint num) {

    // Check which control register to read from
    switch (num) {
        case 0:
            return ROT_MTX[0][0] | (ROT_MTX[0][1] << 16);
        case 1:
            return ROT_MTX[0][2] | (ROT_MTX[1][0] << 16);
        case 2:
            return ROT_MTX[1][1] | (ROT_MTX[1][2] << 16);
        case 3:
            return ROT_MTX[2][0] | (ROT_MTX[2][1] << 16);
        case 4:
            return ROT_MTX[2][2];
        case 5:
            return TRANS_VEC.x;
        case 6:
            return TRANS_VEC.y;
        case 7:
            return TRANS_VEC.z;
        case 8:
            return LIGHT_MTX[0][0] | (LIGHT_MTX[0][1] << 16);
        case 9:
            return LIGHT_MTX[0][2] | (LIGHT_MTX[1][0] << 16);
        case 10:
            return LIGHT_MTX[1][1] | (LIGHT_MTX[1][2] << 16);
        case 11:
            return LIGHT_MTX[2][0] | (LIGHT_MTX[2][1] << 16);
        case 12:
            return LIGHT_MTX[2][2];
        case 13:
            return BG_COLOR_VEC.r;
        case 14:
            return BG_COLOR_VEC.g;
        case 15:
            return BG_COLOR_VEC.b;
        case 16:
            return LIGHT_COLOR_MTX[0][0] | (LIGHT_COLOR_MTX[0][1] << 16);
        case 17:
            return LIGHT_COLOR_MTX[0][2] | (LIGHT_COLOR_MTX[1][0] << 16);
        case 18:
            return LIGHT_COLOR_MTX[1][1] | (LIGHT_COLOR_MTX[1][2] << 16);
        case 19:
            return LIGHT_COLOR_MTX[2][0] | (LIGHT_COLOR_MTX[2][1] << 16);
        case 20:
            return LIGHT_COLOR_MTX[2][2];
        case 21:
            return FAR_COLOR_VEC.r;
        case 22:
            return FAR_COLOR_VEC.g;
        case 23:
            return FAR_COLOR_VEC.b;
        case 24:
            return OFX;
        case 25:
            return OFY;
        case 26:
            return H;
        case 27:
            return DQA;
        case 28:
            return DQB;
        case 29:
            return ZSF3;
        case 30:
            return ZSF4;
        case 31:
            return FLAG;
    }
}

void GteEmulator::WriteControlRegister(uint num, uint value) {

    // Check which data register to write to
    switch (num) {
        case 0:
            ROT_MTX[0][0] = value;
            ROT_MTX[0][1] = (value >> 16);
            break;
        case 1:
            ROT_MTX[0][2] = value;
            ROT_MTX[1][0] = (value >> 16);
            break;
        case 2:
            ROT_MTX[1][1] = value;
            ROT_MTX[1][2] = (value >> 16);
            break;
        case 3:
            ROT_MTX[2][0] = value;
            ROT_MTX[2][1] = (value >> 16);
            break;
        case 4:
            ROT_MTX[2][2] = value;
            break;
        case 5:
            TRANS_VEC.x = value;
            break;
        case 6:
            TRANS_VEC.y = value;
            break;
        case 7:
            TRANS_VEC.z = value;
            break;
        case 8:
            LIGHT_MTX[0][0] = value;
            LIGHT_MTX[0][1] = (value >> 16);
            break;
        case 9:
            LIGHT_MTX[0][2] = value;
            LIGHT_MTX[1][0] = (value >> 16);
            break;
        case 10:
            LIGHT_MTX[1][1] = value;
            LIGHT_MTX[1][2] = (value >> 16);
            break;
        case 11:
            LIGHT_MTX[2][0] = value;
            LIGHT_MTX[2][1] = (value >> 16);
            break;
        case 12:
            LIGHT_MTX[2][2] = value;
            break;
        case 13:
            BG_COLOR_VEC.r = value;
            break;
        case 14:
            BG_COLOR_VEC.g = value;
            break;
        case 15:
            BG_COLOR_VEC.b = value;
            break;
        case 16:
            LIGHT_COLOR_MTX[0][0] = value;
            LIGHT_COLOR_MTX[0][1] = (value >> 16);
            break;
        case 17:
            LIGHT_COLOR_MTX[0][2] = value;
            LIGHT_COLOR_MTX[1][0] = (value >> 16);
            break;
        case 18:
            LIGHT_COLOR_MTX[1][1] = value;
            LIGHT_COLOR_MTX[1][2] = (value >> 16);
            break;
        case 19:
            LIGHT_COLOR_MTX[2][0] = value;
            LIGHT_COLOR_MTX[2][1] = (value >> 16);
            break;
        case 20:
            LIGHT_COLOR_MTX[2][2] = value;
            break;
        case 21:
            FAR_COLOR_VEC.r = value;
            break;
        case 22:
            FAR_COLOR_VEC.g = value;
            break;
        case 23:
            FAR_COLOR_VEC.b = value;
            break;
        case 24:
            OFX = value;
            break;
        case 25:
            OFY = value;
            break;
        case 26:
            H = value;
            break;
        case 27:
            DQA = value;
            break;
        case 28:
            DQB = value;
            break;
        case 29:
            ZSF3 = value;
            break;
        case 30:
            ZSF4 = value;
            break;
        case 31:
            FLAG = value;
            break;
        default:
            break;
    }
}






// -- Helper Functions -------------------------------------------------------------------------------------------

// Get the number of leading zeros in a given [x]-bit value
uint GteEmulator::CountLeadingZeros(uint value, uint num_bits) {
    uint count = 0;
    num_bits -= 1;
    uint mask = 1 << num_bits;
    for (count = 0; count < num_bits; count++) {
        if (value & (mask >> count)) {
            break;
        }
    }
    return count;
}

// Check for any overflows or underflows in calculation
static inline int CheckCalcBounds(int64 x, int flag_num) {

    // Check for bits 30-28 (overflow) and 27-25 (underflow)
    if (flag_num < 4) {
        if (x > 0x7FFFFFFFFFF) {
            GteEmulator::FLAG |= 30 - (flag_num - 1);
        }
        else if (x < -0x80000000000) {
            GteEmulator::FLAG |= 27 - (flag_num - 1);
        }

        // Sign extend the value of X
        x = (x << 20) >> 20;
    }

    // Only other case is flag_num == 4
    else {
        if (x > 0x7FFFFFFF) {
            GteEmulator::FLAG |= 16;
        }
        else if (x < -0x80000000) {
            GteEmulator::FLAG |= 15;
        }
    }

    // Return the value
    return (int)(x & 0xFFFFFFFF);
}

// Divide
static inline uint divide(uint dividend, uint divisor) {

    // Check for division overflow
    if (divisor * 2 > dividend) {

        // Get the shift amount
        uint shift = GteEmulator::CountLeadingZeros(divisor, 16);

        // Shift both values
        dividend <<= shift;
        divisor = (divisor << shift) | 0x8000;

        // Get the division table index
        uint div_idx = ((divisor & 0x7FFF) + 0x40) >> 7;

        // Get the base division value
        int div_val = 0x101 + DIV_TABLE[div_idx];

        // ye
        int num1 = (0x80 + (-div_val * (int)divisor)) >> 8;
        int num2 = (0x80 + (div_val * (0x20000 + num1))) >> 8;

        // Find the final value
        uint result = ((ulong)dividend * num2 + 0x8000) >> 16;
        return std::min<uint>(0x1FFFF, result);
    }

    // Division overflow
    else {
        GteEmulator::FLAG |= (1 << 17);
        return 0x1FFFF;
    }
}

static inline short limA1S(int64 x) {
    short result = (short)std::clamp<int64>(x, -0x8000, 0x7FFF);
    GteEmulator::FLAG |= (result != x) << 24;
    return result;
}

static inline short limA2S(int64 x) {
    short result = (short)std::clamp<int64>(x, -0x8000, 0x7FFF);
    GteEmulator::FLAG |= (result != x) << 23;
    return result;
}

static inline short limA3S(int64 x) {
    short result = (short)std::clamp<int64>(x, -0x8000, 0x7FFF);
    GteEmulator::FLAG |= (result != x) << 22;
    return result;
}

static inline short limA1U(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0x7FFF);
    GteEmulator::FLAG |= (result != x) << 24;
    return result;
}

static inline short limA2U(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0x7FFF);
    GteEmulator::FLAG |= (result != x) << 23;
    return result;
}

static inline short limA3U(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0x7FFF);
    GteEmulator::FLAG |= (result != x) << 22;
    return result;
}

static inline ushort limB1(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0xFF);
    GteEmulator::FLAG |= (result != x) << 21;
    return result;
}

static inline ushort limB2(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0xFF);
    GteEmulator::FLAG |= (result != x) << 20;
    return result;
}

static inline ushort limB3(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0xFF);
    GteEmulator::FLAG |= (result != x) << 19;
    return result;
}

static inline short limC(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0xFFFF);
    GteEmulator::FLAG |= (result != x) << 18;
    return result;
}

static inline short limD1(int64 x) {
    short result = (short)std::clamp<int64>(x, -0x400, 0x3FF);
    GteEmulator::FLAG |= (result != x) << 14;
    return result;
}

static inline short limD2(int64 x) {
    short result = (short)std::clamp<int64>(x, -0x400, 0x3FF);
    GteEmulator::FLAG |= (result != x) << 13;
    return result;
}

static inline short limE(int64 x) {
    short result = (short)std::clamp<int64>(x, 0, 0xFFF);
    GteEmulator::FLAG |= (result != x) << 12;
    return result;
}

// Multiplies a 3x3 matrix with a 3-item vector
static inline void MxV(gte_vec3i base, short mtx[3][3], short vec[3], uint lm, uint sf, bool fc = false) {

    // Expand base values
    int64 base1 = base.x << 12;
    int64 base2 = base.y << 12;
    int64 base3 = base.z << 12;

    // Multiply first 2 matrix/vector components
    int64 val1 = (mtx[0][0] * vec[0]) + (mtx[0][1] * vec[1]);
    int64 val2 = (mtx[1][0] * vec[0]) + (mtx[1][1] * vec[1]);
    int64 val3 = (mtx[2][0] * vec[0]) + (mtx[2][1] * vec[1]);

    // Multiply last matrix/vector components
    int64 last1 = (mtx[0][2] * vec[2]);
    int64 last2 = (mtx[1][2] * vec[2]);
    int64 last3 = (mtx[2][2] * vec[2]);

    // Combine all 3 components for handling all cases except for Far Color
    val1 += last1;
    val2 += last2;
    val3 += last3;

    // PSX hardware handles the Far Color matrix incorrectly for matrix multiplication
    // As a result, only the last part of the formula is performed
    // i.e.
    //
    //     Formula:  (base + MX11*V0 + MX12*V1 + MX13*V2) >> SF*12
    //
    //     Omit:     (base + MX11*V0 + MX12*V1)
    //     Process:                              (MX13*V2) >> SF*12
    //
    if (fc) {

        // Set flags as though the full calculation took place
        CheckCalcBounds(base1 + val1, 1);
        CheckCalcBounds(base2 + val2, 1);
        CheckCalcBounds(base3 + val3, 1);

        // Only perform the last part of the calculation
        val1 = last1;
        val2 = last2;
        val3 = last3;
        base1 = 0;
        base2 = 0;
        base3 = 0;
    }

    // Check bounds and set math accumulators
    GteEmulator::MAC[1] = (int)(CheckCalcBounds(base1 + val1, 1) >> sf);
    GteEmulator::MAC[2] = (int)(CheckCalcBounds(base2 + val2, 2) >> sf);
    GteEmulator::MAC[3] = (int)(CheckCalcBounds(base3 + val3, 3) >> sf);

    // Set unsigned lower limit if lm flag is set
    if (lm == 1) {
        GteEmulator::IR[1] = limA1U(GteEmulator::MAC[1]);
        GteEmulator::IR[2] = limA2U(GteEmulator::MAC[2]);
        GteEmulator::IR[3] = limA3U(GteEmulator::MAC[3]);
    }
    // Otherwise use the signed limiters
    else {
        GteEmulator::IR[1] = limA1S(GteEmulator::MAC[1]);
        GteEmulator::IR[2] = limA2S(GteEmulator::MAC[2]);
        GteEmulator::IR[3] = limA3S(GteEmulator::MAC[3]);
    }
}






// -- GTE Instructions -------------------------------------------------------------------------------------------

void GteEmulator::gte_nop(uint lm, uint tv, uint mv, uint mm, uint sf) {

}


// Rotate, Translate, Perspective (Single)
void GteEmulator::gte_RTPS(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: RTPS\n");
    }

    MxV(TRANS_VEC, ROT_MTX, VX[0], lm, sf);

    SZ_FIFO[0] = SZ_FIFO[1];
    SZ_FIFO[1] = SZ_FIFO[2];
    SZ_FIFO[2] = SZ_FIFO[3];
    SZ_FIFO[3] = limC(MAC[3]);

    uint div_val = divide(H, SZ_FIFO[3]);

    int SX = (int)(CheckCalcBounds(OFX + IR[1] * div_val, 4) >> 16);
    int SY = (int)(CheckCalcBounds(OFY + IR[2] * div_val, 4) >> 16);
    int P = (int)CheckCalcBounds(DQB + DQA * div_val, 4);

    IR[0] = limE(P);

    SXY_FIFO[0] = SXY_FIFO[1];
    SXY_FIFO[1] = SXY_FIFO[2];
    SXY_FIFO[2].x = limD1(SX);
    SXY_FIFO[2].y = limD2(SY);
    SXY_FIFO[3] = SXY_FIFO[2];

    MAC[0] = P;
}

// Normal Clipping
void GteEmulator::gte_NCLIP(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCLIP\n");
    }

    /*
    SXY_FIFO[0].x = (short)0xFFB9;
    SXY_FIFO[0].y = (short)0x0029;
    SXY_FIFO[1].x = (short)0xFFB9;
    SXY_FIFO[1].y = (short)0x0021;
    SXY_FIFO[2].x = (short)0xFFB9;
    SXY_FIFO[2].y = (short)0x00A1;
    */

    int64 A = SXY_FIFO[0].x * (SXY_FIFO[1].y - SXY_FIFO[2].y);
    int64 B = SXY_FIFO[1].x * (SXY_FIFO[2].y - SXY_FIFO[0].y);
    int64 C = SXY_FIFO[2].x * (SXY_FIFO[0].y - SXY_FIFO[1].y);

    MAC[0] = (int)(CheckCalcBounds(A + B + C, 4));
}

// Outer Product of 2 Vectors
void GteEmulator::gte_OP(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: OP\n");
    }

    MAC[1] = (int)(CheckCalcBounds((ROT_MTX[1][1] * IR[3]) - (ROT_MTX[2][2] * IR[2]), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds((ROT_MTX[2][2] * IR[1]) - (ROT_MTX[0][0] * IR[3]), 1) >> sf);
    MAC[3] = (int)(CheckCalcBounds((ROT_MTX[0][0] * IR[2]) - (ROT_MTX[1][1] * IR[1]), 1) >> sf);

    IR[1] = limA1S(MAC[1]);
    IR[2] = limA2S(MAC[2]);
    IR[3] = limA3S(MAC[3]);
}

// Depth Cue (Single)
void GteEmulator::gte_DPCS(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: DPCS\n");
    }

    int R = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.r << 12) - (RGBC.r << 12), 1) >> sf);
    int G = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.g << 12) - (RGBC.g << 12), 2) >> sf);
    int B = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.b << 12) - (RGBC.b << 12), 3) >> sf);

    MAC[1] = (int)(CheckCalcBounds((int64)(RGBC.r << 12) + (IR[0] * limA1S(R)), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds((int64)(RGBC.g << 12) + (IR[0] * limA2S(G)), 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds((int64)(RGBC.b << 12) + (IR[0] * limA3S(B)), 3) >> sf);

    IR[1] = limA1S(MAC[1]);
    IR[2] = limA2S(MAC[2]);
    IR[3] = limA3S(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Interpolate (vector & far color vector)
void GteEmulator::gte_INTPL(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: INTPL\n");
    }

    int R = (int)((((int64)(FAR_COLOR_VEC.r) << 12) - ((int)IR[1] << 12)) >> sf);
    int G = (int)((((int64)(FAR_COLOR_VEC.g) << 12) - ((int)IR[2] << 12)) >> sf);
    int B = (int)((((int64)(FAR_COLOR_VEC.b) << 12) - ((int)IR[3] << 12)) >> sf);

    MAC[1] = (int)(CheckCalcBounds(((int64)IR[1] << 12) + (IR[0] * limA1S(R)), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds(((int64)IR[2] << 12) + (IR[0] * limA1S(G)), 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds(((int64)IR[3] << 12) + (IR[0] * limA1S(B)), 3) >> sf);

    IR[1] = limA1S(MAC[1]);
    IR[2] = limA2S(MAC[2]);
    IR[3] = limA3S(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Multiply Vector by Matrix Vector and Add
void GteEmulator::gte_MVMVA(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: MVMA\n");
    }

    // IR vector for [mv == 3]
    short ir_vec[3] = {IR[1], IR[2], IR[3]};

    short (*matrix)[3] = nullptr;
    gte_vec3i add = {0, 0, 0};
    bool fc = false;
    short* vector = ir_vec;

    // Multiplication matrix
    switch (mm) {
        case 0:     matrix = ROT_MTX;          break;
        case 1:     matrix = LIGHT_MTX;        break;
        case 2:     matrix = LIGHT_COLOR_MTX;  break;
        default:                               break;
    }

    // Multiplication vector
    switch (mv) {
        case 0:     vector = VX[0];     break;
        case 1:     vector = VX[1];     break;
        case 2:     vector = VX[2];     break;
        case 3:     vector = ir_vec;    break;
        default:                        break;
    }

    // Translation (Addition) vector
    switch (tv) {
        case 0:     add = TRANS_VEC;        fc = false;     break;
        case 1:     add = BG_COLOR_VEC;     fc = false;     break;
        case 2:     add = FAR_COLOR_VEC;    fc = true;      break;
        default:                            fc = false;     break;
    }

    // Do the thing
    MxV(add, matrix, vector, lm, sf, fc);
}

// Normal Color Depth Cue (Single)
void GteEmulator::gte_NCDS(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCDS\n");
    }

    gte_vec3i zero = {0, 0, 0};
    MxV(zero, LIGHT_MTX, VX[0], lm, sf);

    short products[3] = {IR[1], IR[2], IR[3]};
    MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

    int R = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.r << 12) - ((RGBC.r << 4) * IR[1]), 1) >> sf);
    int G = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.g << 12) - ((RGBC.g << 4) * IR[2]), 2) >> sf);
    int B = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.b << 12) - ((RGBC.b << 4) * IR[3]), 3) >> sf);

    MAC[1] = (int)(CheckCalcBounds(((RGBC.r << 4) * IR[1]) + (IR[0] * limA1S(R)), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds(((RGBC.g << 4) * IR[2]) + (IR[0] * limA2S(G)), 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds(((RGBC.b << 4) * IR[3]) + (IR[0] * limA3S(B)), 3) >> sf);

    IR[1] = limA1U(MAC[1]);
    IR[2] = limA2U(MAC[2]);
    IR[3] = limA3U(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Color Depth Cue
void GteEmulator::gte_CDP(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: CDP\n");
    }

    short products[3] = {IR[1], IR[2], IR[3]};
    MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

    int R = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.r << 12) - ((RGBC.r << 4) * IR[1]), 1) >> sf);
    int G = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.g << 12) - ((RGBC.g << 4) * IR[2]), 2) >> sf);
    int B = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.b << 12) - ((RGBC.b << 4) * IR[3]), 3) >> sf);

    MAC[1] = (int)(CheckCalcBounds(((RGBC.r << 4) * IR[1]) + (IR[0] * limA1S(R)), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds(((RGBC.g << 4) * IR[2]) + (IR[0] * limA2S(G)), 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds(((RGBC.b << 4) * IR[3]) + (IR[0] * limA3S(B)), 3) >> sf);

    IR[1] = limA1U(MAC[1]);
    IR[2] = limA2U(MAC[2]);
    IR[3] = limA3U(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Normal Color Depth Cue (Triple)
void GteEmulator::gte_NCDT(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCDT\n");
    }

    for (int i = 0; i < 3; i++) {

        gte_vec3i zero = {0, 0, 0};
        MxV(zero, LIGHT_MTX, VX[i], lm, sf);

        short products[3] = {IR[1], IR[2], IR[3]};
        MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

        int R = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.r << 12) - ((RGBC.r << 4) * IR[1]), 1) >> sf);
        int G = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.g << 12) - ((RGBC.g << 4) * IR[2]), 2) >> sf);
        int B = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.b << 12) - ((RGBC.b << 4) * IR[3]), 3) >> sf);

        MAC[1] = (int)(CheckCalcBounds(((RGBC.r << 4) * IR[1]) + (IR[0] * limA1S(R)), 1) >> sf);
        MAC[2] = (int)(CheckCalcBounds(((RGBC.g << 4) * IR[2]) + (IR[0] * limA2S(G)), 2) >> sf);
        MAC[3] = (int)(CheckCalcBounds(((RGBC.b << 4) * IR[3]) + (IR[0] * limA3S(B)), 3) >> sf);

        IR[1] = limA1U(MAC[1]);
        IR[2] = limA2U(MAC[2]);
        IR[3] = limA3U(MAC[3]);

        RGBC_FIFO[0] = RGBC_FIFO[1];
        RGBC_FIFO[1] = RGBC_FIFO[2];
        RGBC_FIFO[2].r = limB1(MAC[1]);
        RGBC_FIFO[2].g = limB2(MAC[2]);
        RGBC_FIFO[2].b = limB3(MAC[3]);
        RGBC_FIFO[2].cd = RGBC.cd;
    }
}

// Normal Color Color (Single)
void GteEmulator::gte_NCCS(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCCS\n");
    }

    gte_vec3i zero = {0, 0, 0};
    MxV(zero, LIGHT_MTX, VX[0], lm, sf);

    short products[3] = {IR[1], IR[2], IR[3]};
    MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

    MAC[1] = ((RGBC.r << 4) * IR[1]) >> sf;
    MAC[2] = ((RGBC.g << 4) * IR[2]) >> sf;
    MAC[3] = ((RGBC.b << 4) * IR[3]) >> sf;

    IR[1] = limA1U(MAC[1]);
    IR[2] = limA2U(MAC[2]);
    IR[3] = limA3U(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;

}

// Color Color
void GteEmulator::gte_CC(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: CC\n");
    }

    short products[3] = {IR[1], IR[2], IR[3]};
    MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

    MAC[1] = ((RGBC.r << 4) * IR[1]) >> sf;
    MAC[2] = ((RGBC.g << 4) * IR[2]) >> sf;
    MAC[3] = ((RGBC.b << 4) * IR[3]) >> sf;

    IR[1] = limA1U(MAC[1]);
    IR[2] = limA2U(MAC[2]);
    IR[3] = limA3U(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Normal Color (Single)
void GteEmulator::gte_NCS(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCS\n");
    }

    gte_vec3i zero = {0, 0, 0};
    MxV(zero, LIGHT_MTX, VX[0], lm, sf);

    short products[3] = {IR[1], IR[2], IR[3]};
    MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Normal Color (Triple)
void GteEmulator::gte_NCT(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCT\n");
    }

    for (int i = 0; i < 3; i++) {

        gte_vec3i zero = {0, 0, 0};
        MxV(zero, LIGHT_MTX, VX[i], lm, sf);

        short products[3] = {IR[1], IR[2], IR[3]};
        MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

        RGBC_FIFO[0] = RGBC_FIFO[1];
        RGBC_FIFO[1] = RGBC_FIFO[2];
        RGBC_FIFO[2].r = limB1(MAC[1]);
        RGBC_FIFO[2].g = limB2(MAC[2]);
        RGBC_FIFO[2].b = limB3(MAC[3]);
        RGBC_FIFO[2].cd = RGBC.cd;
    }

}

// Square of IR Vectors
void GteEmulator::gte_SQR(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: SQR\n");
    }

    MAC[1] = (IR[1] * IR[1]) >> sf;
    MAC[2] = (IR[2] * IR[2]) >> sf;
    MAC[3] = (IR[3] * IR[3]) >> sf;

    IR[1] = limA1U(MAC[1]);
    IR[2] = limA2U(MAC[2]);
    IR[3] = limA3U(MAC[3]);
}

// Depth Cue Color Light
void GteEmulator::gte_DCPL(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: DCPL\n");
    }

    int R = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.r << 12) - ((RGBC.r << 4) * IR[1]), 1) >> sf);
    int G = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.g << 12) - ((RGBC.g << 4) * IR[2]), 2) >> sf);
    int B = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.b << 12) - ((RGBC.b << 4) * IR[3]), 3) >> sf);

    MAC[1] = (int)(CheckCalcBounds(((RGBC.r << 4) * IR[1]) + (IR[0] * limA1S(R)), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds(((RGBC.g << 4) * IR[2]) + (IR[0] * limA2S(G)), 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds(((RGBC.b << 4) * IR[3]) + (IR[0] * limA3S(B)), 3) >> sf);

    IR[1] = limA1S(MAC[1]);
    IR[2] = limA2S(MAC[2]);
    IR[3] = limA3S(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Depth Cue (Triple)
void GteEmulator::gte_DPCT(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: DPCT\n");
    }

    for (int i = 0; i < 3; i++) {

        int R = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.r << 12) - (RGBC_FIFO[0].r << 12), 1) >> sf);
        int G = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.g << 12) - (RGBC_FIFO[0].g << 12), 2) >> sf);
        int B = (int)(CheckCalcBounds((int64)(FAR_COLOR_VEC.b << 12) - (RGBC_FIFO[0].b << 12), 3) >> sf);

        MAC[1] = (int)(CheckCalcBounds((int64)(RGBC_FIFO[0].r << 12) + (IR[0] * limA1S(R)), 1) >> sf);
        MAC[2] = (int)(CheckCalcBounds((int64)(RGBC_FIFO[0].g << 12) + (IR[0] * limA2S(G)), 2) >> sf);
        MAC[3] = (int)(CheckCalcBounds((int64)(RGBC_FIFO[0].b << 12) + (IR[0] * limA3S(B)), 3) >> sf);

        IR[1] = limA1S(MAC[1]);
        IR[2] = limA2S(MAC[2]);
        IR[3] = limA3S(MAC[3]);

        RGBC_FIFO[0] = RGBC_FIFO[1];
        RGBC_FIFO[1] = RGBC_FIFO[2];
        RGBC_FIFO[2].r = limB1(MAC[1]);
        RGBC_FIFO[2].g = limB2(MAC[2]);
        RGBC_FIFO[2].b = limB3(MAC[3]);
        RGBC_FIFO[2].cd = RGBC.cd;
    }
}

// Average of 3 SZ Values
void GteEmulator::gte_AVSZ3(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: AVSZ3\n");
    }

    MAC[0] = (int)(CheckCalcBounds((int64)ZSF3 * (SZ_FIFO[1] + SZ_FIFO[2] + SZ_FIFO[3]), 4));
    OTZ = limC(MAC[0] >> 12);
}

void GteEmulator::gte_AVSZ4(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: AVSZ4\n");
    }

    MAC[0] = (int)(CheckCalcBounds((int64)ZSF4 * (SZ_FIFO[0] + SZ_FIFO[1] + SZ_FIFO[2] + SZ_FIFO[3]), 4));
    OTZ = limC(MAC[0] >> 12);
}

// Rotate, Translate, Perspective (Triple)
void GteEmulator::gte_RTPT(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: RTPT\n");
    }

    for (int i = 0; i < 3; i++) {

        MxV(TRANS_VEC, ROT_MTX, VX[i], lm, sf);

        SZ_FIFO[0] = SZ_FIFO[1];
        SZ_FIFO[1] = SZ_FIFO[2];
        SZ_FIFO[2] = SZ_FIFO[3];
        SZ_FIFO[3] = limC(MAC[3]);

        uint div_val = divide(H, SZ_FIFO[3]);

        short SX = (short)(CheckCalcBounds((int64)OFX + IR[1] * div_val, 4) >> 16);
        short SY = (short)(CheckCalcBounds((int64)OFY + IR[2] * div_val, 4) >> 16);
        int P = (int)CheckCalcBounds(DQB + DQA * div_val, 4);

        IR[0] = limE(P);

        SXY_FIFO[3].x = limD1(SX);
        SXY_FIFO[3].y = limD2(SY);
        SXY_FIFO[0] = SXY_FIFO[1];
        SXY_FIFO[1] = SXY_FIFO[2];
        SXY_FIFO[2] = SXY_FIFO[3];

        MAC[0] = IR[0];
    }
}

// General Purpose Interpolation
void GteEmulator::gte_GPF(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: GPF\n");
    }

    MAC[1] = (int)(CheckCalcBounds(IR[0] * IR[1], 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds(IR[0] * IR[2], 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds(IR[0] * IR[3], 3) >> sf);

    IR[1] = limA1S(MAC[1]);
    IR[2] = limA2S(MAC[2]);
    IR[3] = limA3S(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// General Purpose Interpolation (with Base)
void GteEmulator::gte_GPL(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: GPL\n");
    }

    MAC[1] = (int)(CheckCalcBounds((int64)(MAC[1] << sf) + (IR[0] * IR[1]), 1) >> sf);
    MAC[2] = (int)(CheckCalcBounds((int64)(MAC[2] << sf) + (IR[0] * IR[2]), 2) >> sf);
    MAC[3] = (int)(CheckCalcBounds((int64)(MAC[3] << sf) + (IR[0] * IR[3]), 3) >> sf);

    IR[1] = limA1S(MAC[1]);
    IR[2] = limA2S(MAC[2]);
    IR[3] = limA3S(MAC[3]);

    RGBC_FIFO[0] = RGBC_FIFO[1];
    RGBC_FIFO[1] = RGBC_FIFO[2];
    RGBC_FIFO[2].r = limB1(MAC[1]);
    RGBC_FIFO[2].g = limB2(MAC[2]);
    RGBC_FIFO[2].b = limB3(MAC[3]);
    RGBC_FIFO[2].cd = RGBC.cd;
}

// Normal Color Color (Triple)
void GteEmulator::gte_NCCT(uint lm, uint tv, uint mv, uint mm, uint sf) {

    if (debug) {
        printf("GTE :: NCCT\n");
    }

    for (int i = 0; i < 3; i++) {

        gte_vec3i zero = {0, 0, 0};
        MxV(zero, LIGHT_MTX, VX[i], lm, sf);

        short products[3] = {IR[1], IR[2], IR[3]};
        MxV(BG_COLOR_VEC, LIGHT_COLOR_MTX, products, lm, sf);

        MAC[1] = ((RGBC.r << 4) * IR[1]) >> sf;
        MAC[2] = ((RGBC.g << 4) * IR[2]) >> sf;
        MAC[3] = ((RGBC.b << 4) * IR[3]) >> sf;

        IR[1] = limA1U(MAC[1]);
        IR[2] = limA2U(MAC[2]);
        IR[3] = limA3U(MAC[3]);

        RGBC_FIFO[0] = RGBC_FIFO[1];
        RGBC_FIFO[1] = RGBC_FIFO[2];
        RGBC_FIFO[2].r = limB1(MAC[1]);
        RGBC_FIFO[2].g = limB2(MAC[2]);
        RGBC_FIFO[2].b = limB3(MAC[3]);
        RGBC_FIFO[2].cd = RGBC.cd;
    }
}
