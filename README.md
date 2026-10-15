![Engine Preview](preview.png)

### Dependencies
* C++17 compiler (GCC / MinGW-w64)
* SDL2
* Bullet Physics
* GLEW
* OpenGL and GLU
* FreeType 2
* pkg-config

### Build
```
g++ main.cpp -o engine.exe -O2 -std=c++17 -lmingw32 -lSDL2main -lSDL2 -lBulletDynamics -lBulletCollision -lLinearMath -lglew32 -lopengl32 -lglu32 $(pkg-config --cflags --libs freetype2)
```

### Build (Windows)
1. Install [MSYS64](https://www.msys2.org/)
2. Install dependencies:
   ```
   pacman -S --needed \
   mingw-w64-ucrt-x86_64-toolchain \
   mingw-w64-ucrt-x86_64-SDL2 \
   mingw-w64-ucrt-x86_64-bullet \
   mingw-w64-ucrt-x86_64-glew \
   mingw-w64-ucrt-x86_64-freetype \
   mingw-w64-ucrt-x86_64-pkgconf
   ```
3. Use build script (build.bat):
      ```
      set "PATH=C:\msys64\ucrt64\bin;%PATH%"

      for /f "delims=" %%i in ('pkg-config --cflags --libs freetype2') do set "FREETYPE_FLAGS=%%i"
      
      g++ main.cpp -o engine.exe ^
        -IC:/msys64/ucrt64/include/bullet ^
        -O2 -std=c++17 ^
        -lmingw32 -lSDL2main -lSDL2 ^
        -lBulletDynamics -lBulletCollision -lLinearMath ^
        -lglew32 -lopengl32 -lglu32 ^
        %FREETYPE_FLAGS%

      engine.exe
      ```
