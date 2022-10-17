# SotN Editor

SotN Editor is a mod tool for Castlevania: Symphony of the Night.

It is currently only capable of parsing and displaying data contained within map files (`ST/*/*.BIN` and `BOSS/*/*.BIN`), but it will eventually be able to modify map data (entities, tiles, rooms, etc.) and export those changes back into a working ROM file.

![SotN Editor interface](Screenshot.png)

## Building

Requirements:
* GLFW development libraries
* GLEW development libraries
* GTK3 development libraries (for NFD)

These can be obtained on Linux by running the following command:

```
sudo apt install libglfw3-dev libglew-dev libgtk-3-dev
```

The program can be built in Linux by running the following commands:

```
$ git clone https://github.com/KernelEquinox/SotN-Editor.git
$ cd SotN-Editor
$ mkdir build && cd build
$ cmake ..
$ make
```


## Known Issues

* Some rooms overlap each other
* Some sprites utilizing POLY_GT4 structs display on top of other rooms
* The lever in NO1 (Outer Wall) has a mismatched CLUT
* Room 9 in DAI (Royal Chapel) has weird texture/POLY_GT4 oddities
* More accurate POLY_GT4 handling is required
* Oddities in ST0 need to be analyzed (no breakables in Room 1 or Room 3, weird Richter thing, etc.)
* Rocks in WRP don't show up
* Other things I'm forgetting