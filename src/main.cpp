#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <thread>

// Include this for MSVC since it doesn't like M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
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
#include "log.h"


#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    Log::Error("Glfw Error %d: %s\n", error, description);
}





// Paths for DRA.BIN and F_GAME.BIN
static char* psx_path;
static char* bin_path;
static char* gfx_path;
Map map;

// Any errors that might appear
std::string error;

// Popup flags
enum POPUP_FLAGS {
    PopupFlag_None       =      0,
    PopupFlag_HasOK      = 1 << 0,      // Shows an "OK" button to dismiss the popup
    PopupFlag_Ephemeral  = 1 << 1,      // Used to temporarily display a status while a function is executing
    PopupFlag_Close      = 1 << 2       // Flags the popup to automatically close
};

// Popup status
enum POPUP_STATUS {
    PopupStatus_Init,                   // Initialize the popup to handle a callback
    PopupStatus_Processing,             // Popup is currently processing a callback
    PopupStatus_Finished                // Callback has finished running or doesn't exist
};

// Popup struct
struct popup {
    std::string text;
    POPUP_FLAGS flags = PopupFlag_None;
    POPUP_STATUS status = PopupStatus_Finished;
    std::function<void()> callback;
} popup;

// Viewport struct type
typedef struct Viewport {
    ImVec2 camera = ImVec2(0.0f, 0.0f);    // Drawing offset
    float zoom = 1.0f;                           // Zoom amount
} Viewport;

// Vertex index
static int vtx_idx;

// Main window
GLFWwindow* window;
GLFWwindow* buffer_window;

// Define globals
byte* generic_rgba_cluts;
GLuint generic_cluts_texture;
GLuint fgame_texture;
std::vector<GLuint> fgame_textures;
std::vector<GLuint> item_textures;
byte* item_cluts;
GLuint item_cluts_texture;
std::vector<std::vector<Sprite>> generic_sprite_banks;
GLuint generic_powerup_texture;
GLuint generic_saveroom_texture;
GLuint generic_loadroom_texture;


/**
 * Opens the SotN section of the imgui.ini file.
 */
static void* SotN_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    if (strcmp(name, "Data") != 0) {
        return nullptr;
    }
    return (void*)1;
}

/**
 * Reads each line of the SotN entry.
 */
static void SotN_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler*, void*, const char* line)
{
    char s[2048] = {0};
    if (sscanf(line, "PsxPath=%2047[^\n]", s) == 1)                 { psx_path = ImStrdup(s); }
    else if (sscanf(line, "BinPath=%2047[^\n]", s) == 1)            { bin_path = ImStrdup(s); }
    else if (sscanf(line, "GfxPath=%2047[^\n]", s) == 1)            { gfx_path = ImStrdup(s); }
}

/**
 * Writes all lines to the SotN section of the imgui.ini file.
 */
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





/**
 * Blends the sprite to make it appear as a lightened overlay.
 */
static void blend_lighten(const ImDrawList* draw_list, const ImDrawCmd* cmd) {
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
}

/**
 * Blends the sprite as with the "Lighten" method, but gets rid of black masks.
 */
static void blend_fade_light(const ImDrawList* draw_list, const ImDrawCmd* cmd) {
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_DST_ALPHA);
}

/**
 * Resets the blend mode.
 */
static void blend_default(const ImDrawList* draw_list, const ImDrawCmd* cmd) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}



/**
 * Prompts the user to select a file from their filesystem.
 *
 * @param filters: Array of file extension filters
 *
 * @return Pointer to a string if file was selected
 * @return Null pointer if no file was selected
 *
 */
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


/**
 * Loads common data from the binary files and initializes the MIPS emulator.
 */
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
    free(fgame_pixeldata);

    fseek(f_game, 256 * 512 * 2, SEEK_SET);
    fread(generic_cluts, sizeof(byte), 256 * 16 * 2, f_game);
    fclose(f_game);
    free(fgame_pixels);

    // Convert all generic CLUTs to their RGBA equivalents
    generic_rgba_cluts = (byte*)calloc(256 * 16 * 4, sizeof(byte));

    // Convert RGB1555 CLUT to RGBA CLUT
    Utils::CLUT_to_RGBA(generic_cluts, generic_rgba_cluts, 256, false);

    // Free the allocated bytes since they're no longer needed
    free(generic_cluts);

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

    // Free up allocated VRAM data
    free(vram_data);

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
    if (tileset_data == nullptr) {
        Log::Error("Failed to allocate memory for tileset data.\n");
        free(tileset_data);
        exit(1);
    }
    compressed_data = (byte*)realloc(compressed_data, data_size);
    if (compressed_data == nullptr) {
        Log::Error("Failed to allocate memory for compressed data.\n");
        free(tileset_data);
        exit(1);
    }
    MipsEmulator::CopyFromRAM(COMPRESSED_GENERIC_SAVEROOM_TILESET_ADDR, compressed_data, data_size);
    Compression::Decompress(tileset_data, compressed_data);
    pixel_data = Utils::Indexed_to_RGBA(tileset_data, data_size / 4);
    generic_saveroom_texture = Utils::CreateTexture(pixel_data, 0x80 / 4, 0x80);
    free(pixel_data);

    // Read the save room tileset
    tileset_data = (byte*)realloc(tileset_data, data_size);
    if (tileset_data == nullptr) {
        Log::Error("Failed to allocate memory for tileset data.\n");
        free(tileset_data);
        exit(1);
    }
    compressed_data = (byte*)realloc(compressed_data, data_size);
    if (compressed_data == nullptr) {
        Log::Error("Failed to allocate memory for compressed data.\n");
        free(tileset_data);
        exit(1);
    }
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
    byte* item_pixel_data = Utils::Indexed_to_RGBA(dra_bin_pixels, (((16 * 16) / 2) * 275) / 2);

    // Read the texture data
    for (int i = 0; i < 275; i++) {

        GLuint chunk_texture = Utils::CreateTexture(item_pixel_data + (i * 0x80 * 2), 16 / 4, 16);

        // Add the texture pointer to the list of map textures
        item_textures.push_back(chunk_texture);
    }
    free(item_pixel_data);

    // Convert all item CLUTs to their RGBA equivalents
    item_cluts = (byte*)calloc(320 * 16 * 4, sizeof(byte));

    // Convert RGB1555 CLUT to RGBA CLUT
    Utils::CLUT_to_RGBA(dra_bin_cluts, item_cluts, 320, false);

    // Create a texture for the RGBA CLUTs
    item_cluts_texture = Utils::CreateTexture(item_cluts, 320, 16);

    // Free temporary allocations
    free(dra_bin_pixels);
    free(dra_bin_cluts);
}


