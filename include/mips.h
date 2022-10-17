#ifndef SOTN_EDITOR_MIPS
#define SOTN_EDITOR_MIPS
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "entities.h"
#include "common.h"

// MIPS function type declarations
typedef void (*i_fn)(uint, uint, short);
typedef void (*r_fn)(uint, uint, uint, uint);
typedef void (*j_fn)(uint);
typedef void (*c_fn)(uint, uint, uint);

// Register mnemonics
const uint ZERO = 0;
const uint AT = 1;
const uint V0 = 2;
const uint V1 = 3;
const uint A0 = 4;
const uint A1 = 5;
const uint A2 = 6;
const uint A3 = 7;
const uint T0 = 8;
const uint T1 = 9;
const uint T2 = 10;
const uint T3 = 11;
const uint T4 = 12;
const uint T5 = 13;
const uint T6 = 14;
const uint T7 = 15;
const uint S0 = 16;
const uint S1 = 17;
const uint S2 = 18;
const uint S3 = 19;
const uint S4 = 20;
const uint S5 = 21;
const uint S6 = 22;
const uint S7 = 23;
const uint T8 = 24;
const uint T9 = 25;
const uint K0 = 26;
const uint K1 = 27;
const uint GP = 28;
const uint SP = 29;
const uint FP = 30;
const uint RA = 31;

// Main return value
const uint FUNCTION_RETURN = 0xFFFFFFFF;

// Maximum instruction executions (to prevent infinite loops)
const uint MAX_EXECUTIONS = 65536;

// CLUT data size
const uint CLUT_DATA_SIZE = 0x6000;

// These are used for detecting locations of framebuffer-related operations
const uint LOAD_IMAGE_ADDR = 0x00012B24;
const uint STORE_IMAGE_ADDR = 0x00012B88;
const uint MOVE_IMAGE_ADDR = 0x00012BEC;
const uint CLEAR_IMAGE_ADDR = 0x00012A90;

// Class for general utilities
class MipsEmulator {

    public:

        // Flag whether initialization has been performed
        static bool initialized;

        // Whether to print debug information or not
        static bool debug;

        // 2 MB buffer for PSX memory
        static byte* ram;

        // 32 MIPS registers
        static uint registers[32];

        // Names for each MIPS register
        static constexpr char* register_names[32] = {
            (char*)"ZERO",
            (char*)"AT",
            (char*)"V0", (char*)"V1",
            (char*)"A0", (char*)"A1", (char*)"A2", (char*)"A3",
            (char*)"T0", (char*)"T1", (char*)"T2", (char*)"T3", (char*)"T4", (char*)"T5", (char*)"T6", (char*)"T7",
            (char*)"S0", (char*)"S1", (char*)"S2", (char*)"S3", (char*)"S4", (char*)"S5", (char*)"S6", (char*)"S7",
            (char*)"T8", (char*)"T9",
            (char*)"K0", (char*)"K1",
            (char*)"GP",
            (char*)"SP",
            (char*)"FP",
            (char*)"RA"
        };

        // Dedicated PC register apart from the others
        static uint pc;

        // Counter for number of instructions executed
        static uint num_executed;

        // Framebuffer for LoadImage, StoreImage, MoveImage, and ClearImage
        static GLuint framebuffer;

        // MIPS emulator functions
        static void SetPSXBinary(const char* filename);
        static void SetSotNBinary(const char* filename);
        static void LoadMapFile(const char* filename);
        static void StoreMapCLUT(uint offset, uint count, byte* data);
        static void Initialize(void);
        static void Reset(void);
        static void ClearRegisters(void);
        static void Cleanup(void);
        static void ProcessOpcode(uint opcode);
        static void ProcessFunction(uint func_addr);
        static void LoadFile(const char* filename, uint addr);
        static uint ReadIntFromRAM(uint addr);
        static void WriteIntToRAM(uint addr, uint value);
        static void CopyToRAM(uint addr, const void* src, uint count);
        static void CopyFromRAM(uint addr, void* dst, uint count);
        static std::vector<Entity> FindNewEntities(int exclude_addr);
        static std::vector<Entity> ProcessEntities(void);

        // Used for framebuffer operations
        static void LoadImage(RECT* rect, byte* src);
        static void StoreImage(RECT* rect, byte* dst);
        static void MoveImage(RECT* rect, int x, int y);
        static void ClearImage(RECT* rect, byte r, byte g, byte b);

