#include <stdbool.h>
#include "common.h"



// Class for compression utilities
class Compression {
	
    public:

    	static int Decompress(byte* dst, byte* src);



	private:

		static bool read_high;
		static bool write_high;
		static byte* decompress_buffer;
		static byte* offset;

		static byte ReadFromCompressedBuffer(void);
		static void AppendToDecompressedBuffer(byte newchar);
};
