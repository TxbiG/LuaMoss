# LuaMoss

LuaMoss is a Lua 5.4 module for the Moss Framework. It uses Lua's small native C API directly, avoiding a generated binding with unclear C++ ownership.

The first usable surface includes `Vec2`, `Vec3`, `Vec4`, `Color`, `Quat`, and `Rect`; vector arithmetic and vector methods; window/event/input helpers; and basic 2D immediate renderer helpers.

## Build

Install Lua 5.4 development files, then:

```powershell
cmake -S . -B build -DMOSS_ROOT=C:/Users/TobyG/Documents/MossFramework
cmake --build build --config Debug
```

`moss.dll` is emitted beneath `build`. Put its directory on `package.cpath` and run `examples/basic.lua`.

`Renderer` retains its Lua `Window`, so its native resources are destroyed in the safe order. The module inherits Moss's CMake-selected graphics backend.