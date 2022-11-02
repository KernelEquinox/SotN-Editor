# SotN Editor

SotN Editor is a mod tool for Castlevania: Symphony of the Night.

It is currently only capable of parsing and displaying data contained within map files (`ST/*/*.BIN` and `BOSS/*/*.BIN`), but it will eventually be able to modify map data (entities, tiles, rooms, etc.) and export those changes back into a working ROM file.

![SotN Editor interface](Screenshot.png)

|Platform|Status|Download|
|--------|------|--------|
|Windows|[![Windows](https://github.com/KernelEquinox/SotN-Editor/actions/workflows/Windows.yml/badge.svg)](https://github.com/KernelEquinox/SotN-Editor/actions/workflows/Windows.yml)|[Windows 64-bit (x86)](https://github.com/KernelEquinox/SotN-Editor/releases/latest/download/SotN_Editor_Windows.zip)|
|Linux|[![Linux](https://github.com/KernelEquinox/SotN-Editor/actions/workflows/Linux.yml/badge.svg)](https://github.com/KernelEquinox/SotN-Editor/actions/workflows/Linux.yml)|[Linux 64-bit (x86)](https://github.com/KernelEquinox/SotN-Editor/releases/latest/download/SotN_Editor_Linux.zip)|
|macOS|[![macOS](https://github.com/KernelEquinox/SotN-Editor/actions/workflows/macOS.yml/badge.svg)](https://github.com/KernelEquinox/SotN-Editor/actions/workflows/macOS.yml)|[macOS](https://github.com/KernelEquinox/SotN-Editor/releases/latest/download/SotN_Editor_macOS.zip)|


## Usage

When loading for the first time, SotN Editor will ask for the locations of the PSX binary (`SLUS000.67`), the SotN binary (`DRA.BIN`), and the common graphics file (`BIN/F_GAME.BIN`).

To load a map file, click on `File` -> `Open Map File`, then select one of the map files in the `ST/` or `BOSS/` directory (e.g. `ST/ARE/ARE.BIN`).

Once the map is loaded, the map can be navigated by either clicking and dragging or by using the scroll wheel or touchpad to pan around the map. Clicking on an entity will display the entity's properties in the left-hand Properties window.

The `+` and `-` keys can be used to zoom in and out of the viewport.


## Building

### Linux

Install the necessary requirements by running the following command:

```
sudo apt install build-essential cmake libglfw3-dev libglew-dev libgtk-3-dev
```

Once the requirements are installed, the program can be built by running the following commands:

```
git clone --recursive https://github.com/KernelEquinox/SotN-Editor.git
cd SotN-Editor
cmake -B build
cmake --build build
```

This will create the program in the `build/` directory.

### macOS

Install the necessary requirements with `brew` by running the following command:

```
brew install glew glfw
```

Alternatively, the requirements can be installed via `conda` by running the following command:

```
conda install glew glfw
```

Once the requirements are installed, the program can be built by running the following commands:

```
git clone --recursive https://github.com/KernelEquinox/SotN-Editor.git
cd SotN-Editor
cmake -B build
cmake --build build
```

This will create the program in the `build/` directory.

### Windows

To build the program on Windows:

1. Install [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community) and ensure that `Desktop development with C++` is selected in the Workloads tab.
2. Install [Git for Windows](https://git-scm.com/download/win).
3. Run the following command:

   ```
   git clone --recursive https://github.com/KernelEquinox/SotN-Editor.git
   ```
   
4. Double-click on the `build.bat` file to build the project.

This will create the program in the `build/Debug/` directory.


## Known Issues

* Some rooms overlap each other
* Some sprites utilizing POLY_GT4 structs display on top of other rooms
* The lever in NO1 (Outer Wall) has a mismatched CLUT
* Room 9 in DAI (Royal Chapel) has weird texture/POLY_GT4 oddities
* More accurate POLY_GT4 handling is required
* Oddities in ST0 need to be analyzed (no breakables in Room 1 or Room 3, weird Richter thing, etc.)
* Rocks in WRP don't show up
* Other things I'm forgetting
