#include "sprites.h"
#include "common.h"
#include <vector>



/**
 * Reads sprite data from a given buffer.
 *
 * @param buf: Buffer to read sprite data from
 * @param offset: Offset of sprite bank within the buffer
 * @param base: Base address of the buffer
 *
 * @return A vector of sprite banks
 *
 * @note Sprite banks themselves are vectors of sprites.
 *
 */
std::vector<std::vector<Sprite>> Sprite::ReadSpriteBanks(const byte* buf, const uint offset, const uint base) {

    // Create a new
    std::vector<std::vector<Sprite>> sprite_banks;

    // Start at bank 1
    uint bank_num = 0;
    uint bank_addr = *(uint*)(buf + offset + (bank_num * 4));
    while ((bank_addr >= RAM_BASE_OFFSET && bank_addr < RAM_MAX_OFFSET) || bank_addr == 0) {

        // Initialize a new set of sprites
        std::vector<Sprite> sprites;

        // Add blank entries for null pointers
        if (bank_addr == 0) {
            sprite_banks.push_back(sprites);
            bank_addr = *(uint*)(buf + offset + (++bank_num * 4));
            continue;
        }

        // Adjust bank address relative to base address
        bank_addr -= base;

        // Loop through each sprite in the bank
        uint sprite_num = 0;
        uint sprite_addr = *(uint*)(buf + bank_addr + (sprite_num * 4));
        while ((sprite_addr >= RAM_BASE_OFFSET && sprite_addr < RAM_MAX_OFFSET) || sprite_addr == 0) {

            // Initialize a new sprite
            Sprite sprite;

            // Add blank entries for null pointers
            if (sprite_addr == 0) {
                sprites.push_back(sprite);
                sprite_addr = *(uint*)(buf + bank_addr + (++sprite_num * 4));
                continue;
            }

            // Adjust sprite address to buffer offset
            sprite_addr -= base;

            // Get the number of sprite parts
            // Note: This can be over 0x8000 (flag set)
            ushort num_parts = *(ushort*)(buf + sprite_addr);

            // Loop through each sprite part
            for (int x = 0; x < num_parts; x++) {
                uint cur_offset = x * sizeof(SpritePart);
                SpritePart part = *(SpritePart*)(buf + sprite_addr + cur_offset + 2);
                sprite.parts.push_back(part);
                sprite.address = sprite_addr + cur_offset;
            }

            // Add the sprite to the list
            sprites.push_back(sprite);

            // Get the next sprite address
            sprite_addr = *(uint*)(buf + bank_addr + (++sprite_num * 4));
        }

        // Add the sprite list to the bank
        sprite_banks.push_back(sprites);

        // Get the next bank address
        bank_addr = *(uint*)(buf + offset + (++bank_num * 4));
    }

    // Return the sprite data
    return sprite_banks;
}


