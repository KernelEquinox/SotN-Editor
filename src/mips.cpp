#include <cstdio>
#include <cstdlib>
#include <memory.h>
#include "common.h"
#include "mips.h"
#include "entities.h"
#include "utils.h"
#include "gte.h"
#include "log.h"



// Variables
uint MipsEmulator::registers[32];
uint MipsEmulator::pc;
uint MipsEmulator::num_executed;
byte* MipsEmulator::ram;
byte* MipsEmulator::scratchpad;
byte* MipsEmulator::psx_bin;
byte* MipsEmulator::sotn_bin;
byte* MipsEmulator::map_data;
uint MipsEmulator::psx_bin_size;
uint MipsEmulator::sotn_bin_size;
uint MipsEmulator::map_data_size;
byte* MipsEmulator::clut_data;
uint MipsEmulator::hi;
uint MipsEmulator::lo;
GLuint MipsEmulator::framebuffer;
bool MipsEmulator::debug;
bool MipsEmulator::initialized;



/**
 * Loads the main PSX binary into MIPS RAM.
 *
 * @param filename: Filename of the PSX binary to load
 *
 * @note This file is usually named "SLUS000.67".
 *
 */
void MipsEmulator::SetPSXBinary(const char* filename) {

    // Open the file
    FILE* fp = fopen(filename, "rb");

    // Get the file size
    fseek(fp, 0, SEEK_END);
    psx_bin_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Create a buffer for the PSX binary
    psx_bin = (byte*)calloc(psx_bin_size, sizeof(byte));

    // Read the file
    fread(psx_bin, sizeof(byte), psx_bin_size, fp);
    fclose(fp);
}



/**
 * Loads the main SotN binary into MIPS RAM.
 *
 * @param filename: Filename of the SotN binary to load
 *
 * @note This file is usually named "DRA.BIN".
 *
 */
void MipsEmulator::SetSotNBinary(const char* filename) {

    // Open the file
    FILE* fp = fopen(filename, "rb");

    // Get the file size
    fseek(fp, 0, SEEK_END);
    sotn_bin_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Create a buffer for the SotN binary
    sotn_bin = (byte*)calloc(sotn_bin_size, sizeof(byte));

    // Read the file
    fread(sotn_bin, sizeof(byte), sotn_bin_size, fp);
    fclose(fp);
}



/**
 * Loads the specified map file into MIPS RAM.
 *
 * @param filename: Filename of the map binary to load
 *
 * @note These files are usually located in the "ST/" or "BOSS/" directories.
 *
 */
void MipsEmulator::LoadMapFile(const char* filename) {

    // Open the file
    FILE* fp = fopen(filename, "rb");

    // Get the file size
    fseek(fp, 0, SEEK_END);
    map_data_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Free the previous buffer if one was previously allocated
    if (initialized) {
        free(map_data);
    }

    // Create a buffer for the PSX binary
    map_data = (byte*)calloc(map_data_size, sizeof(byte));

    // Read the file
    fread(map_data, sizeof(byte), map_data_size, fp);
    fclose(fp);
}



/**
 * Stores the map's CLUT data in MIPS RAM.
 *
 * @param offset: Offset of the CLUT data
 * @param count: Number of CLUTs to store
 * @param data: CLUT data to store in MIPS RAM
 *
 */
void MipsEmulator::StoreMapCLUT(uint offset, uint count, byte* data) {

    // Copy the CLUT data to the dedicated 24 KB storage
    memcpy(clut_data + offset, data, count);

    // Copy the CLUT data to RAM
    memcpy(ram + CLUT_BASE_ADDR + offset, data, count);
}



/**
 * Initializes the MIPS emulator.
 *
 * @note This should only ever be called once.
 *
 */
void MipsEmulator::Initialize() {

    Log::Debug("--- MIPS INIT ---\n");

    // Initialize 2 MB of PSX memory
    ram = (byte*)calloc(0x200000, sizeof(byte));

    // Initialize 1 KB of scratchpad memory
    scratchpad = (byte*)calloc(1024, sizeof(byte));

    // Initialize 24 KB of CLUT memory
    clut_data = (byte*)calloc(CLUT_DATA_SIZE, sizeof(byte));

    // Set RA delimiter to flag when execution has returned from the initial function
    registers[RA] = FUNCTION_RETURN;
    registers[SP] = 0x001FFFC0;

    // Create initial framebuffer
    byte* tmp = (byte*)calloc(1024 * 512 * 4, sizeof(byte));
    framebuffer = Utils::CreateTexture(tmp, 1024, 512);
    free(tmp);

    // Initialize the GTE emulator
    GteEmulator::Initialize();

    // Flag as initialized
    initialized = true;
}



/**
 * Resets the MIPS emulator.
 */
void MipsEmulator::Reset() {

    Log::Debug("--- MIPS RESET ---\n");

    // Clear out RAM
    memset(ram, 0, 0x200000);

    // Zero out scratchpad
    memset(scratchpad, 0, 1024);

    // Copy PSX and SotN binaries to RAM
    memcpy(ram + PSX_RAM_OFFSET, psx_bin, psx_bin_size);
    memcpy(ram + SOTN_RAM_OFFSET, sotn_bin, sotn_bin_size);
    memcpy(ram + MAP_RAM_OFFSET, map_data, map_data_size);

    // Copy the CLUT data to RAM
    memcpy(ram + CLUT_BASE_ADDR, clut_data, CLUT_DATA_SIZE);

    // Set up SotN pointer tables
    memcpy(ram + SOTN_PTR_TBL_ADDR, ram + SOTN_RAM_OFFSET + 4, 0x50 * 4);

    // Copy first 16 pointers of the map file to the pointer table
    memcpy(ram + SOTN_PTR_TBL_ADDR, ram + MAP_RAM_OFFSET, 16 * 4);

    // Clear out the registers
    for (uint i = 0; i < 32; i++)
    {
        registers[i] = 0;
    }

    // Clear out HI and LO registers
    hi = 0;
    lo = 0;

    // Clear out PC and number of executed instructions
    pc = 0;
    num_executed = 0;

    // Set RA delimiter to flag when execution has returned from the initial function
    registers[RA] = FUNCTION_RETURN;
    registers[SP] = 0x001FFFC0;

    // Test
    MipsEmulator::WriteIntToRAM(0x00097408, 0x94);

    // Initialize the GTE emulator
    GteEmulator::Initialize();
}



