# foghoge

An atmospheric, D&D-flavoured horror JRPG drawn entirely in ASCII and Unicode glyphs.
Glyphs are placed at free sub-pixel positions rather than on a terminal grid, then lit by a
post-process lantern shader with flicker, vignette and film grain.

Built with C++20 and [raylib](https://www.raylib.com/) 5.5 (fetched and pinned by CMake).

## Build

Requirements: CMake 3.21+, a C++20 compiler and git.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Linux also needs the usual X11/Wayland/GL headers:

```sh
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev \
  libxinerama-dev libgl1-mesa-dev libglu1-mesa-dev libwayland-dev libxkbcommon-dev wayland-protocols
```

## Run

```sh
./build/foghoge                 # macOS / Linux
build\Release\foghoge.exe       # Windows (Visual Studio generator)
```

`assets/` and `shaders/` are copied next to the executable on every build.
Move the mouse to carry the lantern. Esc quits.

## Layout

| Path | What |
| --- | --- |
| `src/main.cpp` | Window, font loading, glyph scene, lighting pass |
| `shaders/lantern.fs` | Post-process: lantern falloff, flicker, vignette, grain |
| `assets/fonts/` | DejaVu Sans Mono (box-drawing and block glyphs) and its license |
