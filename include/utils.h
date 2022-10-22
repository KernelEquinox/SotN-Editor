#ifndef SOTN_EDITOR_UTILS
#define SOTN_EDITOR_UTILS

#include <map>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"



// SJIS -> ASCII lookup table
const std::map<ushort, byte> sjis_lookup_table = {
    {0x4081, ' '},
    {0x4381, ','},
    {0x4481, '.'},
    // {0x4581, '・'},
    {0x4681, ':'},
    {0x4781, ';'},
    {0x4881, '?'},
    {0x4981, '!'},
    {0x4F81, '^'},
    {0x5B81, '-'},
    {0x5C81, '-'},
    {0x5D81, '-'},
    {0x5E81, '/'},
    {0x5F81, '\\'},
    {0x6081, '~'},
    {0x6281, '|'},
    {0x6581, '\''},
    {0x6681, '\''},
    {0x6781, '"'},
    {0x6881, '"'},
    {0x6981, '('},
    {0x6A81, ')'},
    {0x6B81, '('},
    {0x6C81, ')'},
    {0x6D81, '['},
    {0x6E81, ']'},
    {0x6F81, '{'},
    {0x7081, '}'},
    {0x7181, '<'},
    {0x7281, '>'},
    {0x7981, '['},
    {0x7A81, ']'},
    {0x7B81, '+'},
    {0x7C81, '-'},
    {0x7E81, 'x'},
    {0x8181, '='},
    {0x8381, '<'},
    {0x8481, '>'},
    {0x8C81, '\''},
    {0x8D81, '"'},
    {0x8F81, '$'},
    {0x9081, '$'},
    {0x9381, '%'},
    {0x9481, '#'},
    {0x9581, '&'},
    {0x9681, '*'},
    {0x4F82, '0'},
    {0x5082, '1'},
    {0x5182, '2'},
    {0x5282, '3'},
    {0x5382, '4'},
    {0x5482, '5'},
    {0x5582, '6'},
    {0x5682, '7'},
    {0x5782, '8'},
    {0x5882, '9'},
    {0x6082, 'A'},
    {0x6182, 'B'},
    {0x6282, 'C'},
    {0x6382, 'D'},
    {0x6482, 'E'},
    {0x6582, 'F'},
    {0x6682, 'G'},
    {0x6782, 'H'},
    {0x6882, 'I'},
    {0x6982, 'J'},
    {0x6A82, 'K'},
    {0x6B82, 'L'},
    {0x6C82, 'M'},
    {0x6D82, 'N'},
    {0x6E82, 'O'},
    {0x6F82, 'P'},
    {0x7082, 'Q'},
    {0x7182, 'R'},
    {0x7282, 'S'},
    {0x7382, 'T'},
    {0x7482, 'U'},
    {0x7582, 'V'},
    {0x7682, 'W'},
    {0x7782, 'X'},
    {0x7882, 'Y'},
    {0x7982, 'Z'},
    {0x8182, 'a'},
    {0x8282, 'b'},
    {0x8382, 'c'},
    {0x8482, 'd'},
    {0x8582, 'e'},
    {0x8682, 'f'},
    {0x8782, 'g'},
    {0x8882, 'h'},
    {0x8982, 'i'},
    {0x8A82, 'j'},
    {0x8B82, 'k'},
    {0x8C82, 'l'},
    {0x8D82, 'm'},
    {0x8E82, 'n'},
    {0x8F82, 'o'},
    {0x9082, 'p'},
    {0x9182, 'q'},
    {0x9282, 'r'},
    {0x9382, 's'},
    {0x9482, 't'},
    {0x9582, 'u'},
    {0x9682, 'v'},
    {0x9782, 'w'},
    {0x9882, 'x'},
    {0x9982, 'y'},
    {0x9A82, 'z'}
};



// Class for general utilities
class Utils {

	public:

		static uint RGB1555_to_RGBA(ushort color);
		static ushort RGBA_to_RGB1555(uint color);
		static GLuint CreateTexture(void* data, int width, int height);
        static byte* Indexed_to_RGBA(const byte* data, uint num_bytes);
        static byte* RGBA_to_Indexed(const byte* data, uint num_bytes);
        static void CLUT_to_RGBA(const byte* src, const byte* dst, int num_cluts, bool semi_opaque);
        static uint VRAM_to_RGBA(const byte* pixels, const byte* clut, uint width, uint height, byte* output);
        static void HexDump(const byte* buf, const uint num_bytes);
        static byte* GetPixels(const GLuint texture, uint x, uint y, uint width, uint height);
        static void SetPixels(const GLuint texture, uint x, uint y, uint width, uint height, byte* pixels);
        static std::string ReadSotnString(const byte* data);
        static void FindReplace(std::string* str, const std::string& find, const std::string& replace);
        static void SJIS_to_ASCII(std::string* str);


	private:
    
};

#endif //SOTN_EDITOR_UTILS