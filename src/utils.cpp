#include <stdlib.h>
#include <stdio.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <string>
#include <string.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include "common.h"
#include "utils.h"




// Convert a 16-bit RGB1555 value to a 32-bit RGBA value
uint Utils::RGB1555_to_RGBA(ushort color) {

	// Get the RGB1555 components
    byte t = (color >> 15) & 1;
    byte b = (color >> 10) & 0x1F;
    byte g = (color >> 5) & 0x1F;
    byte r = color & 0x1F;

    // Normalize to byte values
    t = t == 1 ? 0x80 : 0xFF;
    b = (b << 3) | (b >> 2);
    g = (g << 3) | (g >> 2);
    r = (r << 3) | (r >> 2);

    // Return value as a single RGBA int
    return (r << 24) | (g << 16) | (b << 8) | t;
}



// Convert a 32-bit RGBA value to a 16-bit RGB1555 value
ushort Utils::RGBA_to_RGB1555(uint color) {

    // Get the RGB components
    byte r = (byte)(color) >> 3;
    byte g = (byte)(color >> 8) >> 3;
    byte b = (byte)(color >> 16) >> 3;
    byte t = (byte)(color >> 24);

    // Adjust alpha value
    t = t == 0x80 ? 1 : 0;

    // Return value as a single RGB1555 short
    return (t << 15) | (b << 10) | (g << 5) | r;
}



// Convert an indexed series of bytes to their RGBA equivalent
byte* Utils::Indexed_to_RGBA(const byte* data, uint num_bytes) {

	// Convert file data to RGBA pixels
    byte* pixel_data = (byte*)calloc(num_bytes * 4, sizeof(byte));

    // Split all of the packed indices into their RGB components
    for (int i = 0; i < num_bytes; i++) {

        // Get the current index
        ushort cur_color = *(ushort*)(data + (i * 2));

        // Create RGBA color from the indices
        uint color = RGB1555_to_RGBA(cur_color);

        // Set the RGBA data
        *(byte*)(pixel_data + (i * 4)) = (byte)(color >> 24);
        *(byte*)(pixel_data + (i * 4) + 1) = (byte)(color >> 16);
        *(byte*)(pixel_data + (i * 4) + 2) = (byte)(color >> 8);
        *(byte*)(pixel_data + (i * 4) + 3) = (byte)(color);
    }

    // Return the allocated buffer
    return pixel_data;
}



// Convert a series of RGBA bytes to their indexed equivalents
byte* Utils::RGBA_to_Indexed(const byte* data, uint num_bytes) {

    // Convert file data to RGBA pixels
    byte* pixel_data = (byte*)calloc(num_bytes * 2, sizeof(byte));

    // Split all of the packed indices into their RGB components
    for (int i = 0; i < num_bytes; i++) {

        // Get the current index
        uint cur_color = *(uint*)(data + (i * 4));

        // Create RGBA color from the indices
        uint color = RGBA_to_RGB1555(cur_color);

        // Set the RGBA data
        *(ushort*)(pixel_data + (i * 2)) = (ushort)color;
    }

    // Return the allocated buffer
    return pixel_data;
}



// Convert a buffer of RGBA1555 CLUT data to the RGBA equivalent
void Utils::CLUT_to_RGBA(const byte* src, const byte* dst, int num_cluts, bool semi_opaque) {

    // Convert RGB1555 CLUT to RGBA CLUT
    for (uint i = 0; i < num_cluts * 16; i++) {

        // Get the current RGB1555 CLUT
        ushort color = *(ushort*)(src + (i * 2));

        // Convert to RGBA
        uint rgba_color = Utils::RGB1555_to_RGBA(color);

        // Check whether alpha should be forced or not
        uint alpha = 0xFF000000;
        if (semi_opaque) {
            alpha = ((rgba_color << 24) & 0xFF000000);
        }

        // Invert bytes and force full opacity
        rgba_color = ((rgba_color >> 24) & 0xFF) | ((rgba_color >> 8) & 0xFF00) | ((rgba_color << 8) & 0xFF0000) | alpha;

        // Set RGBA CLUT value
        *(uint*)(dst + (i * 4)) = rgba_color;
    }
}



// Create a texture out of source data
GLuint Utils::CreateTexture(void* data, int width, int height) {

    // Initialize the texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    // Create the texture from the image data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Set the map VRAM
    return texture;
}



