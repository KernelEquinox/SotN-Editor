#include "common.h"



// Class for compression utilities
class Compression {
	
    public:

    	static uint Decompress(byte* dst, byte* src);



	private:

		static bool read_high;
		static bool write_high;
		static byte* decompress_buffer;
		static byte* offset;

		static byte ReadFromCompressedBuffer();
		static void AppendToDecompressedBuffer(byte new_char);
};
