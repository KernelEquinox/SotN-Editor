#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "compression.h"




// -------------------------------------------------- Decompression --------------------------------------------------

// Variables
bool Compression::read_high;
bool Compression::write_high;
byte* Compression::decompress_buffer;
byte* Compression::offset;



byte Compression::ReadFromCompressedBuffer() {

    byte ret_val;
    
    if (read_high) {
        ret_val = *offset;
        offset += 1;
        ret_val = ret_val & 0xF;
    }
    else {
        ret_val = *offset >> 4;
    }
    read_high = !read_high;
    return ret_val;
}



void Compression::AppendToDecompressedBuffer(byte new_char) {
    if (!write_high) {
        *decompress_buffer = new_char;
    }
    else {
        *decompress_buffer += (new_char << 4);
        decompress_buffer++;
    }
    write_high = !write_high;
}



// Thank you based Ghidra
int Compression::Decompress(byte* dst, byte* src) {
    byte bVar1;
    byte cVar2;
    uint uVar3;
    int iVar4;
    uint *puVar5;
    int iVar6;
    uint uVar7;
    uint auStack116 [23];
    
    puVar5 = auStack116 + 7;
    iVar4 = 0;
    do {
        bVar1 = *src;
        src++;
        iVar4++;
        *puVar5 = (uint)bVar1;
        puVar5++;
    } while (iVar4 < 8);
    read_high = 0;
    write_high = false;
    offset = src;
    decompress_buffer = dst;
    do {
        uVar3 = ReadFromCompressedBuffer();
        switch(uVar3 & 0xFF) {
        case 0:
            iVar6 = 0;
            uVar3 = ReadFromCompressedBuffer();
            uVar7 = ReadFromCompressedBuffer();
            iVar4 = (uVar3 & 0xFF) * 0x10 + (uVar7 & 0xFF);
            if (iVar4 != -0x13) {
                do {
                    AppendToDecompressedBuffer(0x00);
                    iVar6++;
                } while (iVar6 < iVar4 + 0x13);
            }
            break;
        case 1:
            cVar2 = ReadFromCompressedBuffer();
            AppendToDecompressedBuffer(cVar2);
            break;
        case 2:
            cVar2 = ReadFromCompressedBuffer();
            AppendToDecompressedBuffer(cVar2);
            AppendToDecompressedBuffer(cVar2);
            break;
        case 4:
            cVar2 = ReadFromCompressedBuffer();
            AppendToDecompressedBuffer(cVar2);
        case 3:
            cVar2 = ReadFromCompressedBuffer();
            AppendToDecompressedBuffer(cVar2);
            cVar2 = ReadFromCompressedBuffer();
            AppendToDecompressedBuffer(cVar2);
            break;
        case 5:
            iVar4 = 0;
            cVar2 = ReadFromCompressedBuffer();
            uVar3 = ReadFromCompressedBuffer();
            if ((uVar3 & 0xFF) != 0xFFFFFFFD) {
                do {
                    AppendToDecompressedBuffer(cVar2);
                    iVar4++;
                } while (iVar4 < (int)((uVar3 & 0xFF) + 3));
            }
            break;
        case 6:
            iVar4 = 0;
            uVar3 = ReadFromCompressedBuffer();
            if ((uVar3 & 0xFF) != 0xFFFFFFFD) {
                do {
                    AppendToDecompressedBuffer(0x00);
                    iVar4++;
                } while (iVar4 < (int)((uVar3 & 0xFF) + 3));
            }
            break;
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
            uVar7 = auStack116[uVar3 & 0xFF];
            uVar3 = uVar7 & 0xF0;
            if (uVar3 == 0x20) {
                AppendToDecompressedBuffer((byte)uVar7 & 0xF);
                AppendToDecompressedBuffer((byte)uVar7 & 0xF);
            }
            else if (uVar3 < 0x21) {
                if (uVar3 == 0x10) {
                    AppendToDecompressedBuffer((byte)uVar7 & 0xF);
                }
            }
            else if (uVar3 == 0x60) {
                iVar4 = 0;
                if ((uVar7 & 0xF) != 0xFFFFFFFD) {
                    do {
                        AppendToDecompressedBuffer(0x00);
                        iVar4++;
                    } while (iVar4 < (int)((uVar7 & 0xF) + 3));
                }
            }
            break;
        case 0xF:
            if (dst + 0x2000 < decompress_buffer) {
                iVar4 = (decompress_buffer - dst) + 0x2000;
            }
            else {
                iVar4 = 0;
            }
            return iVar4;
        }
    } while (true);
}