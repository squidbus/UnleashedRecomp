# UnleashedRecomp
# Building
1. Decompress and decrypt `default.xex` and place the resulting file in `./UnleashedRecompLib/private/`.
2. Decompress `shader.ar` and place the resulting file in `./UnleashedRecompLib/private/`.
3. Open the repository directory in Visual Studio 2022 (not Preview) and wait for CMake generation to complete.
4. Under Solution Explorer, right-click and choose "Switch to CMake Targets View".
5. Expand the root project and right-click the **PowerRecomp** project and choose "Set as Startup Item", then run it and wait for recompilation to complete.
6. After code recompilation, right-click the **ShaderRecomp** project and choose "Set as Startup Item", then run it and wait for recompilation to complete.
7. After shader recompilation, expand the **UnleashedRecompLib** project and open `CMakeLists.txt`, then save (CTRL+S) to force CMake cache regeneration to populate the recompiled code.
8. Right-click the **UnleashedRecomp** project and choose "Set as Startup Item" and then choose "Add Debug Configuration".
9. Add a `currentDir` property to the generated JSON and set its value to the path of your game files.
10. Start debugging **UnleashedRecomp**.