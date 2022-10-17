#ifndef SOTN_EDITOR_GLOBALS_H
#define SOTN_EDITOR_GLOBALS_H

#include <GLFW/glfw3.h>
#include <vector>
#include "sprites.h"

extern byte* generic_rgba_cluts;
extern GLuint generic_cluts_texture;
extern GLuint fgame_texture;
extern std::vector<GLuint> fgame_textures;
extern std::vector<GLuint> item_textures;
extern byte* item_cluts;
extern std::vector<std::vector<Sprite>> generic_sprite_banks;
extern GLuint generic_powerup_texture;
extern GLuint generic_saveroom_texture;
extern GLuint generic_loadroom_texture;

#endif //SOTN_EDITOR_GLOBALS_H
