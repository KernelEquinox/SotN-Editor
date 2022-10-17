#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// NativeFileDialog header
#include <nfd.h>


#include "common.h"
#include "globals.h"
#include "mips.h"
#include "gte.h"
#include "compression.h"
#include "entities.h"
#include "map.h"
#include "utils.h"


#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}





// Paths for DRA.BIN and F_GAME.BIN
static char* psx_path;
static char* bin_path;
static char* gfx_path;
Map map;

// Any errors that might appear
std::string error;

// Vertex index
static int vtx_idx;

// Define globals
byte* generic_rgba_cluts;
GLuint generic_cluts_texture;
GLuint fgame_texture;
std::vector<GLuint> fgame_textures;
std::vector<GLuint> item_textures;
byte* item_cluts;
std::vector<std::vector<Sprite>> generic_sprite_banks;
GLuint generic_powerup_texture;
GLuint generic_saveroom_texture;
GLuint generic_loadroom_texture;

// Open the settings file for reading
static void* SotN_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    if (strcmp(name, "Data") != 0) {
        return nullptr;
    }
    return (void*)1;
}

// Read each line of the settings file
static void SotN_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler*, void*, const char* line)
{
    char s[2048] = {0};
    if (sscanf(line, "PsxPath=%[^\n]", s) == 1)                 { psx_path = ImStrdup(s); }
    else if (sscanf(line, "BinPath=%[^\n]", s) == 1)            { bin_path = ImStrdup(s); }
    else if (sscanf(line, "GfxPath=%[^\n]", s) == 1)            { gfx_path = ImStrdup(s); }
}

// Write all lines of the settings file
static void SotN_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    // Create the data entry for the INI file
    buf->appendf("[%s][Data]\n", handler->TypeName);

    // Save the file paths
    buf->appendf("PsxPath=%s\n", psx_path);
    buf->appendf("BinPath=%s\n", bin_path);
    buf->appendf("GfxPath=%s\n", gfx_path);
    buf->append("\n");
}





static void blend_lighten(const ImDrawList* draw_list, const ImDrawCmd* cmd) {
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
}

static void blend_fade_light(const ImDrawList* draw_list, const ImDrawCmd* cmd) {
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_DST_ALPHA);
}

static void blend_default(const ImDrawList* draw_list, const ImDrawCmd* cmd) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}



//
// Usage:
//
//     nfdfilteritem_t filters[2] = {
//         { "Map files", "bin" },
//         { "Texture files", "tim" }
//     };
//     nfdchar_t open_file(filters);
//
static nfdchar_t* open_file(nfdfilteritem_t filters[]) {
    nfdchar_t* out_path;
    nfdresult_t result = NFD_OpenDialog(&out_path, filters, 1, nullptr);
    if (result == NFD_OKAY) {
        return out_path;
    }
    else if (result == NFD_CANCEL) {
        // User pressed the cancel button
        error = "File selection canceled.";
    }
    else {
        error = NFD_GetError();
    }
    return nullptr;
}



