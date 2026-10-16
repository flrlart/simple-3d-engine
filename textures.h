#pragma once
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include "paths.h"

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    std::string path;
};

class Textures {
public:
    Textures() = delete;

    static void Preload(const char* path) { Load(path); }

    static Texture* Load(const char* path) {
        if (!path || !*path) return nullptr;
        auto it = cache_.find(path);
        if (it != cache_.end()) return it->second;

        Texture* tex = new Texture();
        tex->path = path;
        if (!upload(*tex)) {
            delete tex;
            return nullptr;
        }
        cache_[tex->path] = tex;
        return tex;
    }

    static void Unload(const char* path) {
        auto it = path ? cache_.find(path) : cache_.end();
        if (it == cache_.end()) {
            SDL_Log("Texture never been loaded: %s", path ? path : "(null)");
            return;
        }
        destroy(it->second);
        cache_.erase(it);
    }

    static Texture* Reload(const char* path) {
        if (!path || !*path) return nullptr;
        auto it = cache_.find(path);
        if (it == cache_.end()) return Load(path);

        Texture fresh;
        fresh.path = path;
        if (!upload(fresh)) {
            SDL_Log("Texture reload failed, keeping the old one: %s", path);
            return it->second;
        }
        Texture* tex = it->second;
        if (tex->id) glDeleteTextures(1, &tex->id);
        tex->id = fresh.id;
        tex->width = fresh.width;
        tex->height = fresh.height;
        return tex;
    }

    static void UnloadAll() {
        for (auto& pair : cache_) destroy(pair.second);
        cache_.clear();
        if (white_) { destroy(white_); white_ = nullptr; }
        if (flatNormal_) { destroy(flatNormal_); flatNormal_ = nullptr; }
        if (imgReady_) { IMG_Quit(); imgReady_ = false; }
    }

    static Texture* White() {
        if (!white_) {
            white_ = new Texture();
            white_->path = "__white";
            white_->width = white_->height = 1;
            const Uint32 pixel = 0xFFFFFFFFu;
            white_->id = createGLTexture(1, 1, &pixel, 1);
        }
        return white_;
    }

    static Texture* FlatNormal() {
        if (!flatNormal_) {
            flatNormal_ = new Texture();
            flatNormal_->path = "__flat_normal";
            flatNormal_->width = flatNormal_->height = 1;
            const unsigned char pixel[4] = {128, 128, 255, 255};
            flatNormal_->id = createGLTexture(1, 1, pixel, 1);
        }
        return flatNormal_;
    }

private:
    inline static std::unordered_map<std::string, Texture*> cache_;
    inline static Texture* white_ = nullptr;
    inline static Texture* flatNormal_ = nullptr;
    inline static bool imgReady_ = false;

    static void destroy(Texture* tex) {
        if (tex->id) glDeleteTextures(1, &tex->id);
        delete tex;
    }

    static std::string fullPath(const std::string& relative) {
        return exeDir() + "textures/" + relative;
    }

    static void ensureImageLib() {
        if (imgReady_) return;
        const int flags = IMG_INIT_PNG | IMG_INIT_JPG;
        if ((IMG_Init(flags) & flags) != flags) {
            SDL_Log("not all image formats available: %s", IMG_GetError());
        }
        imgReady_ = true;
    }

    static bool upload(Texture& tex) {
        ensureImageLib();
        const std::string full = fullPath(tex.path);

        SDL_Surface* loaded = IMG_Load(full.c_str());
        if (!loaded) {
            SDL_Log("texture load failed '%s': %s", full.c_str(), IMG_GetError());
            return false;
        }
        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(loaded);
        if (!rgba) {
            SDL_Log("texture convert failed '%s': %s", full.c_str(), SDL_GetError());
            return false;
        }

        tex.width = rgba->w;
        tex.height = rgba->h;
        tex.id = createGLTexture(rgba->w, rgba->h, rgba->pixels, rgba->pitch / 4);
        SDL_FreeSurface(rgba);
        return tex.id != 0;
    }

    static GLuint createGLTexture(int w, int h, const void* pixels, int rowLengthPixels) {
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLengthPixels);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        return id;
    }
};