/**
 * Frees allocated data when the emulator is destroyed.
 */
void MipsEmulator::Cleanup() {

    // Free all variables
    free(ram);
    free(scratchpad);
    free(clut_data);
    free(sotn_bin);
    free(psx_bin);
    free(map_data);
}



/**
 * Loads a file into MIPS RAM.
 *
 * @param filename: Filename of the file to load
 * @param addr: Address where the data should be stored in MIPS RAM
 *
 */
void MipsEmulator::LoadFile(const char* filename, uint addr) {

    // Open the file
    FILE* fp = fopen(filename, "rb");

    // Get the file size
    fseek(fp, 0, SEEK_END);
    uint num_bytes = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Read the file
    fread(ram + addr, sizeof(unsigned char), num_bytes, fp);
    fclose(fp);
}



/**
 * Reads a 32-bit value from MIPS RAM.
 *
 * @param addr: Address in MIPS RAM to read from
 *
 */
uint MipsEmulator::ReadIntFromRAM(uint addr) {

    return *(uint*)(ram + addr);
}



/**
 * Writes a 32-bit value to MIPS RAM.
 *
 * @param addr: Address in MIPS RAM to write to
 * @param value: Value to write to MIPS RAM
 *
 */
void MipsEmulator::WriteIntToRAM(uint addr, uint value) {

    *(uint*)(ram + addr) = value;
}



/**
 * Processes a MIPS opcode and calls the appropriate function.
 *
 * @param opcode: 32-bit instruction to execute
 *
 */
void MipsEmulator::ProcessOpcode(uint opcode)
{

    // Get the instruction bits
    uint instr_bits = (opcode >> 26) & 0x3F;

    // Check if this was an I-type or J-type instruction
    if (instr_bits > 0)
    {
        // Check if this was a coprocessor instruction
        if ((instr_bits & 0x30) == 0x10) {

            // Get the coprocessor instruction
            uint cop_instr = (opcode >> 21) & 0x1F;

            // Process non-command instructions
            if (cop_instr < 0x10) {

                uint src_index = (opcode >> 16) & 0x1F;
                uint dst_index = (opcode >> 11) & 0x1F;
                uint cop_num = cop_instr & 3;

                // Handle branch destinations
                if (cop_instr & 8) {
                    dst_index = (opcode & 0xFFFF);
                }

                // Convert instruction to function index
                uint fn_idx = cop_instr >> 1;
                ctype_funcs[fn_idx](cop_num, src_index, dst_index);
            }

            // Handle coprocessor command calls
            else {
                GteEmulator::ProcessOpcode(opcode);
            }
        }

        // Check if this was a J-type instruction
        else if (instr_bits == 2 || instr_bits == 3)
        {
            uint offset = opcode & 0x03FFFFFF;
            jtype_funcs[instr_bits](offset);
        }
        else
        {
            uint src_index = (opcode >> 21) & 0x1F;
            uint dst_index = (opcode >> 16) & 0x1F;
            short imm = (short)(opcode & 0xFFFF);
            // Check if this was BLTZ/BGEZ
            if (instr_bits == 1) {
                instr_bits += dst_index;
            }
            itype_funcs[instr_bits](src_index, dst_index, imm);
        }
    }

    // Otherwise this was an R-type instruction
    else
    {
        // Check if this was a NOP
        if (opcode == 0)
        {
            i_nop(0, 0, 0);
        }
        // Otherwise process as R-type
        else
        {
            uint src_index1 = (opcode >> 21) & 0x1F;
            uint src_index2 = (opcode >> 16) & 0x1F;
            uint dst_index = (opcode >> 11) & 0x1F;
            uint shamt = (opcode >> 6) & 0x1F;
            uint funct = opcode & 0x3F;
            rtype_funcs[funct](src_index1, src_index2, dst_index, shamt);
        }
    }

    // Bail if PC indicates that the function is returning from primary invocation
    if (pc == FUNCTION_RETURN) {
        return;
    }

    // Increment PC and execution counter
    num_executed += 1;
    pc += 4;
}



/**
 * Processes all opcodes within a MIPS function.
 *
 * @param func_addr: Address of the function in MIPS RAM
 *
 */
void MipsEmulator::ProcessFunction(uint func_addr)
{
    // Set PC to the function address
    int x = 0;
    pc = func_addr;
    Log::Debug("MipsEmulator::ProcessFunction(0x%08X)\n", func_addr);

    // Process function opcodes until RA indicates that the target function returning
    while (pc != FUNCTION_RETURN && x < MAX_EXECUTIONS) {

        // Grab the next opcode
        uint opcode = *(uint*)(ram + pc);

        // Just bail if the opcode is a dummy function
        if (opcode == 0xFFFEFFFE) {
            Log::Debug("Skipping function [dummy]\n");
            break;
        }

        // Execute the opcode
        ProcessOpcode(opcode);
        x += 1;
    }
}



/**
 * Copies data into RAM.
 *
 * @param addr: Address in MIPS RAM where data should be copied to
 * @param src: Data to be copied
 * @param count: Number of bytes to copy
 *
 */
void MipsEmulator::CopyToRAM(uint addr, const void* src, uint count) {

    // Copy the data to RAM
    memcpy(ram + addr, src, count);
}



/**
 * Copies data from RAM.
 *
 * @param addr: Address in MIPS RAM where data should be read from
 * @param dst: Buffer where data should be copied
 * @param count: Number of bytes to copy
 *
 */
void MipsEmulator::CopyFromRAM(uint addr, void* dst, uint count) {

    // Copy the data from RAM
    memcpy(dst, ram + addr, count);
}



/**
 * Processes all entities currently loaded in MIPS RAM.
 *
 * @return Vector of Entity objects.
 *
 */