// Initialize the MIPS emulator and most base game-related stuff
void load_sotn_data() {

    // Load binaries
    MipsEmulator::SetPSXBinary(psx_path);
    MipsEmulator::SetSotNBinary(bin_path);

    // Initial reset
    MipsEmulator::Reset();



    // Get generic sprite data
    generic_sprite_banks = Sprite::ReadSpriteBanks(MipsEmulator::ram, GENERIC_SPRITE_BANKS_ADDR, RAM_BASE_OFFSET);



    // Read the generic CLUT data
    byte* fgame_pixels = (byte*)calloc(256 * 512 * 2, sizeof(byte));
    byte* generic_cluts = (byte*)calloc(256 * 16 * 2, sizeof(byte));
    FILE* f_game = fopen(gfx_path, "rb");

    // Read the pixels
    fread(fgame_pixels, sizeof(byte), 256 * 512 * 2, f_game);

    byte* fgame_pixeldata = Utils::Indexed_to_RGBA(fgame_pixels, 256 * 512);

    // Read the texture data
    for (int i = 0; i < (256 * 512 * 2) / 8192; i++) {

        GLuint chunk_texture = Utils::CreateTexture(fgame_pixeldata + (i * 8192 * 2), 32, 128);

        // Add the texture pointer to the list of map textures
        fgame_textures.push_back(chunk_texture);
    }

    fseek(f_game, 256 * 512 * 2, SEEK_SET);
    fread(generic_cluts, sizeof(byte), 256 * 16 * 2, f_game);
    fclose(f_game);
    free(fgame_pixels);

    // Convert all generic CLUTs to their RGBA equivalents
    generic_rgba_cluts = (byte*)calloc(256 * 16 * 4, sizeof(byte));

    // Convert RGB1555 CLUT to RGBA CLUT
    Utils::CLUT_to_RGBA(generic_cluts, generic_rgba_cluts, 256, false);

    // Create a texture for the RGBA CLUTs
    generic_cluts_texture = Utils::CreateTexture(generic_rgba_cluts, 256, 16);

    // Allocate data for a full VRAM texture
    byte* vram_data = (byte*)calloc(256 * 512 * 2 * 4 * 2, sizeof(byte));

    // Keep track of offset into VRAM
    uint cur_offset = 0;

    // Create a new framebuffer
    glewInit();
    GLuint tmpfbo;
    glGenFramebuffers(1, &tmpfbo);

    // Bind to the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, tmpfbo);

    // Loop through each line of all 32 textures
    for (int k = 0; k < 128; k++) {
        for (int i = 0; i < 32; i++) {

            // Only process if this is within the TOP half of VRAM
            if (i % 4 < 2) {

                // Attach to the target texture and read the pixels into VRAM
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fgame_textures[i], 0);
                glReadPixels(0, k, 32, 1, GL_RGBA, GL_UNSIGNED_BYTE, vram_data + cur_offset);
                cur_offset += 32 * 4;
            }
        }
    }

    // Loop through each line of all 32 textures
    for (int k = 0; k < 128; k++) {
        for (int i = 0; i < 32; i++) {

            // Only process if this is within the BOTTOM half of VRAM
            if (i % 4 > 1) {

                // Attach to the target texture and read the pixels into VRAM
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fgame_textures[i], 0);
                glReadPixels(0, k, 32, 1, GL_RGBA, GL_UNSIGNED_BYTE, vram_data + cur_offset);
                cur_offset += 32 * 4;

            }
        }
    }

    fgame_texture = Utils::CreateTexture(vram_data, 512, 256);

    // Clear out the F_GAME texture list
    fgame_textures.clear();

    // Bind to the F_GAME texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fgame_texture, 0);

    // Collect all of the tilesets
    for (int i = 0; i < 8; i++) {

        // Read a block of pixels from VRAM
        byte* tileset_data = (byte*)calloc(64 * 256 * 4, sizeof(byte));
        glReadPixels(i * 64, 0, 64, 256, GL_RGBA, GL_UNSIGNED_BYTE, tileset_data);

        // Create a texture from each VRAM block
        GLuint tileset_texture = Utils::CreateTexture(tileset_data, 64, 256);

        // Free the tileset data
        free(tileset_data);

        // Add the tileset to the list of the map's tilesets
        fgame_textures.push_back(tileset_texture);
    }

    // Reset the framebuffer target to the main window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Delete the framebuffer
    glDeleteFramebuffers(1, &tmpfbo);



    // Get the generic map textures (load/save/etc.)
    uint data_size = 0x80 * 0x80;

    // Read the powerup tileset
    byte* compressed_data = (byte*)calloc(data_size, sizeof(byte));
    byte* tileset_data = (byte*)calloc(data_size, sizeof(byte));
    MipsEmulator::CopyFromRAM(COMPRESSED_GENERIC_POWERUP_TILESET_ADDR, compressed_data, data_size);
    Compression::Decompress(tileset_data, compressed_data);
    byte* pixel_data = Utils::Indexed_to_RGBA(tileset_data, data_size / 4);
    generic_powerup_texture = Utils::CreateTexture(pixel_data, 0x80 / 4, 0x80);
    free(pixel_data);

    // Read the save room tileset
    tileset_data = (byte*)realloc(tileset_data, data_size);
    compressed_data = (byte*)realloc(compressed_data, data_size);
    MipsEmulator::CopyFromRAM(COMPRESSED_GENERIC_SAVEROOM_TILESET_ADDR, compressed_data, data_size);
    Compression::Decompress(tileset_data, compressed_data);
    pixel_data = Utils::Indexed_to_RGBA(tileset_data, data_size / 4);
    generic_saveroom_texture = Utils::CreateTexture(pixel_data, 0x80 / 4, 0x80);
    free(pixel_data);

    // Read the save room tileset
    tileset_data = (byte*)realloc(tileset_data, data_size);
    compressed_data = (byte*)realloc(compressed_data, data_size);
    MipsEmulator::CopyFromRAM(COMPRESSED_GENERIC_LOADROOM_TILESET_ADDR, compressed_data, data_size);
    Compression::Decompress(tileset_data, compressed_data);
    pixel_data = Utils::Indexed_to_RGBA(tileset_data, data_size / 4);
    generic_loadroom_texture = Utils::CreateTexture(pixel_data, 0x80 / 4, 0x80);
    free(pixel_data);
    free(tileset_data);
    free(compressed_data);





    // Read item and relic stuff
    byte* dra_bin_pixels = (byte*)calloc(((16 * 16) / 2) * 275, sizeof(byte));
    byte* dra_bin_cluts = (byte*)calloc(320 * 16 * 2, sizeof(byte));
    FILE* dra_bin = fopen(bin_path, "rb");

    // Read the item sprites and CLUTs
    fseek(dra_bin, ITEM_SPRITES_ADDR - SOTN_RAM_OFFSET, SEEK_SET);
    fread(dra_bin_pixels, sizeof(byte), ((16 * 16) / 2) * 275, dra_bin);
    fseek(dra_bin, ITEM_CLUTS_ADDR - SOTN_RAM_OFFSET, SEEK_SET);
    fread(dra_bin_cluts, sizeof(byte), 320 * 16 * 2, dra_bin);
    fclose(dra_bin);

    // Get the pixel data for the items
    byte* item_pixel_data = Utils::Indexed_to_RGBA(dra_bin_pixels, ((16 * 16) / 2) * 275);

    // Read the texture data
    for (int i = 0; i < (((16 * 16) / 2) * 275) / 0x80; i++) {

        GLuint chunk_texture = Utils::CreateTexture(item_pixel_data + (i * 0x80 * 2), 16 / 4, 16);

        // Add the texture pointer to the list of map textures
        item_textures.push_back(chunk_texture);
    }

    // Convert all item CLUTs to their RGBA equivalents
    item_cluts = (byte*)calloc(320 * 16 * 4, sizeof(byte));

    // Convert RGB1555 CLUT to RGBA CLUT
    Utils::CLUT_to_RGBA(dra_bin_cluts, item_cluts, 320, false);
}






int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SotN Editor", NULL, NULL);
    if (window == NULL)
        return 1;

    // Initialize GLEW
    glewInit();

    // Initialize NFD
    NFD_Init();

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGuiContext* g = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Custom type handler
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "SotN";
    ini_handler.TypeHash = ImHashStr("SotN");
    ini_handler.ReadOpenFn = SotN_ReadOpen;
    ini_handler.ReadLineFn = SotN_ReadLine;
    ini_handler.WriteAllFn = SotN_WriteAll;
    g->SettingsHandlers.push_back(ini_handler);

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Clear color
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);














    // Uncomment these to enable debug printing to stdout
    //MipsEmulator::debug = true;
    //GteEmulator::debug = true;

    // Keep dockspace open
    static bool open = true;

    // Flag whether program should exit
    static bool exit = false;

    // Zoom size
    static float zoom_amount = 1;

    // Currently-selected entity in the Main Viewport
    static Entity* selected_entity;














    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Check if the program should exit
        if (exit) {
            break;
        }