/**
 * Loads data from the specified map file.
 *
 * @param map_path: Filesystem path of the file to open
 *
 */
void load_map_data(const std::filesystem::path& map_path) {
    // Reset map loaded flag
    map.loaded = false;
    map.load_status_msg = "Starting ...";

    // Swap to the buffer context
    glfwMakeContextCurrent(buffer_window);

    std::string map_dir = map_path.parent_path().string();
    std::string map_filename = map_path.filename().string();
    std::string map_gfx_file = "";
    bool isLowerCase = Utils::isLowerCase(map_filename);

    // Check if the user has selected a map graphics file by accident
    if (Utils::toLowerCase(map_filename.substr(0, 2)) == "f_") {
        std::string prefix = isLowerCase ? "f_" : "F_";
        error = "Files starting with \"" + prefix + "\" are map graphics files.\n"
            "Please select a proper map data file instead.";
        // Tell the modal popup that the processing has finished
        popup.status = PopupStatus_Finished;
        return;
    }

    // Try and find the map graphics file, setting the map_gfx_file variable if it was found
    for (const auto& entry : std::filesystem::directory_iterator(map_dir)) {
        std::string f = Utils::toLowerCase(entry.path().string());
        if (f == map_dir + "/f_" + map_filename) {
            map_gfx_file = entry.path().string();
            break;
        }
    }

    // Make sure map graphics file exists in the same directory as the map file itself
    if (map_gfx_file != "") {

        // Initialize the emulator
        if (MipsEmulator::initialized) {
            map.load_status_msg = "Cleaning Up ...";
            map.Cleanup();
        }
        else {
            map.load_status_msg = "Initializing MIPS Emulator ...";
            MipsEmulator::Initialize();
            // Initialize the graphics data, populate MIPS RAM, etc.
            map.load_status_msg = "Loading SotN Data ...";
            load_sotn_data();
        }

        // Load and process the map file
        map.load_status_msg = "Loading Map File Data ...";
        map.LoadMapFile(map_path.string().c_str());

        // Load the map file itself into memory
        map.load_status_msg = "Loading Map File Into MIPS RAM ...";
        MipsEmulator::LoadMapFile(map_path.string().c_str());

        // Load the map graphics
        map.load_status_msg = "Loading Map Graphics ...";
        map.LoadMapGraphics(map_gfx_file.c_str());


        // Store map tile CLUTs in MIPS RAM
        map.load_status_msg = "Storing Map CLUTs ...";
        for (int i = 0; i < 256; i++) {
            MipsEmulator::StoreMapCLUT(i * 32, 32, map.map_tile_cluts[i]);
        }

        // Store map entity CLUTs in MIPS RAM
        map.load_status_msg = "Storing Entity CLUTs ...";
        for (size_t i = 0; i < map.entity_cluts.size(); i++) {

            // Get the current entity CLUT data
            ClutEntry clut = map.entity_cluts[i];

            // Store the CLUT in RAM
            MipsEmulator::StoreMapCLUT(clut.offset, clut.count, clut.clut_data);
        }

        // Load the map entities
        map.load_status_msg = "Loading Entities ...";
        map.LoadMapEntities();

        // Clear out any errors
        error.clear();
    }
    else {
        std::string prefix = isLowerCase ? "f_" : "F_";
        error = "Could not find graphics file (" + prefix + map_filename + ").";
        // Tell the modal popup that the processing has finished
        popup.status = PopupStatus_Finished;
        return;
    }




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
    map.width = (x_max - x_min) * 256;
    map.height = (y_max - y_min) * 256;

    // Tell the modal popup that the processing has finished
    popup.status = PopupStatus_Finished;

    // Signal that the map has finished loading
    map.loaded = true;

    map.load_status_msg = "Done!";
    glfwMakeContextCurrent(nullptr);
}


/**
 * Custom qsort() sorting function for the entity property list.
 *
 * @param lhs: Left-hand side of the comparison
 * @param rhs: Right-hand side of the comparison
 *
 * @return Comparison result
 *
 */