std::vector<Entity> MipsEmulator::ProcessEntities() {

    // Initialize vector of entities
    std::vector<Entity> entities;

    // Loop through each entity in RAM
    for (int i = 0; i < 0xC0; i++) {

        // Get the current entity's location in RAM
        uint cur_offset = ENTITY_ALLOCATION_START + (i * sizeof(EntityData));

        // Create entity data structure from existent RAM data
        EntityData* entity_data = (EntityData*)(ram + cur_offset);
        // Copy entity data to Alucard data
        memcpy(ram + ALUCARD_ENTITY_ADDR, ram + cur_offset, sizeof(EntityData));

        // Check if update function exists
        if (entity_data->update_function > 0x80180000 && entity_data->update_function < RAM_MAX_OFFSET) {

            for (int k = 0; k < 2; k++) {

                // Initialize the emulator's registers
                registers[A0] = cur_offset;
                registers[S0] = cur_offset + 0x48;
                registers[S1] = cur_offset;
                WriteIntToRAM(0x0006C3B8, cur_offset);

                // Clear out PC and number of executed instructions
                pc = 0;
                num_executed = 0;

                // Set RA delimiter to flag when execution has returned from the initial function
                registers[RA] = FUNCTION_RETURN;
                registers[SP] = 0x001FFFC0;

                // Test
                WriteIntToRAM(0x00097408, 0x94);

                // Process the entity's update function
                if (entity_data->update_function != 0) {
                    ProcessFunction(entity_data->update_function - RAM_BASE_OFFSET);
                }
            }
        }
    }

    // Loop through each entity in RAM after processing all entities
    for (int i = 0; i < 0xC0; i++) {

        // Get the current entity's location in RAM
        uint cur_offset = ENTITY_ALLOCATION_START + (i * sizeof(EntityData));

        // Check if any bytes were written
        bool data_exists = false;
        for (int k = 0; k < 0xB0; k++) {
            byte cur_data = *(byte*)(ram + cur_offset + k);
            if (cur_data > 0) {
                data_exists = true;
                break;
            }
        }

        // Skip if no bytes were written
        if (!data_exists) {
            continue;
        }

        // Create entity data structure from existent RAM data
        EntityData* entity_data = (EntityData*)(ram + cur_offset);

        // Create a new entity
        Entity entity;

        // Update entity data
        entity.slot = entity_data->room_slot;
        entity.data = *entity_data;
        entity.address = cur_offset;

        // Populate entity data map
        entity.PopulateDataMap();

        // Add the entity to the list
        entities.push_back(entity);
    }

    // Return the entity data
    return entities;
}






// -- Framebuffer Operations -------------------------------------------------------------------------------------

/**
 * Loads data from MIPS RAM into the framebuffer.
 *
 * @param rect: Rectangle in the framebuffer where data should be written
 * @param src: Data to load into the framebuffer
 *
 */
void MipsEmulator::LoadImage(RECT* rect, byte* src) {

    byte* rgba_pixels = Utils::Indexed_to_RGBA(src, rect->w * rect->h);
    Utils::SetPixels(framebuffer, rect->x, rect->y, rect->w, rect->h, rgba_pixels);
    free(rgba_pixels);
}

/**
 * Stores data from the framebuffer into MIPS RAM.
 *
 * @param rect: Rectangle in the framebuffer where data should be read from
 * @param dst: Buffer where data should be stored
 *
 */
void MipsEmulator::StoreImage(RECT* rect, byte* dst) {

    byte* rgba_pixels = Utils::GetPixels(framebuffer, rect->x, rect->y, rect->w, rect->h);
    byte* indexed_pixels = Utils::RGBA_to_Indexed(rgba_pixels, rect->w * rect->h);
    memcpy(dst, indexed_pixels, rect->w * rect->h * 2);
    free(rgba_pixels);
    free(indexed_pixels);
}

/**
 * Copies data from one part of the framebuffer to another part of the framebuffer.
 *
 * @param rect: Rectangle in the framebuffer where data should be read from
 * @param x: X coordinate where the data should be copied
 * @param y: Y coordinate where the data should be copied
 *
 */
void MipsEmulator::MoveImage(RECT* rect, int x, int y) {

    byte* rgba_pixels = Utils::GetPixels(framebuffer, rect->x, rect->y, rect->w, rect->h);
    Utils::SetPixels(framebuffer, x, y, rect->w, rect->h, rgba_pixels);
    free(rgba_pixels);
}

/**
 * Clears a portion of the framebuffer with the specified color.
 *
 * @param rect: Rectangle in the framebuffer that should be cleared
 * @param r: Red component of the color
 * @param g: Green component of the color
 * @param b: Blue component of the color
 *
 */
void MipsEmulator::ClearImage(RECT* rect, byte r, byte g, byte b) {

    byte* clear_pixels = (byte*)calloc(rect->w * rect->h * 4, sizeof(byte));

    // Set all RGBA values within the buffer
    for (int i = 0; i < rect->w * rect->h * 4; i += 4) {
        clear_pixels[i] = r;
        clear_pixels[i+1] = g;
        clear_pixels[i+2] = b;
        clear_pixels[i+3] = 0xFF;
    }

    Utils::SetPixels(framebuffer, rect->x, rect->y, rect->w, rect->h, clear_pixels);
    free(clear_pixels);
}






// -- I-Type Instructions ----------------------------------------------------------------------------------------

void MipsEmulator::i_nop(uint src_index, uint dst_index, short imm) {
    Log::Debug(
        "PC: %08X  % 6d    NOP\n",
        pc + RAM_BASE_OFFSET,
        num_executed
    );
}

void MipsEmulator::i_bltz(uint src_index, uint dst_index, short imm) {
    int src = (int)registers[src_index];
    uint new_pos = (uint)(pc + (imm * 4));
    if (src < 0) {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BLTZ :: (taken) :: %s (%08X) < 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );

        // Execute the next instruction before making the jump because MIPS
        pc += 4;
        num_executed++;
        uint opcode = *(uint*)(ram + pc);
        ProcessOpcode(opcode);
        num_executed--;

        pc = new_pos;
    }
    else {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BLTZ :: (not taken) :: %s (%08X) < 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );
    }
}