// -- Menu Bar -------------------------------------------------------------------------------------------------

        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {

                // Utilize NFD to open the map file
                if (ImGui::MenuItem("Open Map File", "Ctrl+O")) {
                    nfdfilteritem_t filters[1] = {
                        {
                            "Map files",
                            "bin,BIN"
                        }
                    };

                    // Get the selected file
                    nfdchar_t *out_path = open_file(filters);

                    // Make sure a file was selected
                    if (out_path != nullptr) {

                        // Get the filename and directory of the map file
                        std::filesystem::path map_path = std::filesystem::path(out_path);
                        std::string map_dir = map_path.parent_path().string();
                        std::string map_filename = map_path.filename().string();
                        std::string map_gfx_file = map_dir + "/F_" + map_filename;

                        // Make sure map graphics file exists in the same directory as the map file itself
                        if (FILE* fp = fopen(map_gfx_file.c_str(), "r")) {

                            // Initialize the emulator
                            if (MipsEmulator::initialized) {
                                map.Cleanup();
                                selected_entity = nullptr;
                                //MipsEmulator::Cleanup();
                            }
                            else {
                                MipsEmulator::Initialize();
                                // Initialize the graphics data, populate MIPS RAM, etc.
                                load_sotn_data();
                            }

                            // Load and process the map file
                            map.LoadMapFile(map_path.c_str());

                            // Load the map file itself into memory
                            MipsEmulator::LoadMapFile(map_path.c_str());

                            // Load the map graphics
                            map.LoadMapGraphics(map_gfx_file.c_str());


                            // Store map tile CLUTs in MIPS RAM
                            for (int i = 0; i < 256; i++) {
                                MipsEmulator::StoreMapCLUT(i * 32, 32, map.map_tile_cluts[i]);
                            }

                            // Store map entity CLUTs in MIPS RAM
                            for (int i = 0; i < map.entity_cluts.size(); i++) {

                                // Get the current entity CLUT data
                                ClutEntry clut = map.entity_cluts[i];

                                // Store the CLUT in RAM
                                MipsEmulator::StoreMapCLUT(clut.offset, clut.count, clut.clut_data);
                            }

                            // Load the map entities
                            map.LoadMapEntities();

                            // Clear out any errors
                            error.clear();
                        }
                        else {
                            error = "Could not find graphics file (F_" + map_filename + ").";
                        }
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save", "Ctrl+S", false, false)) {
                    // Disabled for now
                }
                if (ImGui::MenuItem("Save As..", nullptr, false, false)) {
                    // Disabled for now
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Quit", "Alt+F4")) {
                    exit = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }





// -- Game Binary & Generic Texture Loading --------------------------------------------------------------------

        // Check if settings were loaded
        if (psx_path == nullptr || strcmp(psx_path, "(null)") == 0) {
            ImGui::OpenPopup("Select PSX file");
        }
        else if (bin_path == nullptr || strcmp(bin_path, "(null)") == 0) {
            ImGui::OpenPopup("Select DRA.BIN file");
        }
        else if (gfx_path == nullptr || strcmp(gfx_path, "(null)") == 0) {
            ImGui::OpenPopup("Select F_GAME.BIN file");
        }

        // Get the center of the viewport
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGuiWindowFlags modal_flags = ImGuiWindowFlags_AlwaysAutoResize;

        // Center the PSX prompt
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // Prompt user to select the DRA.BIN file for binary loading
        if (ImGui::BeginPopupModal("Select PSX file", nullptr, modal_flags)) {
            if (!error.empty()) {
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s", error.c_str());
            }
            ImGui::Text("PSX file path was not found.");
            ImGui::Text("Press \"Open...\" to select the PSX file.");
            ImGui::NewLine();
            ImGui::Text("Note: This may be a file named");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(255, 255, 0, 255), "SLUS_000.67\n\n");
            ImGui::Separator();

            if (ImGui::Button("Open...", ImVec2(120, 0))) {
                nfdfilteritem_t filters[1] = {
                    {
                        "PSX file",
                        "exe,EXE,67"
                    }
                };
                nfdchar_t* out_path = open_file(filters);
                if (out_path != nullptr) {
                    std::string filename = std::filesystem::path(out_path).filename().string();
                    if (!filename.empty()) {
                        psx_path = out_path;
                        MipsEmulator::SetPSXBinary(psx_path);
                        error.clear();
                    }
                    else {
                        error = "PSX file was not selected.";
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                exit = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Center the DRA.BIN prompt
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // Prompt user to select the DRA.BIN file for binary loading
        if (ImGui::BeginPopupModal("Select DRA.BIN file", nullptr, modal_flags)) {
            if (!error.empty()) {
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s", error.c_str());
            }
            ImGui::Text("DRA.BIN file path was not found.\nPress \"Open...\" to select the DRA.BIN file.\n\n");
            ImGui::Separator();

            if (ImGui::Button("Open...", ImVec2(120, 0))) {
                nfdfilteritem_t filters[1] = {
                    {
                        "DRA.BIN file",
                        "bin,BIN"
                    }
                };
                nfdchar_t* out_path = open_file(filters);
                if (out_path != nullptr) {
                    std::string filename = std::filesystem::path(out_path).filename().string();
                    if (filename == "DRA.BIN") {
                        bin_path = out_path;
                        MipsEmulator::SetSotNBinary(bin_path);
                        error.clear();
                    }
                    else {
                        error = "DRA.BIN was not selected.";
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                exit = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Center the F_GAME.BIN prompt
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // Prompt user to select the F_GAME.BIN file for graphics loading
        if (ImGui::BeginPopupModal("Select F_GAME.BIN file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!error.empty()) {
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s", error.c_str());
            }
            ImGui::Text("F_GAME.BIN file path was not found.\nPress \"Open...\" to select the F_GAME.BIN file.\n\n");
            ImGui::Separator();

            if (ImGui::Button("Open...", ImVec2(120, 0))) {
                nfdfilteritem_t filters[1] = {
                    {
                        "F_GAME.BIN file",
                        "bin,BIN"
                    }
                };
                nfdchar_t* out_path = open_file(filters);
                if (out_path != nullptr) {
                    std::string filename = std::filesystem::path(out_path).filename().string();
                    if (filename == "F_GAME.BIN") {
                        gfx_path = out_path;
                        error.clear();
                    }
                    else {
                        error = "F_GAME.BIN was not selected.";
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                exit = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }





// -- Dock Space -----------------------------------------------------------------------------------------------

        // Set the window properties according to the viewport
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);

        // Docking window flags
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;

        // Make windows square with no border
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        // Disable window padding for the main DockSpace window
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Main DockSpace", &open, window_flags);
        ImGui::PopStyleVar(3);

        if (ImGui::DockBuilderGetNode(ImGui::GetID("DockSpace")) == nullptr) {
            ImGuiID dockspace_id = ImGui::GetID("DockSpace");
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            // Reset layout
            ImGui::DockBuilderRemoveNode(dockspace_id);

            // Add an empty node
            ImGui::DockBuilderAddNode(dockspace_id);

            // Main viewport (takes up available space)
            ImGuiID dock_main = dockspace_id;

            // Dock the global items list to the left
            ImGuiID dock_global_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, NULL, &dock_main);

            // Attach the properties to the side of the global items list
            ImGuiID dock_props_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, NULL, &dock_main);

            ImGui::DockBuilderDockWindow("Global Items", dock_global_left);
            ImGui::DockBuilderDockWindow("Properties", dock_props_left);
            ImGui::DockBuilderDockWindow("Main Viewport", dock_main);
            ImGui::DockBuilderDockWindow("VRAM Viewer", dock_main);
            ImGui::DockBuilderDockWindow("Tileset Editor", dock_main);
            ImGui::DockBuilderDockWindow("Sprite Editor", dock_main);
            ImGui::DockBuilderDockWindow("Enemy Editor", dock_main);
            ImGui::DockBuilderDockWindow("Room Editor", dock_main);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGuiID dockspace_id = ImGui::GetID("DockSpace");
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);








        static bool show_window = true;
        static bool use_work_area = true;
        static ImGuiWindowFlags flags = (
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse
        );


        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0, 4.0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 50, 50, 0));





// -- Properties -----------------------------------------------------------------------------------------------

        //ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        if (ImGui::Begin("Properties")) {

            if (!map.map_id.empty()) {

                // Show map ID
                ImGui::Text("Map ID: ");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(255, 255, 0, 255), "%s", map.map_id.c_str());

                // Show number of rooms
                ImGui::Text("Rooms: %lu", map.rooms.size());

                // Show entity properties if one is selected
                if (selected_entity != NULL) {
                    ImGui::Separator();
                    if (!selected_entity->name.empty()) {
                        ImGui::Text("Entity Name: %s", selected_entity->name.c_str());
                    }
                    if (!selected_entity->desc.empty()) {

                        ImGui::Text("Entity Desc: %s", selected_entity->desc.c_str());
                    }
                    ImGui::Text("Entity ID: %04X", selected_entity->id);
                    ImGui::Text("Entity Slot: %04X", selected_entity->slot);
                    ImGui::Text("Sprite Address: %08X", selected_entity->sprite_address + MAP_BIN_OFFSET);
                    static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ContextMenuInBody;
                    if (ImGui::BeginTable("Entity Data", 5, flags)) {
                        uint cur_offset = 0;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("pos_x_accel");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.pos_x_accel);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.pos_x_accel);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("pos_x");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.pos_x);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.pos_x);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("pos_y_accel");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.pos_y_accel);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.pos_y_accel);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("pos_y");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.pos_y);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.pos_y);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("acceleration_x");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.acceleration_x);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.acceleration_x);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("acceleration_y");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.acceleration_y);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.acceleration_y);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("offset_x");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.offset_x);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.offset_x);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("offset_y");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.offset_y);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.offset_y);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("facing");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.facing);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.facing);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("clut_index");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.clut_index);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.clut_index);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("blend_mode");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.blend_mode);
                        ImGui::TableNextColumn(); ImGui::Text("%hhd", selected_entity->data.blend_mode);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("color_flags");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.color_flags);
                        ImGui::TableNextColumn(); ImGui::Text("%hhd", selected_entity->data.color_flags);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("fx_width");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.fx_width);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.fx_width);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("fx_height");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.fx_height);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.fx_height);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("fx_timer");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.fx_timer);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.fx_timer);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("fx_coord_x");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.fx_coord_x);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.fx_coord_x);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("fx_coord_y");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.fx_coord_y);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.fx_coord_y);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("z_depth");
                        ImGui::TableNextColumn(); ImGui::Text("%04hX", selected_entity->data.z_depth);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.z_depth);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("object_id");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.object_id);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.object_id);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("update_function");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.update_function);
                        ImGui::TableNextColumn(); ImGui::Text("%u", selected_entity->data.update_function);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("current_state");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.current_state);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.current_state);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("current_substate");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.current_substate);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.current_substate);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("initial_state");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.initial_state);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.initial_state);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("room_slot");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.room_slot);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.room_slot);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk34");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk34);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk34);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk38");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk38);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk38);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("info_idx");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.info_idx);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.info_idx);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk3C");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk3C);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk3C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("hit_points");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.hit_points);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.hit_points);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("attack_damage");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.attack_damage);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.attack_damage);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("damage_type");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.damage_type);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.damage_type);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk44");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk44);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.unk44);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("hitbox_width");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.hitbox_width);
                        ImGui::TableNextColumn(); ImGui::Text("%hhu", selected_entity->data.hitbox_width);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("hitbox_height");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.hitbox_height);
                        ImGui::TableNextColumn(); ImGui::Text("%hhu", selected_entity->data.hitbox_height);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("unk48");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.unk48);
                        ImGui::TableNextColumn(); ImGui::Text("%hhu", selected_entity->data.unk48);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("unk49");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.unk49);
                        ImGui::TableNextColumn(); ImGui::Text("%hhu", selected_entity->data.unk49);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk4A");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk4A);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk4A);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk4C");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk4C);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk4C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("frame_index");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.frame_index);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.frame_index);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("frame_duration");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.frame_duration);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.frame_duration);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("sprite_bank");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.sprite_bank);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.sprite_bank);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("sprite_image");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.sprite_image);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.sprite_image);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk58");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk58);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk58);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("tileset");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.tileset);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.tileset);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk5C");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk5C);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk5C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk60");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk60);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk60);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("polygt4_id");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.polygt4_id);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.polygt4_id);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk68");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk68);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk68);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk6A");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk6A);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk6A);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("unk6C");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.unk6C);
                        ImGui::TableNextColumn(); ImGui::Text("%hhu", selected_entity->data.unk6C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 1;
                        ImGui::TableNextColumn(); ImGui::Text("unk6D");
                        ImGui::TableNextColumn(); ImGui::Text("%02X", selected_entity->data.unk6D);
                        ImGui::TableNextColumn(); ImGui::Text("%hhd", selected_entity->data.unk6D);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk6E");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk6E);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk6E);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk70");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk70);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk70);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk74");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk74);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk74);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk78");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk78);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk78);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk7C");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk7C);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk7C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk80");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk80);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk80);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk82");
                        ImGui::TableNextColumn(); ImGui::Text("%04hX", selected_entity->data.unk82);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk82);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk84");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk84);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk84);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk86");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk86);
                        ImGui::TableNextColumn(); ImGui::Text("%hd", selected_entity->data.unk86);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk88");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk88);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.unk88);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk8A");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk8A);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.unk8A);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk8C");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk8C);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.unk8C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 2;
                        ImGui::TableNextColumn(); ImGui::Text("unk8E");
                        ImGui::TableNextColumn(); ImGui::Text("%04X", selected_entity->data.unk8E);
                        ImGui::TableNextColumn(); ImGui::Text("%hu", selected_entity->data.unk8E);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk90");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk90);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk90);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk94");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk94);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk94);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk98");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk98);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk98);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unk9C");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unk9C);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unk9C);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unkA0");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unkA0);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unkA0);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unkA4");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unkA4);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unkA4);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unkA8");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unkA8);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unkA8);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unkAC");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unkAC);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unkAC);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("unkB0");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.unkB0);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.unkB0);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("pickup_flag");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.pickup_flag);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.pickup_flag);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", cur_offset);
                        ImGui::TableNextColumn(); ImGui::Text("%02X", cur_offset); cur_offset += 4;
                        ImGui::TableNextColumn(); ImGui::Text("callback");
                        ImGui::TableNextColumn(); ImGui::Text("%08X", selected_entity->data.callback);
                        ImGui::TableNextColumn(); ImGui::Text("%d", selected_entity->data.callback);

                    }
                    ImGui::EndTable();
                }

                // No entity was selected
                else {
                    ImGui::Text("Select an entity to view its data.");
                }
            }

            // Map file hasn't been loaded yet
            else {
                ImGui::Text("Map file not loaded.");
            }
        }
        ImGui::End();





