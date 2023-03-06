#ifndef SOTN_EDITOR_MIPS
#define SOTN_EDITOR_MIPS
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "entities.h"
#include "common.h"

const char* const A0_FUNCS[192] = {
    "FileOpen(filename,accessmode)",
    "FileSeek(fd,offset,seektype)",
    "FileRead(fd,dst,length)",
    "FileWrite(fd,src,length)",
    "FileClose(fd)",
    "FileIoctl(fd,cmd,arg)",
    "exit(exitcode)",
    "FileGetDeviceFlag(fd)",
    "FileGetc(fd)",
    "FilePutc(char,fd)",
    "todigit(char)",
    "!! atof(src)",
    "strtoul(src,src_end,base)",
    "strtol(src,src_end,base)",
    "abs(val)",
    "labs(val)",
    "atoi(src)",
    "atol(src)",
    "atob(src,num_dst)",
    "SaveState(buf)",
    "RestoreState(buf,param)",
    "strcat(dst,src)",
    "strncat(dst,src,maxlen)",
    "strcmp(str1,str2)",
    "strncmp(str1,str2,maxlen)",
    "strcpy(dst,src)",
    "strncpy(dst,src,maxlen)",
    "strlen(src)",
    "index(src,char)",
    "rindex(src,char)",
    "strchr(src,char)",
    "strrchr(src,char)",
    "strpbrk(src,list)",
    "strspn(src,list)",
    "strcspn(src,list)",
    "strtok(src,list)  ;use strtok(0,list) in further calls",
    "! strstr(str,substr)",
    "toupper(char)",
    "tolower(char)",
    "bcopy(src,dst,len)",
    "bzero(dst,len)",
    "! bcmp(ptr1,ptr2,len)",
    "memcpy(dst,src,len)",
    "memset(dst,fillbyte,len)",
    "! memmove(dst,src,len)",
    "! memcmp(src1,src2,len)",
    "memchr(src,scanbyte,len)",
    "rand()",
    "srand(seed)",
    "qsort(base,nel,width,callback)",
    "!! strtod(src,src_end)",
    "malloc(size)",
    "free(buf)",
    "lsearch(key,base,nel,width,callback)",
    "bsearch(key,base,nel,width,callback)",
    "calloc(sizx,sizy)",
    "realloc(old_buf,new_siz)",
    "InitHeap(addr,size)",
    "SystemErrorExit(exitcode)",
    "std_in_getchar()",
    "std_out_putchar(char)",
    "std_in_gets(dst)",
    "std_out_puts(src)",
    "printf(fmt,...)",
    "SystemErrorUnresolvedException()",
    "LoadExeHeader(filename,headerbuf)",
    "LoadExeFile(filename,headerbuf)",
    "DoExecute(headerbuf,param1,param2)",
    "FlushCache()",
    "init_a0_b0_c0_vectors",
    "GPU_dw(Xdst,Ydst,Xsiz,Ysiz,src)",
    "gpu_send_dma(Xdst,Ydst,Xsiz,Ysiz,src)",
    "SendGP1Command(gp1cmd)",
    "GPU_cw(gp0cmd)",
    "GPU_cwp(src,num)",
    "send_gpu_linked_list(src)",
    "gpu_abort_dma()",
    "GetGPUStatus()",
    "gpu_sync()",
    "SystemError",
    "SystemError",
    "LoadAndExecute(filename,stackbase,stackoffset)",
    "SystemError / GetSysSp()",
    "SystemError / set_ioabort_handler(src)",
    "CdInit_1()",
    "_bu_init_1()",
    "CdRemove_1()",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "dev_tty_init()",
    "dev_tty_open(fcb,and unused:\"path\name\",accessmode)",
    "dev_tty_in_out(fcb,cmd)",
    "dev_tty_ioctl(fcb,cmd,arg)",
    "dev_cd_open(fcb,\"path\name\",accessmode)",
    "dev_cd_read(fcb,dst,len)",
    "dev_cd_close(fcb)",
    "dev_cd_firstfile(fcb,\"path\name\",direntry)",
    "dev_cd_nextfile(fcb,direntry)",
    "dev_cd_chdir(fcb,\"path\")",
    "dev_card_open(fcb,\"path\name\",accessmode)",
    "dev_card_read(fcb,dst,len)",
    "dev_card_write(fcb,src,len)",
    "dev_card_close(fcb)",
    "dev_card_firstfile(fcb,\"path\name\",direntry)",
    "dev_card_nextfile(fcb,direntry)",
    "dev_card_erase(fcb,\"path\name\")",
    "dev_card_undelete(fcb,\"path\name\")",
    "dev_card_format(fcb)",
    "dev_card_rename(fcb1,\"path\name1\",fcb2,\"path\name2\")",
    "dev_card_clear_error_or_so(fcb)",
    "_bu_init_2()",
    "CdInit_2()",
    "CdRemove_2()",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "CdAsyncSeekL(src)",
    "NOP -> DTL-H2000_CdAsyncSeekP(src)",
    "NOP -> DTL-H2000_CdAsyncGetlocL(dst?)",
    "NOP -> DTL-H2000_CdAsyncGetlocP(dst?)",
    "CdAsyncGetStatus(dst)",
    "NOP -> DTL-H2000_CdAsyncGetParam(dst?)",
    "CdAsyncReadSector(count,dst,mode)",
    "NOP -> DTL-H2000_CdAsyncReadWithNewMode(mode)",
    "NOP -> DTL-H2000_CdAsyncReadFinalCount1(r4)",
    "CdAsyncSetMode(mode)",
    "NOP -> DTL-H2000_CdAsyncMotorOn()",
    "NOP -> DTL-H2000_CdAsyncPause()",
    "NOP -> DTL-H2000_CdAsyncPlayOrReadS()",
    "NOP -> DTL-H2000_CdAsyncStop()",
    "NOP -> DTL-H2000_CdAsyncMute()",
    "NOP -> DTL-H2000_CdAsyncDemute()",
    "NOP -> DTL-H2000_CdSetAudioVolume(src)  ;4-byte src",
    "NOP -> DTL-H2000_CdAsyncSetSession1(dst)",
    "NOP -> DTL-H2000_CdAsyncSetSession(session,dst)",
    "NOP -> DTL-H2000_CdAsyncForward()",
    "NOP -> DTL-H2000_CdAsyncBackward()",
    "NOP -> DTL-H2000_CdAsyncPlay()",
    "NOP -> DTL-H2000_CdAsyncGetStatSpecial(r4,r5)",
    "NOP -> DTL-H2000_CdAsyncGetID(r4,r5)",
    "CdromIoIrqFunc1()",
    "CdromDmaIrqFunc1()",
    "CdromIoIrqFunc2()",
    "CdromDmaIrqFunc2()",
    "CdromGetInt5errCode(dst1,dst2)",
    "CdInitSubFunc()",
    "AddCDROMDevice()",
    "AddMemCardDevice()",
    "AddDuartTtyDevice()",
    "AddDummyTtyDevice()",
    "SystemError / AddMessageWindowDevice",
    "SystemError / AddCdromSimDevice",
    "SetConf(num_EvCB,num_TCB,stacktop)",
    "GetConf(num_EvCB_dst,num_TCB_dst,stacktop_dst)",
    "SetCdromIrqAutoAbort(type,flag)",
    "SetMemSize(megabytes)",
    "WarmBoot()",
    "SystemErrorBootOrDiskFailure(type,errorcode)",
    "EnqueueCdIntr()",
    "DequeueCdIntr()",
    "CdGetLbn(filename)",
    "CdReadSector(count,sector,buffer)",
    "CdGetStatus()",
    "bu_callback_okay()",
    "bu_callback_err_write()",
    "bu_callback_err_busy()",
    "bu_callback_err_eject()",
    "_card_info(port)",
    "_card_async_load_directory(port)",
    "set_card_auto_format(flag)",
    "bu_callback_err_prev_write()",
    "card_write_test(port)",
    "NOP",
    "NOP",
    "ioabort_raw(param)",
    "NOP",
    "GetSystemInfo(index)",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP"
};