void MipsEmulator::i_bgez(uint src_index, uint dst_index, short imm) {
    int src = (int)registers[src_index];
    uint new_pos = (uint)(pc + (imm * 4));
    if (src >= 0) {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BGEZ :: (taken) :: %s (%08X) >= 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );

        // Execute the next instruction before making the jump because MIPS
        pc += 4;
        num_executed++;
        uint opcode = *(uint*)(ram + pc);
        ProcessOpcode(opcode);
        num_executed--;

        pc = new_pos;
    }
    else {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BGEZ :: (not taken) :: %s (%08X) >= 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );
    }
}

void MipsEmulator::i_beq(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    uint new_pos = pc + (imm * 4);
    if (src == registers[dst_index]) {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BEQ :: (taken) :: %s (%08X) == %s (%08X) :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            register_names[dst_index],
            registers[dst_index],
            new_pos + RAM_BASE_OFFSET
        );

        // Execute the next instruction before making the jump because MIPS
        pc += 4;
        num_executed++;
        uint opcode = *(uint*)(ram + pc);
        ProcessOpcode(opcode);
        num_executed--;

        pc = new_pos;
    }
    else {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BEQ :: (not taken) :: %s (%08X) == %s (%08X) :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            register_names[dst_index],
            registers[dst_index],
            new_pos + RAM_BASE_OFFSET
        );
    }
}

void MipsEmulator::i_bne(uint src_index, uint dst_index, short imm) {
    int src = registers[src_index];
    uint new_pos = (uint)(pc + (imm * 4));
    if (src != registers[dst_index]) {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BNE :: (taken) :: %s (%08X) != %s (%08X) :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            register_names[dst_index],
            registers[dst_index],
            new_pos + RAM_BASE_OFFSET
        );

        // Execute the next instruction before making the jump because MIPS
        pc += 4;
        num_executed++;
        uint opcode = *(uint*)(ram + pc);
        ProcessOpcode(opcode);
        num_executed--;

        pc = new_pos;
    }
    else {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BNE :: (not taken) :: %s (%08X) != %s (%08X) :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            register_names[dst_index],
            registers[dst_index],
            new_pos + RAM_BASE_OFFSET
        );
    }
}

void MipsEmulator::i_blez(uint src_index, uint dst_index, short imm) {
    int src = (int)registers[src_index];
    uint new_pos = (uint)(pc + (imm * 4));
    if (src <= 0) {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BLEZ :: (taken) :: %s (%08X) <= 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );

        // Execute the next instruction before making the jump because MIPS
        pc += 4;
        num_executed++;
        uint opcode = *(uint*)(ram + pc);
        ProcessOpcode(opcode);
        num_executed--;

        pc = new_pos;
    }
    else {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BLEZ :: (not taken) :: %s (%08X) <= 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );
    }
}

void MipsEmulator::i_bgtz(uint src_index, uint dst_index, short imm) {
    int src = (int)registers[src_index];
    uint new_pos = (uint)(pc + (imm * 4));
    if (src > 0) {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BGTZ :: (taken) :: %s (%08X) > 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );

        // Execute the next instruction before making the jump because MIPS
        pc += 4;
        num_executed++;
        uint opcode = *(uint*)(ram + pc);
        ProcessOpcode(opcode);
        num_executed--;

        pc = new_pos;
    }
    else {
        Log::Debug(
            "PC: %08X  % 6d    "
            "BGTZ :: (taken) :: %s (%08X) > 0 :: PC=%08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            new_pos + RAM_BASE_OFFSET
        );
    }
}

void MipsEmulator::i_addi(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    registers[dst_index] = src + imm;
    Log::Debug(
        "PC: %08X  % 6d    "
        "ADDI :: %s = %s (%08X) + %04X"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm, register_names[dst_index], registers[dst_index]
    );

}

