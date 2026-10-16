#pragma once
#include <SDL2/SDL.h>
#include <string>

inline const std::string& exeDir() {
    static const std::string dir = [] {
        char* base = SDL_GetBasePath();
        std::string s = base ? base : "./";
        if (base) SDL_free(base);
        return s;
    }();
    return dir;
}