const char* const B0_FUNCS[94] = {
    "alloc_kernel_memory(size)",
    "free_kernel_memory(buf)",
    "init_timer(t,reload,flags)",
    "get_timer(t)",
    "enable_timer_irq(t)",
    "disable_timer_irq(t)",
    "restart_timer(t)",
    "DeliverEvent(class, spec)",
    "OpenEvent(class,spec,mode,func)",
    "CloseEvent(event)",
    "WaitEvent(event)",
    "TestEvent(event)",
    "EnableEvent(event)",
    "DisableEvent(event)",
    "OpenThread(reg_PC,reg_SP_FP,reg_GP)",
    "CloseThread(handle)",
    "ChangeThread(handle)",
    "NOP",
    "InitPad(buf1,siz1,buf2,siz2)",
    "StartPad()",
    "StopPad()",
    "OutdatedPadInitAndStart(type,button_dest,unused,unused)",
    "OutdatedPadGetButtons()",
    "ReturnFromException()",
    "SetDefaultExitFromException()",
    "SetCustomExitFromException(addr)",
    "SystemError",
    "SystemError",
    "SystemError",
    "SystemError",
    "SystemError",
    "SystemError",
    "UnDeliverEvent(class,spec)",
    "SystemError",
    "SystemError",
    "SystemError",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "SystemError",
    "SystemError",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "NOP",
    "FileOpen(filename,accessmode)",
    "FileSeek(fd,offset,seektype)",
    "FileRead(fd,dst,length)",
    "FileWrite(fd,src,length)",
    "FileClose(fd)",
    "FileIoctl(fd,cmd,arg)",
    "exit(exitcode)",
    "FileGetDeviceFlag(fd)",
    "FileGetc(fd)",
    "FilePutc(char,fd)",
    "std_in_getchar()",
    "std_out_putchar(char)",
    "std_in_gets(dst)",
    "std_out_puts(src)",
    "chdir(name)",
    "FormatDevice(devicename)",
    "firstfile(filename,direntry)",
    "nextfile(direntry)",
    "FileRename(old_filename,new_filename)",
    "FileDelete(filename)",
    "FileUndelete(filename)",
    "AddDevice(device_info)",
    "RemoveDevice(device_name_lowercase)",
    "PrintInstalledDevices()",
    "InitCard(pad_enable)",
    "StartCard()",
    "StopCard()",
    "_card_info_subfunc(port)",
    "write_card_sector(port,sector,src)",
    "read_card_sector(port,sector,dst)",
    "allow_new_card()",
    "Krom2RawAdd(shiftjis_code)",
    "SystemError",
    "Krom2Offset(shiftjis_code)",
    "GetLastError()",
    "GetLastFileError(fd)",
    "GetC0Table",
    "GetB0Table",
    "get_bu_callback_port()",
    "testdevice(devicename)",
    "SystemError",
    "ChangeClearPad(int)",
    "get_card_status(slot)",
    "wait_card_status(slot)"
};