void MipsEmulator::i_addiu(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    registers[dst_index] = (uint)(src + imm);
    Log::Debug(
        "PC: %08X  % 6d    "
        "ADDIU :: %s = %s (%08X) + %04X"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_slti(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    if (src < imm) {
        registers[dst_index] = 1;
    }
    else {
        registers[dst_index] = 0;
    }
    Log::Debug(
        "PC: %08X  % 6d    "
        "SLTI :: %s = %s (%08X) < %04X ? 1 : 0"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_sltiu(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    if (src < (ushort)imm) {
        registers[dst_index] = 1;
    }
    else {
        registers[dst_index] = 0;
    }
    Log::Debug(
        "PC: %08X  % 6d    "
        "SLTIU :: %s = %s (%08X) < %04X ? 1 : 0"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_andi(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    registers[dst_index] = src & (ushort)imm;
    Log::Debug(
        "PC: %08X  % 6d    "
        "ANDI :: %s = %s (%08X) & %04X"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_ori(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    registers[dst_index] = (uint)(src | (ushort)imm);
    Log::Debug(
        "PC: %08X  % 6d    "
        "ORI :: %s = %s (%08X) | %04hX"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_xori(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];
    registers[dst_index] = (uint)(src ^ (ushort)imm);
    Log::Debug(
        "PC: %08X  % 6d    "
        "XORI :: %s = %s (%08X) ^ %04hX"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index],
        src,
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lui(uint src_index, uint dst_index, short imm) {
    registers[dst_index] = (uint)(imm << 16);
    Log::Debug(
        "PC: %08X  % 6d    "
        "LUI :: %s = %04X << 16"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        imm,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lb(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load byte from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = (int)*(char*)(scratchpad + src + imm);
    }
    // Fetch from RAM
    else {
        registers[dst_index] = (int)*(char*)(ram + src + imm);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load byte from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lh(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load short from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = (int)*(short*)(scratchpad + src + imm);
    }
    // Fetch from RAM
    else {
        registers[dst_index] = (int)*(short*)(ram + src + imm);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load short from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lwl(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load word (LEFT) from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc. (adjusted so that the target address is the lowest order byte)
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = *(uint*)(scratchpad + src + imm - 3);
    }
    // Fetch from RAM (adjusted so that the target address is the lowest order byte)
    else {
        registers[dst_index] = *(uint*)(ram + src + imm - 3);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load word (LEFT) from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lw(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load word from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = *(uint*)(scratchpad + src + imm);
    }
    // Fetch from RAM
    else {
        registers[dst_index] = *(uint*)(ram + src + imm);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load word from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lbu(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load byte (unsigned) from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = (int)*(byte*)(scratchpad + src + imm);
    }
    // Fetch from RAM
    else {
        registers[dst_index] = (int)*(byte*)(ram + src + imm);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load byte (unsigned) from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lhu(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load short (unsigned) from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0xFFFF0000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = (int)*(ushort*)(scratchpad + src + imm);
    }
    // Fetch from RAM
    else {
        registers[dst_index] = (int)*(ushort*)(ram + src + imm);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load short (unsigned) from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_lwr(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Load word (RIGHT) from %s (%08X) + %04X (=%08X) into %s"
            "\n                                   * %s = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            src,
            imm,
            src + imm,
            register_names[dst_index],
            register_names[dst_index],
            registers[dst_index]
        );

        return;
    }

    // Fetch from scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        registers[dst_index] = *(uint*)(scratchpad + src + imm);
    }
    // Fetch from RAM
    else {
        registers[dst_index] = *(uint*)(ram + src + imm);
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Load word (RIGHT) from %s (%08X) + %04X (=%08X) into %s"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        src,
        imm,
        src + imm,
        register_names[dst_index],
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::i_sb(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if store address is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to write to out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Store byte %s (%02X) at %s %08X + %04X (=%08X)"
            "\n                                   [%08X] = %02X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[dst_index],
            registers[dst_index],
            register_names[src_index],
            src,
            imm,
            src + imm,
            src + imm + RAM_BASE_OFFSET,
            registers[dst_index]
        );

        return;
    }

    // Store in scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        *(byte*)(scratchpad + src + imm) = registers[dst_index];
    }

    // Store in RAM
    else {
        *(byte*)(ram + src + imm) = registers[dst_index];
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Store byte %s (%02X) at %s %08X + %04X (=%08X)"
        "\n                                   [%08X] = %02X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        registers[dst_index],
        register_names[src_index],
        src,
        imm,
        src + imm,
        src + imm + RAM_BASE_OFFSET,
        registers[dst_index]
    );

}

void MipsEmulator::i_sh(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if store address is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to write to out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Store short %s (%04X) at %s %08X + %04X (=%08X)"
            "\n                                   [%08X] = %04X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[dst_index],
            registers[dst_index],
            register_names[src_index],
            src,
            imm,
            src + imm,
            src + imm + RAM_BASE_OFFSET,
            registers[dst_index]
        );

        return;
    }

    // Store in scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        *(short*)(scratchpad + src + imm) = registers[dst_index];
    }
    // Store in RAM
    else {
        *(short*)(ram + src + imm) = registers[dst_index];
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Store short %s (%04X) at %s %08X + %04X (=%08X)"
        "\n                                   [%08X] = %04X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        registers[dst_index],
        register_names[src_index],
        src,
        imm,
        src + imm,
        src + imm + RAM_BASE_OFFSET,
        registers[dst_index]
    );

}

void MipsEmulator::i_swl(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if store address is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to write to out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Store word (LEFT) %s (%08X) at %s %08X + %04X (=%08X)"
            "\n                                   [%08X] = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[dst_index],
            registers[dst_index],
            register_names[src_index],
            src,
            imm,
            src + imm,
            src + imm + RAM_BASE_OFFSET,
            registers[dst_index]
        );

        return;
    }

    // Store in scratchpad memory or MCTRL1, IO ports, etc. (adjusted so that the target address is the lowest order byte)
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        *(uint*)(scratchpad + src + imm - 3) = registers[dst_index];
    }
    // Store in RAM (adjusted so that the target address is the lowest order byte)
    else {
        *(uint*)(ram + src + imm - 3) = registers[dst_index];
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Store word (LEFT) %s (%08X) at %s %08X + %04X (=%08X)"
        "\n                                   [%08X] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        registers[dst_index],
        register_names[src_index],
        src,
        imm,
        src + imm,
        src + imm + RAM_BASE_OFFSET,
        registers[dst_index]
    );

}

void MipsEmulator::i_sw(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if store address is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to write to out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Store word %s (%08X) at %s %08X + %04X (=%08X)"
            "\n                                   [%08X] = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[dst_index],
            registers[dst_index],
            register_names[src_index],
            src,
            imm,
            src + imm,
            src + imm + RAM_BASE_OFFSET,
            registers[dst_index]
        );

        return;
    }

    // Store in scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        *(uint*)(scratchpad + src + imm) = registers[dst_index];
    }
    // Store in RAM
    else {
        *(uint*)(ram + src + imm) = registers[dst_index];
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Store word %s (%08X) at %s %08X + %04X (=%08X)"
        "\n                                   [%08X] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        registers[dst_index],
        register_names[src_index],
        src,
        imm,
        src + imm,
        src + imm + RAM_BASE_OFFSET,
        registers[dst_index]
    );

}

void MipsEmulator::i_swr(uint src_index, uint dst_index, short imm) {
    uint src = registers[src_index];

    // Check if store address is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        Log::Error("Tried to write to out of bounds address.\n"
            "PC: %08X  % 6d    "
            "Store word (right) %s (%08X) at %s %08X + %04X (=%08X)"
            "\n                                   [%08X] = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[dst_index],
            registers[dst_index],
            register_names[src_index],
            src,
            imm,
            src + imm,
            src + imm + RAM_BASE_OFFSET,
            registers[dst_index]
        );

        return;
    }

    // Store in scratchpad memory or MCTRL1, IO ports, etc.
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        *(uint*)(scratchpad + src + imm) = registers[dst_index];
    }
    // Store in RAM
    else {
        *(uint*)(ram + src + imm) = registers[dst_index];
    }

    Log::Debug(
        "PC: %08X  % 6d    "
        "Store word (right) %s (%08X) at %s %08X + %04X (=%08X)"
        "\n                                   [%08X] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        registers[dst_index],
        register_names[src_index],
        src,
        imm,
        src + imm,
        src + imm + RAM_BASE_OFFSET,
        registers[dst_index]
    );

}

/*
void MipsEmulator::i_ll(uint src_index, uint dst_index, short imm) {

}

void MipsEmulator::i_lwc1(uint src_index, uint dst_index, short imm) {

}
*/

void MipsEmulator::i_lwc2(uint src_index, uint dst_index, short imm) {
    // Load value from RAM
    uint src = registers[src_index];

    // Set base memory address
    byte* mem_base = ram;

    // Check if address to load is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        uint val = *(uint*)(mem_base + src + imm);
        Log::Error("Tried to read out of bounds address.\n"
            "PC: %08X  % 6d    "
            "LWC2 :: Cop2_Data[%d] = [%s+%04X] ([%08X] -> %08X)"
            "\n                                   * Cop2_Data[%d] = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            dst_index,
            register_names[src_index],
            imm,
            src + imm,
            val,
            dst_index,
            val
        );

        return;
    }

    // Check if address is in the scratchpad area
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        mem_base = scratchpad;
    }

    uint val = *(uint*)(mem_base + src + imm);
    // Write value to COP2 data register
    GteEmulator::WriteDataRegister(dst_index, val);
    Log::Debug(
        "PC: %08X  % 6d    "
        "LWC2 :: Cop2_Data[%d] = [%s+%04X] ([%08X] -> %08X)"
        "\n                                   * Cop2_Data[%d] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        dst_index,
        register_names[src_index],
        imm,
        src + imm,
        val,
        dst_index,
        val
    );

}