static int IMGUI_CDECL entity_sort_func(const void* lhs, const void* rhs) {

    // Convert std::variant to a signed 64-bit value
    struct ToLong {
        long operator()(char value) { return ((long)value & 0xFF); }
        long operator()(byte value) { return ((long)value & 0xFF); }
        long operator()(short value) { return ((long)value & 0xFFFF); }
        long operator()(ushort value) { return ((long)value & 0xFFFF); }
        long operator()(int value) { return ((long)value & 0xFFFFFFFF); }
        long operator()(uint value) { return ((long)value & 0xFFFFFFFF); }
    };

    // Get sorting data
    static const ImGuiTableSortSpecs* s_current_sort_specs = ImGui::TableGetSortSpecs();
    const EntityDataEntry* a = (const EntityDataEntry*)lhs;
    const EntityDataEntry* b = (const EntityDataEntry*)rhs;

    // Check which columns are being sorted
    for (int i = 0; i < s_current_sort_specs->SpecsCount; i++) {
        const ImGuiTableColumnSortSpecs* sort_spec = &s_current_sort_specs->Specs[i];
        long delta = 0;

        // Execute sorting logic based on column being processed
        switch (sort_spec->ColumnIndex) {
            case 0:
            case 1:
                delta = ((long)a->offset - (long)b->offset);
                break;
            case 2:
                delta = (strcmp(a->name.c_str(), b->name.c_str()));
                break;
            case 3:
            case 4:
                delta = (std::visit(ToLong{}, a->value) - std::visit(ToLong{}, b->value));
                break;
            default:
                IM_ASSERT(0);
                break;
        }
        if (delta > 0) {
            return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
        }
        if (delta < 0) {
            return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
        }
    }

    // Default differentiation function
    return (a->offset - b->offset);
}





