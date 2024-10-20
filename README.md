# UnleashedRecomp
# Building
1. Clone **UnleashedRecomp** with submodules (or run `update_submodules.bat` to ensure submodules are pulled).
```
git clone --recurse-submodules https://github.com/hedge-dev/UnleashedRecomp.git
```
2. Decompress and decrypt `default.xex` and place the resulting file in `./UnleashedRecompLib/private/`.
3. Decompress `shader.ar` and place the resulting file in `./UnleashedRecompLib/private/`.
4. Open the repository directory in Visual Studio 2022 *(not Preview)* and wait for CMake generation to complete.
5. Under Solution Explorer, right-click and choose "Switch to CMake Targets View".
6. Expand the root project and right-click the **PowerRecomp** project and choose "Set as Startup Item", then run it and wait for recompilation to complete.
7. After code recompilation, right-click the **ShaderRecomp** project and choose "Set as Startup Item", then run it and wait for recompilation to complete.
8. After shader recompilation, expand the **UnleashedRecompLib** project and open `CMakeLists.txt`, then save (CTRL+S) to force CMake cache regeneration to populate the recompiled code.
9.  Right-click the **UnleashedRecomp** project and choose "Set as Startup Item" and then choose "Add Debug Configuration".
10. Add a `currentDir` property to the first element under `configurations` in the generated JSON and set its value to the path to your game directory (where root is the directory containing `dlc`, `game`, `save`, `update`, etc).
11. Start debugging **UnleashedRecomp**.