/*
void MipsEmulator::i_lwc3(uint src_index, uint dst_index, short imm) {

}

void MipsEmulator::i_sc(uint src_index, uint dst_index, short imm) {

}

void MipsEmulator::i_swc1(uint src_index, uint dst_index, short imm) {

}
*/

void MipsEmulator::i_swc2(uint src_index, uint dst_index, short imm) {
    // Print initial value from RAM
    uint src = registers[src_index];

    // Set base memory address
    byte* mem_base = ram;

    // Check if store address is in addressable memory space
    if (src >= RAM_BASE_OFFSET && src <= RAM_MAX_OFFSET) {
        src -= RAM_BASE_OFFSET;
    } else if (RAM_BASE_OFFSET + src + imm > RAM_MAX_OFFSET
            && (src & 0x1F800000) != 0x1F800000) {
        byte* ram_offset = mem_base + src + imm;
        uint val = *(uint*)(ram_offset);
        Log::Error("Tried to write to out of bounds address.\n"
            "PC: %08X  % 6d    "
            "SWC2 :: [%s+%04X] ([%08X] -> %08X) = Cop2_Data[%d]"
            "\n                                   * [%08X] = %08X\n",
            pc + RAM_BASE_OFFSET,
            num_executed,
            register_names[src_index],
            imm,
            registers[src_index] + imm,
            val,
            dst_index,
            registers[src_index] + imm,
            *(uint*)(ram_offset)
        );

        return;
    }

    // Check if address is in the scratchpad area
    if ((src & 0x1F800000) == 0x1F800000) {
        src -= 0x1F800000;
        mem_base = scratchpad;
    }

    byte* ram_offset = mem_base + src + imm;
    uint val = *(uint*)(ram_offset);
    // Read value from COP2 data register into RAM
    *(uint*)(ram_offset) = GteEmulator::ReadDataRegister(dst_index);
    Log::Debug(
        "PC: %08X  % 6d    "
        "SWC2 :: [%s+%04X] ([%08X] -> %08X) = Cop2_Data[%d]"
        "\n                                   * [%08X] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        imm,
        registers[src_index] + imm,
        val,
        dst_index,
        registers[src_index] + imm,
        *(uint*)(ram_offset)
    );

}

/*
void MipsEmulator::i_swc3(uint src_index, uint dst_index, short imm) {

}
 */





// -- R-Type Instructions ----------------------------------------------------------------------------------------

void MipsEmulator::r_nop(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    Log::Debug("NOP\n");
}