/**
 * Do the things.
 */
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
    window = glfwCreateWindow(1280, 720, "SotN Editor", nullptr, nullptr);
    if (window == nullptr)
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














    // Keep dockspace open
    static bool open = true;

    // Flag whether program should exit
    static bool exit = false;

    // Currently-selected entity in the Main Viewport
    static Entity* selected_entity;

    // Define viewports
    static Viewport main_view;
    static Viewport vram_view;

    // Uncomment to enable debug logging to stdout
    //Log::level = LOG_DEBUG;














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

                        // Deselect the currently-selected entity
                        selected_entity = nullptr;

                        // Set a status popup
                        popup.text = "Loading map data for [" + map_path.filename().string() + "] ...";
                        popup.flags = PopupFlag_Ephemeral;

                        // Set the map loading function as a callback to the status popup
                        popup.callback = [map_path] { return load_map_data(map_path); };
                        popup.status = PopupStatus_Init;
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
        ImGuiWindowFlags modal_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

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
                    std::string filename = Utils::toLowerCase(std::filesystem::path(out_path).filename().string());
                    if (filename == "dra.bin") {
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
        if (ImGui::BeginPopupModal("Select F_GAME.BIN file", nullptr, modal_flags)) {
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
                    std::string filename = Utils::toLowerCase(std::filesystem::path(out_path).filename().string());
                    if (filename == "f_game.bin") {
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
                    ImGui::Text("Entity Address: %08X", selected_entity->address + RAM_BASE_OFFSET);
                    ImGui::Text("Sprite Address: %08X", selected_entity->sprite_address + MAP_BIN_OFFSET);
                    ImGui::Text("Decomp Function Addr: %08X", map.entity_functions[selected_entity->data.object_id] + MAP_BIN_OFFSET);

                    // Copy entity data to the clipboard
                    if (ImGui::Button("Copy to clipboard")) {
                        ImGui::LogToClipboard();

                        // Convert data to JSON
                        ImGui::LogText("{\n");
                        if (!selected_entity->name.empty()) {
                            ImGui::LogText("    \"name\": \"%s\",\n", selected_entity->name.c_str());
                        }
                        if (!selected_entity->desc.empty()) {
                            ImGui::LogText("    \"desc\": \"%s\",\n", selected_entity->desc.c_str());
                        }
                        ImGui::LogText("    \"id\": %d,\n", selected_entity->id);
                        ImGui::LogText("    \"slot\": %d,\n", selected_entity->slot);
                        ImGui::LogText("    \"address\": %u,    // 0x%08X\n", selected_entity->address + RAM_BASE_OFFSET, selected_entity->address + RAM_BASE_OFFSET);

                        ImGui::LogText("    \"data\": {\n");

                        // Loop through each data map entry
                        for (int i = 0; i < selected_entity->data_vec.size(); i++) {

                            // Using "auto" so that "std::variant<...>" doesn't have to be typed out
                            auto* entry = &selected_entity->data_vec[i];

                            // Determine JSON data value and comment value
                            std::string fmt_dec = "%";
                            std::string fmt_hex;
                            if (std::holds_alternative<char>(entry->value) || std::holds_alternative<byte>(entry->value)) {
                                fmt_dec.append("hh");
                                fmt_hex.append("0x%02hhX");
                            }
                            else if (std::holds_alternative<short>(entry->value) || std::holds_alternative<ushort>(entry->value)) {
                                fmt_dec.append("h");
                                fmt_hex.append("0x%04hX");
                            }
                            else {
                                fmt_hex.append("0x%08X");
                            }

                            // Determine JSON data display type (signed vs. unsigned)
                            if (std::is_signed<decltype(entry->value)>::value) {
                                fmt_dec.append("d");
                            }
                            else {
                                fmt_dec.append("u");
                            }

                            // Log the data map entry
                            std::string json_line = "        \"%s\": " + fmt_dec;
                            json_line.append(",    // " + fmt_hex + "\n");
                            std::string output = Utils::FormatString(json_line.c_str(), entry->name.c_str(), entry->value, entry->value);
                            ImGui::LogText("%s", output.c_str());
                        }
                        ImGui::LogText("    }\n");
                        ImGui::LogText("}\n");

                        ImGui::LogFinish();
                    }



                    ImGuiTableFlags test_flags = (
                        ImGuiTableFlags_NoSavedSettings |
                        ImGuiTableFlags_Sortable |
                        ImGuiTableFlags_BordersV |
                        ImGuiTableFlags_BordersOuterV |
                        ImGuiTableFlags_BordersInnerV |
                        ImGuiTableFlags_BordersH |
                        ImGuiTableFlags_BordersOuterH |
                        ImGuiTableFlags_BordersInnerH |
                        ImGuiTableFlags_SortMulti
                    );
                    static ImVector<int> selection;
                    static bool data_needs_sort = false;
                    static const ImGuiTableSortSpecs* s_current_sort_specs;



                    if (ImGui::BeginTable("Entity Properties", 5, test_flags, ImVec2(0, 0), 0.0f)) {
                        // Declare columns
                        ImGui::TableSetupColumn("#",   ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.0f, 0);
                        ImGui::TableSetupColumn("#",   ImGuiTableColumnFlags_WidthFixed, 0.0f, 1);
                        ImGui::TableSetupColumn("Name",           ImGuiTableColumnFlags_WidthFixed, 0.0f, 2);
                        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthFixed, 0.0f, 3);
                        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthFixed, 0.0f, 4);



                        // Resort data if sorting method was changed
                        ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs();
                        if (sorts_specs && sorts_specs->SpecsDirty) {
                            data_needs_sort = true;
                        }
                        if (sorts_specs && data_needs_sort && selected_entity->data_vec.size() > 1) {
                            qsort(&selected_entity->data_vec[0], (size_t)selected_entity->data_vec.size(), sizeof(selected_entity->data_vec[0]), entity_sort_func);
                            sorts_specs->SpecsDirty = false;
                        }
                        data_needs_sort = false;

                        // Show headers
                        ImGui::TableHeadersRow();

                        // Show data
                        ImGui::PushButtonRepeat(true);

                        // Utilize clipper for list (just in case for later)
                        ImGuiListClipper clipper;
                        clipper.Begin(selected_entity->data_vec.size());
                        while (clipper.Step()) {

                            // Loop through each property
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {

                                // Get the current property
                                auto* entry = &selected_entity->data_vec[i];
                                std::string fmt_dec = "%";
                                std::string fmt_hex;
                                uint data_size = 0;
                                if (std::holds_alternative<char>(entry->value) || std::holds_alternative<byte>(entry->value)) {
                                    fmt_dec.append("hh");
                                    fmt_hex.append("%02hhX");
                                    data_size = 1;
                                }
                                else if (std::holds_alternative<short>(entry->value) || std::holds_alternative<ushort>(entry->value)) {
                                    fmt_dec.append("h");
                                    fmt_hex.append("%04hX");
                                    data_size = 2;
                                }
                                else {
                                    fmt_hex.append("%08X");
                                    data_size = 4;
                                }

                                if (std::is_signed<decltype(entry->value)>::value) {
                                    fmt_dec.append("d");
                                }
                                else {
                                    fmt_dec.append("u");
                                }

                                // Format hex and dec strings
                                std::string hex_val = Utils::FormatString(fmt_hex.c_str(), entry->value);
                                std::string dec_val = Utils::FormatString(fmt_dec.c_str(), entry->value);

                                const bool item_is_selected = selection.contains(entry->offset);
                                ImGui::PushID(entry->offset);
                                ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);

                                // For the demo purpose we can select among different type of items submitted in the first column
                                ImGui::TableSetColumnIndex(0);

                                std::string label = Utils::FormatString("%d  ", entry->offset);

                                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                                if (ImGui::Selectable(label.c_str(), item_is_selected, selectable_flags, ImVec2(0, 0))) {
                                    if (ImGui::GetIO().KeyCtrl) {
                                        if (item_is_selected) {
                                            selection.find_erase_unsorted(entry->offset);
                                        }
                                        else {
                                            selection.push_back(entry->offset);
                                        }
                                    }
                                    else {
                                        selection.clear();
                                        selection.push_back(entry->offset);
                                    }
                                }

                                if (ImGui::TableSetColumnIndex(1)) {
                                    ImGui::Text("%02X  ", entry->offset);
                                }

                                if (ImGui::TableSetColumnIndex(2)) {
                                    ImGui::Text("%s  ", entry->name.c_str());
                                }

                                if (ImGui::TableSetColumnIndex(3)) {
                                    ImGui::Text("%s  ", hex_val.c_str());
                                }

                                if (ImGui::TableSetColumnIndex(4)) {
                                    ImGui::Text("%s  ", dec_val.c_str());
                                }

                                ImGui::PopID();
                            }
                        }
                        ImGui::PopButtonRepeat();
                        ImGui::EndTable();
                    }
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

            if (map.loaded) {

                // No padding for main scrolling viewport
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                // Create a child window
                if (ImGui::BeginChild("MainViewportDisplay", ImVec2(0, 0), true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

                    // Create an invisible button for the canvas
                    ImGui::PushID(0);
                    ImGui::InvisibleButton("##MainCanvas", ImGui::GetWindowSize());
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        main_view.camera.x += ImGui::GetIO().MouseDelta.x;
                        main_view.camera.y += ImGui::GetIO().MouseDelta.y;
                    }

                    // Update offsets with scroll
                    main_view.camera.x += io.MouseWheelH * 2;
                    main_view.camera.y += io.MouseWheel * 2;

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
                    ImGui::SetCursorPos(ImVec2(main_view.camera.x, main_view.camera.y));






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



                    // Handle main viewport zooming functionality
                    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Equal)) {
                        float x_ratio = main_view.camera.x / (map.width * main_view.zoom);
                        float y_ratio = main_view.camera.y / (map.height * main_view.zoom);
                        main_view.zoom *= 2;
                        main_view.camera.x = x_ratio * (map.width * main_view.zoom) - (ImGui::GetWindowSize().x / 2);
                        main_view.camera.y = y_ratio * (map.height * main_view.zoom) - (ImGui::GetWindowSize().y / 2);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
                        // Get the current X/Y ratio
                        float x_ratio = main_view.camera.x / (map.width * main_view.zoom);
                        float y_ratio = main_view.camera.y / (map.height * main_view.zoom);
                        main_view.zoom /= 2;
                        main_view.camera.x = x_ratio * (map.width * main_view.zoom) + (ImGui::GetWindowSize().x / 4);
                        main_view.camera.y = y_ratio * (map.height * main_view.zoom) + (ImGui::GetWindowSize().y / 4);
                    }

                    // Don't space out the items
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));





                    // Draw backgrounds
                    for (int i = 0; i < map.rooms.size(); i++) {
                        Room* cur_room = &map.rooms[i];

                        if (cur_room->load_flag == 0xFF) {
                            continue;
                        }

                        ImGui::SetCursorPos(ImVec2(main_view.camera.x, main_view.camera.y));

                        uint x_coord = (cur_room->x_start - x_min) * 256 * main_view.zoom;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * main_view.zoom;

                        for (auto const& layer : cur_room->bg_ordering_table) {

                            // Get layer data
                            uint id = layer.first;
                            std::vector<EntitySpritePart> sprites = layer.second;

                            // Loop through each sprite in the current layer
                            for (int p = 0; p < sprites.size(); p++) {

                                // Get the current sprite
                                EntitySpritePart* cur_sprite = &sprites[p];

                                // Get the X/Y coords
                                float sprite_x = main_view.camera.x + x_coord + (cur_sprite->x * main_view.zoom);
                                float sprite_y = main_view.camera.y + y_coord + (cur_sprite->y * main_view.zoom);

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

                                // Draw the image
                                ImGui::Image(
                                    (void*)(intptr_t)cur_sprite->texture,
                                    ImVec2((float)cur_sprite->width * main_view.zoom, (float)cur_sprite->height * main_view.zoom),
                                    uv0, uv1
                                );
                                draw_list->AddCallback(blend_default, nullptr);
                            }
                        }
                        // Draw the actual background on top of everything below it
                        ImGui::SetCursorPos(ImVec2(main_view.camera.x + (float)x_coord, main_view.camera.y + (float)y_coord));
                        ImGui::Image((void*)(intptr_t)cur_room->bg_texture, ImVec2(cur_room->bg_layer.width * 16 * main_view.zoom, cur_room->bg_layer.height * 16 * main_view.zoom));
                    }

                    // Draw entities in between FG and BG
                    for (int i = 0; i < map.rooms.size(); i++) {
                        Room* cur_room = &map.rooms[i];

                        if (cur_room->load_flag == 0xFF) {
                            continue;
                        }

                        ImGui::SetCursorPos(ImVec2(main_view.camera.x, main_view.camera.y));

                        uint x_coord = (cur_room->x_start - x_min) * 256 * main_view.zoom;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * main_view.zoom;

                        for (auto const& layer : cur_room->mid_ordering_table) {

                            // Get layer data
                            uint id = layer.first;
                            std::vector<EntitySpritePart> sprites = layer.second;

                            // Loop through each sprite in the current layer
                            for (int p = 0; p < sprites.size(); p++) {

                                // Get the current sprite
                                EntitySpritePart* cur_sprite = &sprites[p];

                                // Get the X/Y coords
                                float sprite_x = main_view.camera.x + x_coord + (cur_sprite->x * main_view.zoom);
                                float sprite_y = main_view.camera.y + y_coord + (cur_sprite->y * main_view.zoom);

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
                                            ImVec2(sprite_x + (float)cur_sprite->width * main_view.zoom + clip_min.x, sprite_y + (float)cur_sprite->height * main_view.zoom + clip_min.y),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255)
                                        );
                                    }
                                    else {
                                        ImGui::Image(
                                            (void*)(intptr_t)cur_sprite->texture,
                                            ImVec2((float)cur_sprite->width * main_view.zoom, (float)cur_sprite->height * main_view.zoom),
                                            uv0, uv1
                                        );
                                    }

                                    auto& buf = ImGui::GetWindowDrawList()->VtxBuffer;
                                    if (vtx_idx < buf.Size) {

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
                                                tmp_y = buf[vtx_idx + 1].pos.y;
                                                buf[vtx_idx + 1].pos.y = buf[vtx_idx + 3].pos.y;
                                                buf[vtx_idx + 3].pos.y = tmp_y;
                                            }
                                            buf[vtx_idx + 0].pos.x += (cur_sprite->top_left.x * main_view.zoom);
                                            buf[vtx_idx + 0].pos.y += (cur_sprite->top_left.y * main_view.zoom);
                                            buf[vtx_idx + 1].pos.x += (cur_sprite->top_right.x * main_view.zoom);
                                            buf[vtx_idx + 1].pos.y += (cur_sprite->top_right.y * main_view.zoom);
                                            buf[vtx_idx + 2].pos.x += (cur_sprite->bottom_right.x * main_view.zoom);
                                            buf[vtx_idx + 2].pos.y += (cur_sprite->bottom_right.y * main_view.zoom);
                                            buf[vtx_idx + 3].pos.x += (cur_sprite->bottom_left.x * main_view.zoom);
                                            buf[vtx_idx + 3].pos.y += (cur_sprite->bottom_left.y * main_view.zoom);
                                        }
                                    }
                                }
                                else {
                                    ImGui::Image(
                                        (void*)(intptr_t)cur_sprite->texture,
                                        ImVec2((float)cur_sprite->width * main_view.zoom, (float)cur_sprite->height * main_view.zoom),
                                        uv0, uv1
                                    );
                                }

                                // Check for rotations
                                if (cur_sprite->rotate) {
                                    auto& buf = ImGui::GetWindowDrawList()->VtxBuffer;
                                    if (vtx_idx < buf.Size) {

                                        // Calculate rotation angle in radians
                                        float rad = ((((float)cur_sprite->rotate / 4096.0f) * 360.0f) * (float)M_PI) / 180.0f;
                                        float rot_sin = sinf(rad);
                                        float rot_cos = cosf(rad);

                                        // Calculate center point
                                        ImVec2 pivot = ImVec2(
                                            buf[vtx_idx].pos.x - (float)cur_sprite->offset_x * main_view.zoom,
                                            buf[vtx_idx].pos.y - (float)cur_sprite->offset_y * main_view.zoom
                                        );

                                        // Determine pivot point
                                        ImVec2 rel_pivot = ImRotate(pivot, rot_cos, rot_sin);
                                        rel_pivot.x -= pivot.x;
                                        rel_pivot.y -= pivot.y;

                                        // Rotate vertex around pivot
                                        for (int k = vtx_idx; k < buf.Size; k++) {
                                            ImVec2 rot_val = ImRotate(buf[k].pos, rot_cos, rot_sin);
                                            buf[k].pos.x = rot_val.x - rel_pivot.x;
                                            buf[k].pos.y = rot_val.y - rel_pivot.y;
                                        }
                                    }
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

                        ImGui::SetCursorPos(ImVec2(main_view.camera.x, main_view.camera.y));

                        uint x_coord = (cur_room->x_start - x_min) * 256 * main_view.zoom;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * main_view.zoom;

                        ImGui::SetCursorPos(ImVec2(main_view.camera.x + (float)x_coord, main_view.camera.y + (float)y_coord));
                        ImGui::Image((void*)(intptr_t)cur_room->fg_texture, ImVec2(cur_room->fg_layer.width * 16 * main_view.zoom, cur_room->fg_layer.height * 16 * main_view.zoom));

                        for (auto const& layer : cur_room->fg_ordering_table) {

                            // Get layer data
                            uint id = layer.first;
                            std::vector<EntitySpritePart> sprites = layer.second;

                            // Loop through each sprite in the current layer
                            for (int p = 0; p < sprites.size(); p++) {

                                // Get the current sprite
                                EntitySpritePart* cur_sprite = &sprites[p];

                                // Get the X/Y coords
                                float sprite_x = main_view.camera.x + x_coord + (cur_sprite->x * main_view.zoom);
                                float sprite_y = main_view.camera.y + y_coord + (cur_sprite->y * main_view.zoom);

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
                                            ImVec2(sprite_x + (float)cur_sprite->width * main_view.zoom + clip_min.x, sprite_y + (float)cur_sprite->height * main_view.zoom + clip_min.y),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255),
                                            IM_COL32(255, 255, 255, 255)
                                        );
                                    }
                                    else {
                                        ImGui::Image(
                                            (void*)(intptr_t)cur_sprite->texture,
                                            ImVec2((float)cur_sprite->width * main_view.zoom, (float)cur_sprite->height * main_view.zoom),
                                            uv0, uv1
                                        );
                                    }

                                    auto& buf = ImGui::GetWindowDrawList()->VtxBuffer;
                                    if (vtx_idx < buf.Size) {

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
                                                tmp_y = buf[vtx_idx + 1].pos.y;
                                                buf[vtx_idx + 1].pos.y = buf[vtx_idx + 3].pos.y;
                                                buf[vtx_idx + 3].pos.y = tmp_y;
                                            }
                                            buf[vtx_idx + 0].pos.x += (cur_sprite->top_left.x * main_view.zoom);
                                            buf[vtx_idx + 0].pos.y += (cur_sprite->top_left.y * main_view.zoom);
                                            buf[vtx_idx + 1].pos.x += (cur_sprite->top_right.x * main_view.zoom);
                                            buf[vtx_idx + 1].pos.y += (cur_sprite->top_right.y * main_view.zoom);
                                            buf[vtx_idx + 2].pos.x += (cur_sprite->bottom_right.x * main_view.zoom);
                                            buf[vtx_idx + 2].pos.y += (cur_sprite->bottom_right.y * main_view.zoom);
                                            buf[vtx_idx + 3].pos.x += (cur_sprite->bottom_left.x * main_view.zoom);
                                            buf[vtx_idx + 3].pos.y += (cur_sprite->bottom_left.y * main_view.zoom);
                                        }
                                    }
                                }
                                else {
                                    ImGui::Image(
                                        (void*)(intptr_t)cur_sprite->texture,
                                        ImVec2((float)cur_sprite->width * main_view.zoom, (float)cur_sprite->height * main_view.zoom),
                                        uv0, uv1
                                    );
                                }

                                // Check for rotations
                                if (cur_sprite->rotate) {
                                    auto& buf = ImGui::GetWindowDrawList()->VtxBuffer;
                                    if (vtx_idx < buf.Size) {

                                        // Calculate rotation angle in radians
                                        float rad = ((((float)cur_sprite->rotate / 4096.0f) * 360.0f) * (float)M_PI) / 180.0f;
                                        float rot_sin = sinf(rad);
                                        float rot_cos = cosf(rad);

                                        // Calculate center point
                                        ImVec2 pivot = ImVec2(
                                            buf[vtx_idx].pos.x - (float)cur_sprite->offset_x * main_view.zoom,
                                            buf[vtx_idx].pos.y - (float)cur_sprite->offset_y * main_view.zoom
                                        );

                                        // Determine pivot point
                                        ImVec2 rel_pivot = ImRotate(pivot, rot_cos, rot_sin);
                                        rel_pivot.x -= pivot.x;
                                        rel_pivot.y -= pivot.y;

                                        // Rotate vertex around pivot
                                        for (int k = vtx_idx; k < buf.Size; k++) {
                                            ImVec2 rot_val = ImRotate(buf[k].pos, rot_cos, rot_sin);
                                            buf[k].pos.x = rot_val.x - rel_pivot.x;
                                            buf[k].pos.y = rot_val.y - rel_pivot.y;
                                        }
                                    }
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
                        uint x_coord = (cur_room->x_start - x_min) * 256 * main_view.zoom;
                        uint y_coord = (cur_room->y_start - y_min) * 256 * main_view.zoom;

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
                        ImGui::SetCursorPos(ImVec2(main_view.camera.x + x_coord, main_view.camera.y + y_coord));
                        ImGui::Text("Room %d", i);





                        // Loop through each entity
                        for (int k = 0; k < cur_room->entities.size(); k++) {

                            // Get the current entity
                            Entity* entity = &cur_room->entities[k];

                            // Check if entity has a hitbox size
                            float button_width = (entity->data.hitbox_width > 0 ? (float)entity->data.hitbox_width : 16.0f) * main_view.zoom;
                            float button_height = (entity->data.hitbox_height > 0 ? (float)entity->data.hitbox_height : 16.0f) * main_view.zoom;

                            // Set the cursor position to the target item
                            ImGui::SetCursorPos(
                                ImVec2(
                                    main_view.camera.x + x_coord + (entity->data.pos_x * main_view.zoom) - (button_width / 2),
                                    main_view.camera.y + y_coord + (entity->data.pos_y * main_view.zoom) - (button_height / 2)
                                )
                            );

                            // Draw an outline around the entity
                            ImGui::PushID(entity_uuid);
                            if (ImGui::ColorButton("##test", ImVec4(0, 0, 0, 0), color_flags, ImVec2(button_width, button_height))) {
                                Log::Info("Clicked entity %d in room %d\n", k, i);
                                selected_entity = &map.rooms[i].entities[k];
                            }
                            ImGui::PopID();
                            entity_uuid++;
                        }

                        ImGui::PopStyleColor();
                    }


                    // Show zoom amount
                    ImGui::SetCursorPos(ImVec2(0, ImGui::GetWindowSize().y - 14));
                    ImGui::Text("Zoom: %0.2f%%", main_view.zoom * 100);

                    // Show camera position
                    ImGui::SetCursorPos(ImVec2(0, 0));
                    ImGui::Text("Camera: (%0.0f, %0.0f)", -main_view.camera.x * (1 / main_view.zoom), -main_view.camera.y * (1 / main_view.zoom));

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

            if (map.loaded) {

                // No padding for main scrolling viewport
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                // Create a child window
                if (ImGui::BeginChild("MainVRAMDisplay", ImVec2(0, 0), true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

                    // Create an invisible button for the canvas
                    ImGui::PushID(0);
                    ImGui::InvisibleButton("##VRAMCanvas", ImGui::GetWindowSize());
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        vram_view.camera.x += ImGui::GetIO().MouseDelta.x;
                        vram_view.camera.y += ImGui::GetIO().MouseDelta.y;
                    }

                    // Update offsets with scroll
                    vram_view.camera.x += io.MouseWheelH * 2;
                    vram_view.camera.y += io.MouseWheel * 2;

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



                    // Handle VRAM viewport zooming functionality (inaccurate)
                    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Equal)) {
                        float x_ratio = vram_view.camera.x / (map.width * vram_view.zoom);
                        float y_ratio = vram_view.camera.y / (map.height * vram_view.zoom);
                        vram_view.zoom *= 2;
                        vram_view.camera.x = x_ratio * (map.width * vram_view.zoom) - (ImGui::GetWindowSize().x / 2);
                        vram_view.camera.y = y_ratio * (map.height * vram_view.zoom) - (ImGui::GetWindowSize().y / 2);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
                        // Get the current X/Y ratio
                        float x_ratio = vram_view.camera.x / (map.width * vram_view.zoom);
                        float y_ratio = vram_view.camera.y / (map.height * vram_view.zoom);
                        vram_view.zoom /= 2;
                        vram_view.camera.x = x_ratio * (map.width * vram_view.zoom) + (ImGui::GetWindowSize().x / 4);
                        vram_view.camera.y = y_ratio * (map.height * vram_view.zoom) + (ImGui::GetWindowSize().y / 4);
                    }






                    // Create an image at 0,0
                    ImGui::SetCursorPos(ImVec2(vram_view.camera.x, vram_view.camera.y));

                    // Make the text not do a weird thing
                    ImGui::Text("");

                    ImVec2 cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("Map VRAM:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)map.map_vram, ImVec2(512 * vram_view.zoom, 256 * vram_view.zoom));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("Generic CLUTs:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)generic_cluts_texture, ImVec2(256 * vram_view.zoom, 16 * vram_view.zoom));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("Item CLUTs:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)item_cluts_texture, ImVec2(320 * vram_view.zoom, 16 * vram_view.zoom));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("Tileset 7:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)map.map_tilesets[7], ImVec2(64 * vram_view.zoom, 256 * vram_view.zoom));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("F_GAME.BIN VRAM Texture:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)fgame_texture, ImVec2(512 * vram_view.zoom, 256 * vram_view.zoom));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("F_GAME.BIN Texture 7:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)fgame_textures[7], ImVec2(64 * vram_view.zoom, 256 * vram_view.zoom));

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y + 5));
                    ImGui::Text("MIPS Framebuffer:");

                    cursor_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(cursor_pos.x + vram_view.camera.x, cursor_pos.y));
                    ImGui::Image((void*)(intptr_t)MipsEmulator::framebuffer, ImVec2(1024 * vram_view.zoom, 512 * vram_view.zoom));


                    // Show zoom amount
                    ImGui::SetCursorPos(ImVec2(0, ImGui::GetWindowSize().y - 14));
                    ImGui::Text("Zoom: %0.2f%%", vram_view.zoom * 100);

                    // Show camera position
                    ImGui::SetCursorPos(ImVec2(0, 0));
                    ImGui::Text("Camera: (%0.0f, %0.0f)", -vram_view.camera.x * (1 / vram_view.zoom), -vram_view.camera.y * (1 / vram_view.zoom));


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
        }
        ImGui::End();