// -- Main Viewport --------------------------------------------------------------------------------------------

        if (ImGui::Begin("Main Viewport")) {

            if (!map.map_id.empty()) {

                // No padding for main scrolling viewport
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                // Current drawing offset
                static ImVec2 offset(0.0f, 0.0f);

                // Create a child window
                if (ImGui::BeginChild("MainViewportDisplay", ImVec2(0, 0), true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

                    // Create an invisible button for the canvas
                    ImGui::PushID(0);
                    ImGui::InvisibleButton("##MainCanvas", ImGui::GetWindowSize());
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        offset.x += ImGui::GetIO().MouseDelta.x;
                        offset.y += ImGui::GetIO().MouseDelta.y;
                    }

                    // Update offsets with scroll
                    offset.x += io.MouseWheelH * 2;
                    offset.y += io.MouseWheel * 2;

                    ImGui::PopID();



                    // Get the min/max coords of the invisible button
                    const ImVec2 p0 = ImGui::GetItemRectMin();
                    const ImVec2 p1 = ImGui::GetItemRectMax();



                    // Allow buttons to be clicked within the InvisibleButton
                    ImGui::SetItemAllowOverlap();

                    // Force all items within the area to be clipped
                    ImGui::PushClipRect(p0, p1, true);

                    // Get the cursor position of the cursor
                    ImVec2 bak_pos = ImGui::GetCursorPos();

                    // Create an image at 0,0
                    ImGui::SetCursorPos(ImVec2(offset.x, offset.y));






                    // Minimum starting coordinates
                    uint x_min = 42069;
                    uint y_min = 42069;
                    uint x_max = 0;
                    uint y_max = 0;

                    // Unique entity identifier
                    uint entity_uuid = 0;

                    // Loop through each room
                    for (int i = 0; i < map.rooms.size(); i++) {

                        // Check if this was lower than the current minimum X/Y coords
                        if (map.rooms[i].x_start < x_min) {
                            x_min = map.rooms[i].x_start;
                        }
                        if (map.rooms[i].y_start < y_min) {
                            y_min = map.rooms[i].y_start;
                        }
                        if (map.rooms[i].x_start + map.rooms[i].width > x_max) {
                            x_max = map.rooms[i].x_start + map.rooms[i].width;
                        }
                        if (map.rooms[i].y_start + map.rooms[i].height > y_max) {
                            y_max = map.rooms[i].y_start + map.rooms[i].height;
                        }
                    }

                    // Map dimensions
                    uint map_width = (x_max - x_min) * 256;
                    uint map_height = (y_max - y_min) * 256;



                    ImGuiIO& io = ImGui::GetIO();
                    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Equal)) {
                        float x_ratio = offset.x / (map_width * zoom_amount);
                        float y_ratio = offset.y / (map_height * zoom_amount);
                        zoom_amount *= 2;
                        offset.x = x_ratio * (map_width * zoom_amount) - (ImGui::GetWindowSize().x / 2);
                        offset.y = y_ratio * (map_height * zoom_amount) - (ImGui::GetWindowSize().y / 2);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
                        // Get the current X/Y ratio
                        float x_ratio = offset.x / (map_width * zoom_amount);
                        float y_ratio = offset.y / (map_height * zoom_amount);
                        zoom_amount /= 2;
                        offset.x = x_ratio * (map_width * zoom_amount) + (ImGui::GetWindowSize().x / 4);
                        offset.y = y_ratio * (map_height * zoom_amount) + (ImGui::GetWindowSize().y / 4);
                    }

                    // Don't space out the items
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));





                    // Draw backgrounds
                    for (int i = 0; i < map.rooms.size(); i++) {
                        Room* cur_room = &map.rooms[i];

                        if (cur_room->load_flag == 0xFF) {
                            continue;
                        }

                        ImGui::SetCursorPos(ImVec2(offset.x, offset.y));

                        uint x_coord = (cur_room->x_start - x_min) * 256 * zoom_amount;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * zoom_amount;

                        for (auto const& layer : cur_room->bg_ordering_table) {

                            // Get layer data
                            uint id = layer.first;
                            std::vector<EntitySpritePart> sprites = layer.second;

                            // Loop through each sprite in the current layer
                            for (int p = 0; p < sprites.size(); p++) {

                                // Get the current sprite
                                EntitySpritePart* cur_sprite = &sprites[p];

                                // Get the X/Y coords
                                float sprite_x = offset.x + x_coord + (cur_sprite->x * zoom_amount);
                                float sprite_y = offset.y + y_coord + (cur_sprite->y * zoom_amount);

                                // Set the cursor position to the target item
                                ImGui::SetCursorPos(ImVec2(sprite_x, sprite_y));

                                // Set default UV coords
                                ImVec2 uv0 = ImVec2(cur_sprite->flip_x, cur_sprite->flip_y);
                                ImVec2 uv1 = ImVec2(!cur_sprite->flip_x, !cur_sprite->flip_y);

                                ImDrawList* draw_list = ImGui::GetWindowDrawList();

                                // Check if the sprite should be blended
                                if (cur_sprite->blend) {
                                    if ((cur_sprite->blend_mode & 0x70) == 0x70) {
                                        draw_list->AddCallback(blend_fade_light, nullptr);
                                    }

                                    // Check if lighten blend mode should be applied
                                    else if ((cur_sprite->blend_mode & 0x20) == 0x20) {
                                        draw_list->AddCallback(blend_lighten, nullptr);
                                    }
                                }

                                float tmp_width = (float)cur_sprite->width * zoom_amount;
                                float tmp_height = (float)cur_sprite->height * zoom_amount;

                                // Draw the image
                                ImGui::Image(
                                        (void*)(intptr_t)cur_sprite->texture,
                                        ImVec2((float)cur_sprite->width * zoom_amount, (float)cur_sprite->height * zoom_amount),
                                        uv0, uv1
                                );
                                draw_list->AddCallback(blend_default, nullptr);
                            }
                        }
                        // Draw the actual background on top of everything below it
                        ImGui::SetCursorPos(ImVec2(offset.x + (float)x_coord, offset.y + (float)y_coord));
                        ImGui::Image((void*)(intptr_t)cur_room->bg_texture, ImVec2(cur_room->bg_layer.width * 16 * zoom_amount, cur_room->bg_layer.height * 16 * zoom_amount));
                    }

                    // Draw entities in between FG and BG
                    for (int i = 0; i < map.rooms.size(); i++) {
                        Room* cur_room = &map.rooms[i];

                        if (cur_room->load_flag == 0xFF) {
                            continue;
                        }

                        ImGui::SetCursorPos(ImVec2(offset.x, offset.y));

                        uint x_coord = (cur_room->x_start - x_min) * 256 * zoom_amount;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * zoom_amount;

                        for (auto const& layer : cur_room->mid_ordering_table) {

                            // Get layer data
                            uint id = layer.first;
                            std::vector<EntitySpritePart> sprites = layer.second;

                            // Loop through each sprite in the current layer
                            for (int p = 0; p < sprites.size(); p++) {

                                // Get the current sprite
                                EntitySpritePart* cur_sprite = &sprites[p];

                                // Get the X/Y coords
                                float sprite_x = offset.x + x_coord + (cur_sprite->x * zoom_amount);
                                float sprite_y = offset.y + y_coord + (cur_sprite->y * zoom_amount);

                                // Set the cursor position to the target item
                                ImGui::SetCursorPos(ImVec2(sprite_x, sprite_y));

                                // Set default UV coords
                                ImVec2 uv0 = ImVec2(cur_sprite->flip_x, cur_sprite->flip_y);
                                ImVec2 uv1 = ImVec2(!cur_sprite->flip_x, !cur_sprite->flip_y);

                                ImDrawList* draw_list = ImGui::GetWindowDrawList();

                                // Check if the sprite should be blended
                                if (cur_sprite->blend) {
                                    if ((cur_sprite->blend_mode & 0x70) == 0x70) {
                                        draw_list->AddCallback(blend_fade_light, nullptr);
                                    }

                                    // Check if lighten blend mode should be applied
                                    else if ((cur_sprite->blend_mode & 0x20) == 0x20) {
                                        draw_list->AddCallback(blend_lighten, nullptr);
                                    }
                                }

                                float tmp_width = (float)cur_sprite->width * zoom_amount;
                                float tmp_height = (float)cur_sprite->height * zoom_amount;

                                // Check if image had a polygon
                                if (cur_sprite->polygon.code > 0) {

                                    // Get the polygon
                                    POLY_GT4* polygon = &cur_sprite->polygon;

                                    // Get the current vertex index
                                    vtx_idx = ImGui::GetWindowDrawList()->VtxBuffer.Size;

                                    // Check if shaded polygon should be drawn instead of image
                                    if ((polygon->code & 0xF) == 3) {

                                        ImVec2 clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
                                        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                                            ImVec2(sprite_x + clip_min.x, sprite_y + clip_min.y),
                                            ImVec2(sprite_x + (float)cur_sprite->width * zoom_amount + clip_min.x, sprite_y + (float)cur_sprite->height * zoom_amount + clip_min.y),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255)
                                        );
                                    }
                                    else {
                                        ImGui::Image(
                                            (void*)(intptr_t)cur_sprite->texture,
                                            ImVec2((float)cur_sprite->width * zoom_amount, (float)cur_sprite->height * zoom_amount),
                                            uv0, uv1
                                        );
                                    }

                                    auto& buf = ImGui::GetWindowDrawList()->VtxBuffer;
                                    if (vtx_idx < buf.Size) {
                                        auto test1 = buf[vtx_idx + 0];
                                        auto test2 = buf[vtx_idx + 1];
                                        auto test3 = buf[vtx_idx + 2];
                                        auto test4 = buf[vtx_idx + 3];



                                        // Check whether polygon should be opaque or semi-transparent
                                        uint alpha = 0xFF;
                                        if ((polygon->pad3 & 1) != 0) {
                                            alpha = 0x80;
                                        }

                                        buf[vtx_idx + 0].col = IM_COL32(polygon->r0*2-1, polygon->g0*2-1, polygon->b0*2-1, alpha);
                                        buf[vtx_idx + 1].col = IM_COL32(polygon->r1*2-1, polygon->g1*2-1, polygon->b1*2-1, alpha);
                                        buf[vtx_idx + 2].col = IM_COL32(polygon->r3*2-1, polygon->g3*2-1, polygon->b3*2-1, alpha);
                                        buf[vtx_idx + 3].col = IM_COL32(polygon->r2*2-1, polygon->g2*2-1, polygon->b2*2-1, alpha);

                                        // Check if sprite is skewed
                                        if (cur_sprite->skew) {
                                            if (cur_sprite->flip_x) {
                                                float tmp_x = buf[vtx_idx + 0].pos.x;
                                                buf[vtx_idx + 0].pos.x = buf[vtx_idx + 1].pos.x;
                                                buf[vtx_idx + 1].pos.x = tmp_x;
                                                tmp_x = buf[vtx_idx + 2].pos.x;
                                                buf[vtx_idx + 2].pos.x = buf[vtx_idx + 3].pos.x;
                                                buf[vtx_idx + 3].pos.x = tmp_x;
                                            }
                                            if (cur_sprite->flip_y) {
                                                float tmp_y = buf[vtx_idx + 0].pos.y;
                                                buf[vtx_idx + 0].pos.y = buf[vtx_idx + 2].pos.y;
                                                buf[vtx_idx + 2].pos.y = tmp_y;
                                                tmp_y = buf[vtx_idx + 2].pos.y;
                                                buf[vtx_idx + 1].pos.y = buf[vtx_idx + 1].pos.y;
                                                buf[vtx_idx + 3].pos.y = tmp_y;
                                            }
                                            buf[vtx_idx + 0].pos.x += (cur_sprite->top_left.x * zoom_amount);
                                            buf[vtx_idx + 0].pos.y += (cur_sprite->top_left.y * zoom_amount);
                                            buf[vtx_idx + 1].pos.x += (cur_sprite->top_right.x * zoom_amount);
                                            buf[vtx_idx + 1].pos.y += (cur_sprite->top_right.y * zoom_amount);
                                            buf[vtx_idx + 2].pos.x += (cur_sprite->bottom_right.x * zoom_amount);
                                            buf[vtx_idx + 2].pos.y += (cur_sprite->bottom_right.y * zoom_amount);
                                            buf[vtx_idx + 3].pos.x += (cur_sprite->bottom_left.x * zoom_amount);
                                            buf[vtx_idx + 3].pos.y += (cur_sprite->bottom_left.y * zoom_amount);
                                        }
                                    }
                                }
                                else {
                                    ImGui::Image(
                                            (void*)(intptr_t)cur_sprite->texture,
                                            ImVec2((float)cur_sprite->width * zoom_amount, (float)cur_sprite->height * zoom_amount),
                                            uv0, uv1
                                    );
                                }
                                draw_list->AddCallback(blend_default, nullptr);
                            }
                        }
                    }

                    // Draw foregrounds
                    for (int i = 0; i < map.rooms.size(); i++) {
                        Room* cur_room = &map.rooms[i];

                        if (cur_room->load_flag == 0xFF) {
                            continue;
                        }

                        ImGui::SetCursorPos(ImVec2(offset.x, offset.y));

                        uint x_coord = (cur_room->x_start - x_min) * 256 * zoom_amount;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * zoom_amount;

                        ImGui::SetCursorPos(ImVec2(offset.x + (float)x_coord, offset.y + (float)y_coord));
                        ImGui::Image((void*)(intptr_t)cur_room->fg_texture, ImVec2(cur_room->fg_layer.width * 16 * zoom_amount, cur_room->fg_layer.height * 16 * zoom_amount));

                        for (auto const& layer : cur_room->fg_ordering_table) {

                            // Get layer data
                            uint id = layer.first;
                            std::vector<EntitySpritePart> sprites = layer.second;

                            // Loop through each sprite in the current layer
                            for (int p = 0; p < sprites.size(); p++) {

                                // Get the current sprite
                                EntitySpritePart* cur_sprite = &sprites[p];

                                // Get the X/Y coords
                                float sprite_x = offset.x + x_coord + (cur_sprite->x * zoom_amount);
                                float sprite_y = offset.y + y_coord + (cur_sprite->y * zoom_amount);

                                // Set the cursor position to the target item
                                ImGui::SetCursorPos(ImVec2(sprite_x, sprite_y));

                                // Set default UV coords
                                ImVec2 uv0 = ImVec2(cur_sprite->flip_x, cur_sprite->flip_y);
                                ImVec2 uv1 = ImVec2(!cur_sprite->flip_x, !cur_sprite->flip_y);

                                ImDrawList* draw_list = ImGui::GetWindowDrawList();

                                // Check if the sprite should be blended
                                if (cur_sprite->blend) {
                                    if ((cur_sprite->blend_mode & 0x70) == 0x70) {
                                        draw_list->AddCallback(blend_fade_light, nullptr);
                                    }

                                    // Check if lighten blend mode should be applied
                                    else if ((cur_sprite->blend_mode & 0x20) == 0x20) {
                                        draw_list->AddCallback(blend_lighten, nullptr);
                                    }
                                }

                                float tmp_width = (float)cur_sprite->width * zoom_amount;
                                float tmp_height = (float)cur_sprite->height * zoom_amount;

                                // Check if image had a polygon
                                if (cur_sprite->polygon.code > 0) {

                                    // Get the polygon
                                    POLY_GT4* polygon = &cur_sprite->polygon;

                                    // Get the current vertex index
                                    vtx_idx = ImGui::GetWindowDrawList()->VtxBuffer.Size;

                                    // Check if shaded polygon should be drawn instead of image
                                    if ((polygon->code & 0xF) == 3) {

                                        ImVec2 clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
                                        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                                                ImVec2(sprite_x + clip_min.x, sprite_y + clip_min.y),
                                                ImVec2(sprite_x + (float)cur_sprite->width * zoom_amount + clip_min.x, sprite_y + (float)cur_sprite->height * zoom_amount + clip_min.y),
                                                IM_COL32(255, 255, 255, 255),
                                                IM_COL32(255, 255, 255, 255),
                                                IM_COL32(255, 255, 255, 255),
                                                IM_COL32(255, 255, 255, 255)
                                        );
                                    }
                                    else {
                                        ImGui::Image(
                                                (void*)(intptr_t)cur_sprite->texture,
                                                ImVec2((float)cur_sprite->width * zoom_amount, (float)cur_sprite->height * zoom_amount),
                                                uv0, uv1
                                        );
                                    }

                                    auto& buf = ImGui::GetWindowDrawList()->VtxBuffer;
                                    if (vtx_idx < buf.Size) {
                                        auto test1 = buf[vtx_idx + 0];
                                        auto test2 = buf[vtx_idx + 1];
                                        auto test3 = buf[vtx_idx + 2];
                                        auto test4 = buf[vtx_idx + 3];



                                        // Check whether polygon should be opaque or semi-transparent
                                        uint alpha = 0xFF;
                                        if ((polygon->pad3 & 1) != 0) {
                                            alpha = 0x80;
                                        }

                                        buf[vtx_idx + 0].col = IM_COL32(polygon->r0*2-1, polygon->g0*2-1, polygon->b0*2-1, alpha);
                                        buf[vtx_idx + 1].col = IM_COL32(polygon->r1*2-1, polygon->g1*2-1, polygon->b1*2-1, alpha);
                                        buf[vtx_idx + 2].col = IM_COL32(polygon->r3*2-1, polygon->g3*2-1, polygon->b3*2-1, alpha);
                                        buf[vtx_idx + 3].col = IM_COL32(polygon->r2*2-1, polygon->g2*2-1, polygon->b2*2-1, alpha);

                                        // Check if sprite is skewed
                                        if (cur_sprite->skew) {
                                            if (cur_sprite->flip_x) {
                                                float tmp_x = buf[vtx_idx + 0].pos.x;
                                                buf[vtx_idx + 0].pos.x = buf[vtx_idx + 1].pos.x;
                                                buf[vtx_idx + 1].pos.x = tmp_x;
                                                tmp_x = buf[vtx_idx + 2].pos.x;
                                                buf[vtx_idx + 2].pos.x = buf[vtx_idx + 3].pos.x;
                                                buf[vtx_idx + 3].pos.x = tmp_x;
                                            }
                                            if (cur_sprite->flip_y) {
                                                float tmp_y = buf[vtx_idx + 0].pos.y;
                                                buf[vtx_idx + 0].pos.y = buf[vtx_idx + 2].pos.y;
                                                buf[vtx_idx + 2].pos.y = tmp_y;
                                                tmp_y = buf[vtx_idx + 2].pos.y;
                                                buf[vtx_idx + 1].pos.y = buf[vtx_idx + 1].pos.y;
                                                buf[vtx_idx + 3].pos.y = tmp_y;
                                            }
                                            buf[vtx_idx + 0].pos.x += (cur_sprite->top_left.x * zoom_amount);
                                            buf[vtx_idx + 0].pos.y += (cur_sprite->top_left.y * zoom_amount);
                                            buf[vtx_idx + 1].pos.x += (cur_sprite->top_right.x * zoom_amount);
                                            buf[vtx_idx + 1].pos.y += (cur_sprite->top_right.y * zoom_amount);
                                            buf[vtx_idx + 2].pos.x += (cur_sprite->bottom_right.x * zoom_amount);
                                            buf[vtx_idx + 2].pos.y += (cur_sprite->bottom_right.y * zoom_amount);
                                            buf[vtx_idx + 3].pos.x += (cur_sprite->bottom_left.x * zoom_amount);
                                            buf[vtx_idx + 3].pos.y += (cur_sprite->bottom_left.y * zoom_amount);
                                        }
                                    }
                                }
                                else {
                                    ImGui::Image(
                                            (void*)(intptr_t)cur_sprite->texture,
                                            ImVec2((float)cur_sprite->width * zoom_amount, (float)cur_sprite->height * zoom_amount),
                                            uv0, uv1
                                    );
                                }
                                draw_list->AddCallback(blend_default, nullptr);
                            }
                        }
                    }






                    ImGui::PopStyleVar();



                    // Loop through each room to draw the entity stuff
                    for (int i = 0; i < map.rooms.size(); i++) {

                        Room* cur_room = &map.rooms[i];

                        // Skip if room is a transition room
                        if (cur_room->load_flag == 0xFF) {
                            continue;
                        }

                        // Get the current room's starting X/Y coords
                        uint x_coord = (cur_room->x_start - x_min) * 256 * zoom_amount;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * zoom_amount;

                        // Use a magenta outline for the entity buttons
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 0, 255, 255));

                        // Make the button have only an outline
                        ImGuiColorEditFlags color_flags = (
                                ImGuiColorEditFlags_AlphaPreview |
                                ImGuiColorEditFlags_NoTooltip |
                                ImGuiColorEditFlags_NoDragDrop |
                                ImGuiColorEditFlags_NoCheckerboard
                        );

                        // Get the entity init data
                        uint entity_layout_id = cur_room->entity_layout_id;
                        std::vector<EntityInitData> init_data_list = map.entity_layouts[entity_layout_id];

                        // Draw the room name
                        ImGui::SetCursorPos(ImVec2(offset.x + x_coord, offset.y + y_coord));
                        ImGui::Text("Room %d", i);





                        // Loop through each entity
                        for (int k = 0; k < cur_room->entities.size(); k++) {

                            // Get the current entity
                            Entity* entity = &cur_room->entities[k];

                            // Check if entity has a hitbox size
                            float button_width = (entity->data.hitbox_width > 0 ? (float)entity->data.hitbox_width : 16.0f) * zoom_amount;
                            float button_height = (entity->data.hitbox_height > 0 ? (float)entity->data.hitbox_height : 16.0f) * zoom_amount;

                            // Set the cursor position to the target item
                            ImGui::SetCursorPos(
                                ImVec2(
                                    offset.x + x_coord + (entity->data.pos_x * zoom_amount) - (button_width / 2),
                                    offset.y + y_coord + (entity->data.pos_y * zoom_amount) - (button_height / 2)
                                )
                            );

                            // Draw an outline around the entity
                            ImGui::PushID(entity_uuid);
                            if (ImGui::ColorButton("##test", ImVec4(0, 0, 0, 0), color_flags, ImVec2(button_width, button_height))) {
                                printf("Clicked entity %d in room %d\n", k, i);
                                selected_entity = &map.rooms[i].entities[k];
                            }
                            ImGui::PopID();
                            entity_uuid++;
                        }

                        ImGui::PopStyleColor();
                    }


                    ImGui::SetCursorPos(ImVec2(0, ImGui::GetWindowSize().y - 14));
                    ImGui::Text("Zoom: %0.2f%%", zoom_amount * 100);

                    // Stop clipping area
                    ImGui::PopClipRect();

                    // End the child
                    ImGui::EndChild();
                }

                // Pop window padding
                ImGui::PopStyleVar();
            }

            // Map file hasn't been loaded yet
            else {
                ImGui::Text("Map file not loaded.");
            }
        }
        ImGui::End();