// Convert indexed image + CLUT into RGBA data
// Returns the number of non-empty pixels
uint Utils::VRAM_to_RGBA(const byte* pixels, const byte* clut, uint width, uint height, byte* output) {

    // Number of opaque pixels
    uint written_pixels = 0;

    // Convert all pixels to their CLUT components
    for (int i = 0; i < width * height; i++) {

        // Get the current pixel
        uint cur_pix = *(uint*)(pixels + (i * 4));

        // Convert the RGBA value into an RGB1555 value
        ushort packed_indices = Utils::RGBA_to_RGB1555(cur_pix);

        // Get the CLUT indices
        byte idx1 = (packed_indices & 0xF) * 4;
        byte idx2 = ((packed_indices >> 4) & 0xF) * 4;
        byte idx3 = ((packed_indices >> 8) & 0xF) * 4;
        byte idx4 = ((packed_indices >> 12) & 0xF) * 4;

        // Write the CLUT color values to the tile output
        output[i * 16] = *(byte*)(clut + idx1);
        output[i * 16 + 1] = *(byte*)(clut + idx1 + 1);
        output[i * 16 + 2] = *(byte*)(clut + idx1 + 2);
        output[i * 16 + 3] = *(byte*)(clut + idx1 + 3);
        output[i * 16 + 4] = *(byte*)(clut + idx2);
        output[i * 16 + 5] = *(byte*)(clut + idx2 + 1);
        output[i * 16 + 6] = *(byte*)(clut + idx2 + 2);
        output[i * 16 + 7] = *(byte*)(clut + idx2 + 3);
        output[i * 16 + 8] = *(byte*)(clut + idx3);
        output[i * 16 + 9] = *(byte*)(clut + idx3 + 1);
        output[i * 16 + 10] = *(byte*)(clut + idx3 + 2);
        output[i * 16 + 11] = *(byte*)(clut + idx3 + 3);
        output[i * 16 + 12] = *(byte*)(clut + idx4);
        output[i * 16 + 13] = *(byte*)(clut + idx4 + 1);
        output[i * 16 + 14] = *(byte*)(clut + idx4 + 2);
        output[i * 16 + 15] = *(byte*)(clut + idx4 + 3);

        // Correct any transparent pixels
        if (*(byte*)(clut + idx1) + *(byte*)(clut + idx1 + 1) + *(byte*)(clut + idx1 + 2) == 0) {
            output[i * 16 + 3] = 0;
        }
        else {
            written_pixels++;
        }
        if (*(byte*)(clut + idx2) + *(byte*)(clut + idx2 + 1) + *(byte*)(clut + idx2 + 2) == 0) {
            output[i * 16 + 7] = 0;
        }
        else {
            written_pixels++;
        }
        if (*(byte*)(clut + idx3) + *(byte*)(clut + idx3 + 1) + *(byte*)(clut + idx3 + 2) == 0) {
            output[i * 16 + 11] = 0;
        }
        else {
            written_pixels++;
        }
        if (*(byte*)(clut + idx4) + *(byte*)(clut + idx4 + 1) + *(byte*)(clut + idx4 + 2) == 0) {
            output[i * 16 + 15] = 0;
        }
        else {
            written_pixels++;
        }
    }

    // Return the number of written pixels
    return written_pixels;
}



// Dump [num_bytes] from [buf]
void Utils::HexDump(const byte* buf, const uint num_bytes) {

    // Print each byte
    for (int i = 0; i < num_bytes; i++) {
        if (i % 16 == 0) {
            printf("\n");
        }
        printf("%02X ", *(byte*)(buf + i));
    }
    printf("\n");
}



// Helper function to get pixels from a given texture
byte* Utils::GetPixels(const GLuint texture, uint x, uint y, uint width, uint height) {

    // Pixels to return
    byte* pixels = (byte*)calloc(width * height * 4, sizeof(byte));

    // Attach to the texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Return the pixels that were read
    return pixels;
}



// Helper function to set a range of pixels for a given texture
void Utils::SetPixels(const GLuint texture, uint x, uint y, uint width, uint height, byte *pixels) {

    // Bind to the target texture
    glBindTexture(GL_TEXTURE_2D, texture);

    // Overwrite the rectangular region
    glTexSubImage2D(
        GL_TEXTURE_2D, 0,
        x, y,
        width, height,
        GL_RGBA, GL_UNSIGNED_BYTE,
        pixels
    );

    // Reset target
    glBindTexture(GL_TEXTURE_2D, 0);
}



// Convert a SotN format string to a regular ASCII string
std::string Utils::ReadSotnString(const byte* data) {

    byte cur_char = *(byte*)(data);
    uint count = 0;

    // Determine number of bytes to copy
    while (cur_char != 0xFF) {
        count += 1;
        cur_char = *(byte*)(data + count);
    }

    // Copy the bytes to a new buffer
    byte* new_string = (byte*)calloc(count, sizeof(byte));
    memcpy(new_string, data, count);

    // Convert each character
    for (int i = 0; i < count; i++) {
        *(new_string + i) += 0x20;
    }

    // Return a null pointer if string is null
    if (new_string[0] == 0) {
        return "";
    }

    // Return the byte array as a string (parameters are passed as string initializer args)
    return {(char*)new_string, count};
}



void Utils::FindReplace(std::string* str, std::string find, std::string replace) {

    // Current position in the string to search
    uint cur_pos = 0;

    // Process all characters
    while (cur_pos < str->length()) {

        // Get the next occurrence in the string
        cur_pos = str->find(find, cur_pos);

        // Bail if searched string wasn't found
        if (cur_pos == -1) {
            break;
        }

        // Replace the occurrence in the string
        str->replace(cur_pos, find.length(), replace);

        // Move to next character
        cur_pos += replace.length();
    }
}



// Convert a string with SJIS chars into an ASCII string
void Utils::SJIS_to_ASCII(std::string* str) {

    uint replaced = 0;

    byte* bytes = (byte*)str->c_str();

    for (uint i = 0; i < str->length() - replaced; i++) {
        *(bytes + i) = *(bytes + i + replaced);
        if (*(bytes + i) > 0x80) {
            ushort test = *(ushort*)(bytes + i + replaced);
            *(bytes + i) = sjis_lookup_table.at(test);
            replaced += 1;
        }
    }

    bytes[str->length() - replaced] = 0;
    str->assign((char*)bytes);
}