        // MIPS I-type instructions
        static void i_nop(uint src_index, uint dst_index, short imm);
        static void i_bltz(uint src_index, uint dst_index, short imm);
        static void i_bgez(uint src_index, uint dst_index, short imm);
        static void i_beq(uint src_index, uint dst_index, short imm);
        static void i_bne(uint src_index, uint dst_index, short imm);
        static void i_blez(uint src_index, uint dst_index, short imm);
        static void i_bgtz(uint src_index, uint dst_index, short imm);
        static void i_addi(uint src_index, uint dst_index, short imm);
        static void i_addiu(uint src_index, uint dst_index, short imm);
        static void i_slti(uint src_index, uint dst_index, short imm);
        static void i_sltiu(uint src_index, uint dst_index, short imm);
        static void i_andi(uint src_index, uint dst_index, short imm);
        static void i_ori(uint src_index, uint dst_index, short imm);
        static void i_xori(uint src_index, uint dst_index, short imm);
        static void i_lui(uint src_index, uint dst_index, short imm);
        static void i_lb(uint src_index, uint dst_index, short imm);
        static void i_lh(uint src_index, uint dst_index, short imm);
        static void i_lwl(uint src_index, uint dst_index, short imm);
        static void i_lw(uint src_index, uint dst_index, short imm);
        static void i_lbu(uint src_index, uint dst_index, short imm);
        static void i_lhu(uint src_index, uint dst_index, short imm);
        static void i_lwr(uint src_index, uint dst_index, short imm);
        static void i_sb(uint src_index, uint dst_index, short imm);
        static void i_sh(uint src_index, uint dst_index, short imm);
        static void i_swl(uint src_index, uint dst_index, short imm);
        static void i_sw(uint src_index, uint dst_index, short imm);
        static void i_swr(uint src_index, uint dst_index, short imm);
        // static void i_ll(uint src_index, uint dst_index, short imm);
        // static void i_lwc1(uint src_index, uint dst_index, short imm);
        static void i_lwc2(uint src_index, uint dst_index, short imm);
        // static void i_lwc3(uint src_index, uint dst_index, short imm);
        // static void i_sc(uint src_index, uint dst_index, short imm);
        // static void i_swc1(uint src_index, uint dst_index, short imm);
        static void i_swc2(uint src_index, uint dst_index, short imm);
        // static void i_swc3(uint src_index, uint dst_index, short imm);

        // MIPS R-type instructions
        static void r_nop(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_sll(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_srl(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_sra(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_sllv(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_srlv(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_srav(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_jr(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_jalr(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_mfhi(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_mthi(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_mflo(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_mtlo(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_mult(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_multu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_div(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_divu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_add(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_addu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_sub(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_subu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_and(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_or(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_xor(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_nor(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_slt(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        static void r_sltu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        // static void r_tge(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        // static void r_tgeu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        // static void r_tlt(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        // static void r_tltu(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        // static void r_teq(uint src_index1, uint src_index2, uint dst_index, uint shamt);
        // static void r_tne(uint src_index1, uint src_index2, uint dst_index, uint shamt);

        // MIPS J-type instructions
        static void j_nop(uint offset);
        static void j_j(uint offset);
        static void j_jal(uint offset);

        // MIPS coprocessor instructions
        static void c_mfc(uint cop_num, uint src_index, uint dst_index);
        static void c_cfc(uint cop_num, uint src_index, uint dst_index);
        static void c_mtc(uint cop_num, uint src_index, uint dst_index);
        static void c_ctc(uint cop_num, uint src_index, uint dst_index);
        static void c_bcx(uint cop_num, uint cond, uint dest);

        // Immediate function list
        const static constexpr i_fn itype_funcs[64] = {
            i_nop,
            i_bltz,      // Shortcut for BLTZ
            i_bgez,      // Shortcut for BGEZ
            i_nop,
            i_beq,
            i_bne,
            i_blez,
            i_bgtz,
            i_addi,
            i_addiu,
            i_slti,
            i_sltiu,
            i_andi,
            i_ori,
            i_xori,
            i_lui,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_nop,
            i_lb,
            i_lh,
            i_lwl,
            i_lw,
            i_lbu,
            i_lhu,
            i_lwr,
            i_nop,
            i_sb,
            i_sh,
            i_swl,
            i_sw,
            i_nop,
            i_nop,
            i_swr,
            i_nop,
            i_nop, // ll
            i_nop, // lwc1
            i_lwc2, // lwc2
            i_nop, // pref
            i_nop,
            i_nop, // ldc1
            i_nop, // ldc2
            i_nop,
            i_nop, // sc
            i_nop, // swc1
            i_swc2, // swc2
            i_nop,
            i_nop,
            i_nop, // sdc1
            i_nop, // sdc2
            i_nop
        };

        // Register function list
        const static constexpr r_fn rtype_funcs[64] = {
            r_sll,
            r_nop,
            r_srl,
            r_sra,
            r_sllv,
            r_nop,
            r_srlv,
            r_srav,
            r_jr,
            r_jalr,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_mfhi,
            r_mthi,
            r_mflo,
            r_mtlo,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_mult,
            r_multu,
            r_div,
            r_divu,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_add,
            r_addu,
            r_sub,
            r_subu,
            r_and,
            r_or,
            r_xor,
            r_nor,
            r_nop,
            r_nop,
            r_slt,
            r_sltu,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop,
            r_nop
        };

        // Jump function list
        const static constexpr j_fn jtype_funcs[4] = {
            j_nop,
            j_nop,
            j_j,
            j_jal
        };

        // Coprocessor function list
        const static constexpr c_fn ctype_funcs[5] = {
            c_mfc,
            c_cfc,
            c_mtc,
            c_ctc,
            c_bcx
        };



    private:

        // 1 KB buffer for scratchpad memory
        static byte* scratchpad;

        // Allocated buffers for PSX binary and SotN binary (SLUS_000.67 and DRA.BIN)
        static byte* psx_bin;
        static byte* sotn_bin;
        static byte* map_data;

        // 24 KB CLUT storage
        static byte* clut_data;

        // Sizes of each binary
        static uint psx_bin_size;
        static uint sotn_bin_size;
        static uint map_data_size;

        // HI and LO registers (DIV, MULT, etc.)
        static uint hi;
        static uint lo;
};

#endif //SOTN_EDITOR_MIPS