const char* const C0_FUNCS[30] = {
    "EnqueueTimerAndVblankIrqs(priority)",
    "EnqueueSyscallHandler(priority)",
    "SysEnqIntRP(priority,struc)",
    "SysDeqIntRP(priority,struc)",
    "get_free_EvCB_slot()",
    "get_free_TCB_slot()",
    "ExceptionHandler()",
    "InstallExceptionHandlers()",
    "SysInitMemory(addr,size)",
    "SysInitKernelVariables()",
    "ChangeClearRCnt(t,flag)",
    "SystemError",
    "InitDefInt(priority)",
    "SetIrqAutoAck(irq,flag)",
    "NOP -> DTL-H2000_dev_sio_init",
    "NOP -> DTL-H2000_dev_sio_open",
    "NOP -> DTL-H2000_dev_sio_in_out",
    "NOP -> DTL-H2000_dev_sio_ioctl",
    "InstallDevices(ttyflag)",
    "FlushStdInOutPut()",
    "NOP -> DTL-H2000_SystemError",
    "tty_cdevinput(circ,char)",
    "tty_cdevscan()",
    "tty_circgetc(circ)",
    "tty_circputc(char,circ)",
    "ioabort(txt1,txt2)",
    "set_card_find_mode(mode)",
    "KernelRedirect(ttyflag)",
    "AdjustA0Table()",
    "get_card_find_mode()"
};

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

// Size of RAM
const uint RAM_SIZE = 0x200000;
const uint SAVE_STATE_SIZE = 0x180000;

// Main return value
const uint FUNCTION_RETURN = 0xFFFFFFFF;

// Maximum instruction executions (to prevent infinite loops)
const uint MAX_EXECUTIONS = 1 << 20;

// CLUT data size
const uint CLUT_DATA_SIZE = 0x6000;

// Integer value of JR RA instruction
const uint INST_JR_RA = 0x03E00008;

// These are used for detecting locations of framebuffer-related operations
const uint LOAD_IMAGE_ADDR = 0x00012B24;
const uint STORE_IMAGE_ADDR = 0x00012B88;
const uint MOVE_IMAGE_ADDR = 0x00012BEC;
const uint CLEAR_IMAGE_ADDR = 0x00012A90;

// These are used to skip a few functions
const uint SS_VAB_WAIT_ADDR = 0x000E3278;
const uint SPU_INIT_ADDR = 0x00027274;
const uint SETUP_AUDIO_ADDR = 0x001325D8;
const uint VSYNC_ADDR = 0x00015308;
const uint DRAWSYNC_ADDR = 0x0001290C;
const uint ADDQUE_ADDR = 0x00014670;
const uint STARTINTR_ADDR = 0x00015694;
const uint DMA_CALLBACK_ADDR = 0x0001555C;

// RAM bounds checking
const uint MAX_RAM_ADDR = 0x00200000;

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
        static void ClearRegisters();
        static void Initialize();
        static void Reset();
        static void InitSotNBinary();
        static void SaveState();
        static void LoadState();
        //static void ClearRegisters();
        static void Cleanup();
        static void ProcessOpcode(uint opcode);
        static void ProcessFunction(uint func_addr, uint end_addr = 0);
        static void LoadFile(const char* filename, uint addr);
        static uint ReadIntFromRAM(uint addr);
        static void WriteIntToRAM(uint addr, uint value);
        static void CopyToRAM(uint addr, const void* src, uint count);
        static void CopyFromRAM(uint addr, void* dst, uint count);
        static std::vector<Entity> FindNewEntities(int exclude_addr);
        static std::vector<Entity> ProcessEntities();
        static void ClearEntities();

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

        // 2 MB buffer for save state
        static byte* save_state_ram;

        // Sizes of each binary
        static uint psx_bin_size;
        static uint sotn_bin_size;
        static uint map_data_size;

        // HI and LO registers (DIV, MULT, etc.)
        static uint hi;
        static uint lo;

        // Whether a function is being forcibly returned from
        static bool force_ret;

        // Whether a top-level return is occurring
        static bool ret_hit;
};

#endif //SOTN_EDITOR_MIPS