void MipsEmulator::r_sll(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)(src2 << shamt);
    Log::Debug(
        "PC: %08X  % 6d    "
        "SLL :: %s = %s (%08X) << %d"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index2],
        src2,
        shamt,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_srl(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)(src2 >> shamt);
    Log::Debug(
        "PC: %08X  % 6d    "
        "SRL :: %s = %s (%08X) >> %d"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index2],
        src2,
        shamt,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_sra(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)(src2 >> shamt);
    // Check if result should be sign extended
    if (((src2 >> 31) & 1) == 1) {
        registers[dst_index] |= 0xFFFFFFFF ^ (0xFFFFFFFF >> shamt);
    }
    Log::Debug(
        "PC: %08X  % 6d    "
        "SRA :: %s = %s (%08X) >> %d"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index2],
        src2,
        shamt,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_sllv(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)(src2 << src1);
    Log::Debug(
        "PC: %08X  % 6d    "
        "SLLV :: %s = %s (%08X) << %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index2],
        src2,
        register_names[src_index1],
        src1,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_srlv(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)(src2 >> src1);
    Log::Debug(
        "PC: %08X  % 6d    "
        "SRLV :: %s = %s (%08X) >> %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index2],
        src2,
        register_names[src_index1],
        src1,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_srav(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)(src2 >> src1);
    // Check if result should be sign extended
    if (((src2 >> 31) & 1) == 1) {
        registers[dst_index] |= 0xFFFFFFFF ^ (0xFFFFFFFF >> (int)src1);
    }
    Log::Debug(
        "PC: %08X  % 6d    "
        "SRAV :: %s = %s (%08X) >> %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index2],
        src2,
        register_names[src_index1],
        src1,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_jr(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint dest = src1 - RAM_BASE_OFFSET;
    Log::Debug(
    "PC: %08X  % 6d    -> JR %s (%08X)\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        dest
    );

    // Execute next opcode
    num_executed++;
    uint opcode = *(uint*)(ram + pc + 4);
    ProcessOpcode(opcode);
    num_executed--;

    // Check if jump location is within executable bounds
    if (dest >= 0x00010000 && dest < 0x00200000)
        pc = dest - 4;
    // Otherwise return to the calling function
    else
        pc = registers[RA];
}

void MipsEmulator::r_jalr(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];

    uint func_start = src1 - RAM_BASE_OFFSET;
    Log::Debug(
        "PC: %08X  % 6d    >>>>> JALR %s %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        func_start + RAM_BASE_OFFSET
    );

    // Execute next opcode
    num_executed++;
    uint opcode = *(uint*)(ram + pc + 4);
    ProcessOpcode(opcode);
    num_executed--;

    // Check for dedicated image processing functions
    if (func_start == LOAD_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        byte* src = (byte*)(ram + (registers[A1] - RAM_BASE_OFFSET));
        LoadImage(rect, src);
        return;
    }
    else if (func_start == STORE_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        byte* dst = (byte*)(ram + registers[A1]);
        StoreImage(rect, dst);
        return;
    }
    else if (func_start == MOVE_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        int x = *(int*)(ram + registers[A1]);
        int y = *(int*)(ram + registers[A2]);
        MoveImage(rect, x, y);
        return;
    }
    else if (func_start == CLEAR_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        byte r = *(byte*)(ram + registers[A1]);
        byte g = *(byte*)(ram + registers[A2]);
        byte b = *(byte*)(ram + registers[A3]);
        ClearImage(rect, r, g, b);
        return;
    }

    // Check if jump location is within executable bounds
    if (func_start >= 0x00010000 && func_start < 0x00200000) {
        // Save PC in RA and set PC to function address
        registers[RA] = pc;
        pc = func_start - 4;
    }
}

void MipsEmulator::r_mfhi(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    registers[dst_index] = hi;
    Log::Debug(
        "PC: %08X  % 6d    "
        "MFHI :: %s = hi (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        hi,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_mthi(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    hi = src1;
    Log::Debug(
        "PC: %08X  % 6d    "
        "MTHI :: hi = %s (%08X)"
        "\n                                   hi = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        src1,
        hi
    );

}

void MipsEmulator::r_mflo(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    registers[dst_index] = lo;
    Log::Debug(
        "PC: %08X  % 6d    "
        "MFLO :: %s = lo (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        lo,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_mtlo(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    lo = src1;
    Log::Debug(
        "PC: %08X  % 6d    "
        "MTLO :: lo = %s (%08X)"
        "\n                                   lo = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        src1,
        lo
    );

}

void MipsEmulator::r_mult(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    long result = (int)src1 * (int)src2;
    hi = (uint)(result >> 32) & 0xFFFFFFFF;
    lo = (uint)(result & 0xFFFFFFFF);
    Log::Debug(
        "PC: %08X  % 6d    "
        "MULT :: %s (%08X) * %s (%08X)"
        "\n                                   hi = %08X, lo = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        hi,
        lo
    );

}

void MipsEmulator::r_multu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    unsigned long result = src1 * src2;
    hi = (uint)(result >> 32) & 0xFFFFFFFF;
    lo = (uint)(result & 0xFFFFFFFF);
    Log::Debug(
        "PC: %08X  % 6d    "
        "MULTU :: %s (%08X) * %s (%08X)"
        "\n                                   hi = %08X, lo = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        hi,
        lo
    );

}

void MipsEmulator::r_div(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    lo = (uint)((int)src1 / (int)src2);
    hi = (uint)((int)src1 % (int)src2);
    Log::Debug(
        "PC: %08X  % 6d    "
        "DIV :: %s (%08X) / %s (%08X)"
        "\n                                   hi = %08X, lo = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        hi,
        lo
    );

}

void MipsEmulator::r_divu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    lo = src1 / src2;
    hi = src1 % src2;
    Log::Debug(
        "PC: %08X  % 6d    "
        "DIVU :: %s (%08X) / %s (%08X)"
        "\n                                   hi = %08X, lo = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        hi,
        lo
    );

}

void MipsEmulator::r_add(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)((int)src1 + (int)src2);
    Log::Debug(
        "PC: %08X  % 6d    "
        "ADD :: %s = %s (%08X) + %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_addu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = src1 + src2;
    Log::Debug(
        "PC: %08X  % 6d    "
        "ADDU :: %s = %s (%08X) + %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_sub(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = (uint)((int)src1 - (int)src2);
    Log::Debug(
        "PC: %08X  % 6d    "
        "SUB :: %s = %s (%08X) - %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_subu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = src1 - src2;
    Log::Debug(
        "PC: %08X  % 6d    "
        "SUBU :: %s = %s (%08X) - %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_and(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = src1 & src2;
    Log::Debug(
        "PC: %08X  % 6d    "
        "AND :: %s = %s (%08X) & %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_or(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = src1 | src2;
    Log::Debug(
        "PC: %08X  % 6d    "
        "OR :: %s = %s (%08X) | %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_xor(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = src1 ^ src2;
    Log::Debug(
        "PC: %08X  % 6d    "
        "XOR :: %s = %s (%08X) ^ %s (%08X)"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_nor(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    registers[dst_index] = ~(src1 | src2);
    Log::Debug(
        "PC: %08X  % 6d    "
        "NOR :: %s = ~ <%s (%08X) | %s (%08X)>"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );
}

void MipsEmulator::r_slt(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    if ((int)src1 < (int)src2) {
        registers[dst_index] = 1;
    }
    else {
        registers[dst_index] = 0;
    }
    Log::Debug(
        "PC: %08X  % 6d    "
        "SLT :: %s = %s (%08X) < %s (%08X) ? 1 : 0"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

void MipsEmulator::r_sltu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {
    uint src1 = registers[src_index1];
    uint src2 = registers[src_index2];
    if (src1 < src2) {
        registers[dst_index] = 1;
    }
    else {
        registers[dst_index] = 0;
    }
    Log::Debug(
        "PC: %08X  % 6d    "
        "SLTU :: %s = %s (%08X) < %s (%08X) ? 1 : 0"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[dst_index],
        register_names[src_index1],
        src1,
        register_names[src_index2],
        src2,
        register_names[dst_index],
        registers[dst_index]
    );

}

/*
void MipsEmulator::r_tge(uint src_index1, uint src_index2, uint dst_index, uint shamt) {

}

void MipsEmulator::r_tgeu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {

}

void MipsEmulator::r_tlt(uint src_index1, uint src_index2, uint dst_index, uint shamt) {

}

void MipsEmulator::r_tltu(uint src_index1, uint src_index2, uint dst_index, uint shamt) {

}

void MipsEmulator::r_teq(uint src_index1, uint src_index2, uint dst_index, uint shamt) {

}

void MipsEmulator::r_tne(uint src_index1, uint src_index2, uint dst_index, uint shamt) {

}
*/





// -- J-Type Instructions ----------------------------------------------------------------------------------------

void MipsEmulator::j_nop(uint offset) {
    Log::Debug(
        "PC: %08X  % 6d    NOP\n",
        pc + RAM_BASE_OFFSET,
        num_executed
    );
}

void MipsEmulator::j_j(uint offset) {
    uint func_start = (offset * 4);
    Log::Debug(
        "PC: %08X  % 6d    -> J %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        func_start
    );

    // Execute the next instruction before making the jump because MIPS
    num_executed++;
    uint opcode = *(uint*)(ram + pc + 4);
    ProcessOpcode(opcode);
    num_executed--;

    // Check if jump location is within executable bounds
    if (func_start >= 0x00010000 && func_start < 0x00200000) {
        pc = func_start - 4;
    }
}

void MipsEmulator::j_jal(uint offset) {
    uint func_start = (offset * 4);

    Log::Debug(
        "PC: %08X  % 6d    >>>>> JAL %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        func_start + RAM_BASE_OFFSET
    );

    // Execute the next instruction before making the jump because MIPS
    num_executed++;
    uint opcode = *(uint*)(ram + pc + 4);
    ProcessOpcode(opcode);
    num_executed--;

    // Check for dedicated image processing functions
    if (func_start == LOAD_IMAGE_ADDR) {
        uint reg_A0 = registers[A0];
        uint reg_A1 = registers[A1];

        // Check if address is in the scratchpad area
        if ((reg_A1 & 0x1F800000) == 0x1F800000) {
            reg_A1 -= 0x1F800000;
        }

        // Check if address is in RAM
        if (reg_A1 >= RAM_BASE_OFFSET && reg_A1 < RAM_MAX_OFFSET) {
            reg_A1 -= RAM_BASE_OFFSET;
        }
        RECT* rect = (RECT*)(ram + registers[A0]);
        byte* src = (byte*)(ram + reg_A1);
        LoadImage(rect, src);
        return;
    }
    else if (func_start == STORE_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        byte* dst = (byte*)(ram + registers[A1]);
        StoreImage(rect, dst);
        return;
    }
    else if (func_start == MOVE_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        int x = *(int*)(ram + registers[A1]);
        int y = *(int*)(ram + registers[A2]);
        MoveImage(rect, x, y);
        return;
    }
    else if (func_start == CLEAR_IMAGE_ADDR) {
        RECT* rect = (RECT*)(ram + registers[A0]);
        byte r = *(byte*)(ram + registers[A1]);
        byte g = *(byte*)(ram + registers[A2]);
        byte b = *(byte*)(ram + registers[A3]);
        ClearImage(rect, r, g, b);
        return;
    }


    // Check if jump location is within executable bounds
    if (func_start >= 0x00010000 && func_start < 0x00200000) {
        registers[31] = pc;
        pc = func_start - 4;
    }
}






// -- Coprocessor Instructions -----------------------------------------------------------------------------------

void MipsEmulator::c_mfc(uint cop_num, uint src_index, uint dst_index) {
    registers[src_index] = GteEmulator::ReadDataRegister(dst_index);
    Log::Debug(
        "PC: %08X  % 6d    "
        "MFC :: %s = Cop%d_Data[%d]"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        cop_num,
        dst_index,
        register_names[src_index],
        registers[src_index]
    );

}

void MipsEmulator::c_cfc(uint cop_num, uint src_index, uint dst_index) {
    registers[src_index] = GteEmulator::ReadControlRegister(dst_index);
    Log::Debug(
        "PC: %08X  % 6d    "
        "CFC :: %s = Cop%d_Control[%d]"
        "\n                                   * %s = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        register_names[src_index],
        cop_num,
        dst_index,
        register_names[src_index],
        registers[src_index]
    );

}

void MipsEmulator::c_mtc(uint cop_num, uint src_index, uint dst_index) {
    uint src = registers[src_index];
    GteEmulator::WriteDataRegister(dst_index, src);
    Log::Debug(
        "PC: %08X  % 6d    "
        "MTC :: Cop%d_Data[%d] = %s"
        "\n                                   * Cop%d_Data[%d] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        cop_num,
        dst_index,
        register_names[src_index],
        cop_num,
        dst_index,
        registers[src_index]
    );

}

void MipsEmulator::c_ctc(uint cop_num, uint src_index, uint dst_index) {
    uint src = registers[src_index];
    GteEmulator::WriteControlRegister(dst_index, src);
    Log::Debug(
        "PC: %08X  % 6d    "
        "CTC :: Cop%d_Control[%d] = %s"
        "\n                                   * Cop%d_Control[%d] = %08X\n",
        pc + RAM_BASE_OFFSET,
        num_executed,
        cop_num,
        dst_index,
        register_names[src_index],
        cop_num,
        dst_index,
        registers[src_index]
    );

}

void MipsEmulator::c_bcx(uint cop_num, uint cond, uint dest) {
    uint new_pos = (uint)(pc + (dest * 4));
    if (cond == 0) {
        Log::Debug("BC%dF :: <Not Implemented> :: Never jump to %08X\n", cop_num, new_pos + RAM_BASE_OFFSET);
    }
    else {
        Log::Debug("BC%dT :: <Not Implemented> :: Always jump to %08X\n", cop_num, new_pos + RAM_BASE_OFFSET);
    }
}