// -- VRAM -----------------------------------------------------------------------------------------------------

        if (ImGui::Begin("VRAM Viewer")) {

            if (!map.map_id.empty()) {

                // No padding for main scrolling viewport
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                // Current drawing offset
                static ImVec2 offset(0.0f, 0.0f);

                // Create a child window
                if (ImGui::BeginChild("MainVRAMDisplay", ImVec2(0, 0), true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar)) {

                    // Create an invisible button for the canvas
                    ImGui::PushID(0);
                    ImGui::InvisibleButton("##VRAMCanvas", ImGui::GetWindowSize());
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        offset.x += ImGui::GetIO().MouseDelta.x;
                        offset.y += ImGui::GetIO().MouseDelta.y;
                    }
                    ImGui::PopID();



                    // Get the min/max coords of the invisible button
                    const ImVec2 p0 = ImGui::GetItemRectMin();
                    const ImVec2 p1 = ImGui::GetItemRectMax();



                    // Allow buttons to be clicked within the InvisibleButton
                    ImGui::SetItemAllowOverlap();

                    // Force all items within the area to be clipped
                    ImGui::PushClipRect(p0, p1, true);

                    // Get the current position of the cursor
                    ImVec2 cur_pos = ImGui::GetCursorPos();

                    // Create an image at 0,0
                    ImGui::SetCursorPos(ImVec2(offset.x, offset.y));






                    ImVec2 cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y + 5));
                    ImGui::Text("Map VRAM:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)map.map_vram, ImVec2(512, 256));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y + 5));
                    ImGui::Text("Generic CLUTs:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)generic_cluts_texture, ImVec2(256, 16));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y + 5));
                    ImGui::Text("Tileset 7:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)map.map_tilesets[7], ImVec2(64, 256));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)fgame_texture, ImVec2(512, 256));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)fgame_textures[7], ImVec2(64, 256));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)MipsEmulator::framebuffer, ImVec2(1024, 512));


                    // Restore position
                    ImGui::SetCursorPos(cur_pos);

                    // Stop clipping area
                    ImGui::PopClipRect();

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                }
            }

            // Map file hasn't been loaded yet
            else {
                ImGui::Text("Map file not loaded.");
            }

            //ImGui::Image((void*)(intptr_t)map.map_textures[(tileset * 4) + tex_x + (tex_y * 2)], ImVec2(32, 128));
        }
        ImGui::End();





// -- Generic Error Popup --------------------------------------------------------------------------------------

        if (!error.empty() && !ImGui::IsPopupOpen("any", ImGuiPopupFlags_AnyPopupId)) {
            ImGui::OpenPopup("Error");
        }

        // Display error message
        if (ImGui::BeginPopupModal("Error", nullptr, modal_flags)) {
            ImGui::Text("SotN Editor has encountered an error.\n\nDetails:");
            ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s\n\n", error.c_str());
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                error.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }





// -- Cleanup --------------------------------------------------------------------------------------------------

        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // End DockSpace
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    NFD_Quit();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