// -- Generic Info Popup ---------------------------------------------------------------------------------------

        if ((!error.empty() || !popup.text.empty()) && !ImGui::IsPopupOpen("any", ImGuiPopupFlags_AnyPopupId)) {
            ImGui::OpenPopup("Info");
        }

        // Display popup
        if (ImGui::BeginPopupModal("Info", nullptr, modal_flags)) {

            // Error-specific popup
            if (!error.empty()) {
                ImGui::Text("SotN Editor has encountered an error.\n\nDetails:");
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s\n\n", error.c_str());
                ImGui::Separator();
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    error.clear();
                    ImGui::CloseCurrentPopup();
                }
            }

            // Generic popup
            else {

                ImGui::Text("%s", popup.text.c_str());

                // Check if this is the map being loaded
                if (!map.loaded) {
                    ImGui::Text("* %s", map.load_status_msg.c_str());
                }

                // Show an OK button to close the popup
                if ((popup.flags | PopupFlag_HasOK) == PopupFlag_HasOK) {
                    ImGui::Separator();
                    if (ImGui::Button("OK", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                        popup.text.clear();
                        popup.flags = PopupFlag_None;
                    }
                }

                // Close the popup on the next frame
                else if ((popup.flags | PopupFlag_Ephemeral) == PopupFlag_Ephemeral) {

                    // Wait until the popup is visible before flagging it to close
                    ImGuiWindow* win = ImGui::GetTopMostAndVisiblePopupModal();
                    if (win != nullptr) {
                        popup.flags = PopupFlag_Close;
                    }
                }

                // Close the popup
                else if ((popup.flags | PopupFlag_Close) == PopupFlag_Close) {

                    // Run any popup callbacks
                    if (popup.status == PopupStatus_Init && popup.callback != nullptr) {

                        // Create a new buffer window
                        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                        buffer_window = glfwCreateWindow(1280, 720, "", nullptr, window);

                        // Change status to prevent any UI
                        popup.status = PopupStatus_Processing;
                        std::thread popup_thread(popup.callback);
                        popup.callback = nullptr;
                        popup_thread.detach();
                    }

                    // Close the popup and clear data if callback finished
                    if (popup.status == PopupStatus_Finished) {
                        glfwMakeContextCurrent(window);
                        ImGui::CloseCurrentPopup();
                        popup.text.clear();
                        popup.flags = PopupFlag_None;

                        // Destroy the buffer window
                        glfwDestroyWindow(buffer_window);
                    }
                }

                // Close the popup automatically if no flags were set
                else {
                    popup.text.clear();
                    ImGui::CloseCurrentPopup();
                    popup.flags = PopupFlag_None;
                }
            }
            ImGui::EndPopup();
        }





// -- Render ---------------------------------------------------------------------------------------------------

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

    // Clean up MIPS data and map data
    map.Cleanup();
    MipsEmulator::Cleanup();

    return 0;
}
