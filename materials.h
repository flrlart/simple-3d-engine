#pragma once
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include "textures.h"

struct MaterialDetails {
    int r = 255, g = 255, b = 255;
    float tiling_x = 1.0f, tiling_y = 1.0f;
    float offset_x = 0.0f, offset_y = 0.0f;
    float metallic = 0.0f;
    float roughness = 0.8f;
    float normalStrength = 1.0f;
    bool normalFlipY = false;
};

struct Material {
    std::string id;
    Texture* texture = nullptr;
    Texture* normalMap = nullptr;
    Texture* metalMap = nullptr;
    MaterialDetails details;
};

class Materials {
public:
    Materials() = delete;

    static Material* Create(const char* id, Texture* tex, const MaterialDetails details = MaterialDetails(),
                            Texture* normalMap = nullptr, Texture* metalMap = nullptr) {
        if (!id || !*id) {
            SDL_Log("mat id is empty");
            return nullptr;
        }
        if (registry_.find(id) != registry_.end()) {
            SDL_Log("mat id already in use: %s", id);
            return nullptr;
        }
        Material* mat = new Material();
        mat->id = id;
        mat->texture = tex;
        mat->normalMap = normalMap;
        mat->metalMap = metalMap;
        mat->details = details;
        registry_[mat->id] = mat;
        return mat;
    }

    static Material* Find(const char* id) {
        if (!id) return nullptr;
        auto it = registry_.find(id);
        return it == registry_.end() ? nullptr : it->second;
    }

    static void Destroy(const char* id) {
        auto it = id ? registry_.find(id) : registry_.end();
        if (it == registry_.end()) {
            SDL_Log("mat not found: %s", id ? id : "(null)");
            return;
        }
        delete it->second;
        registry_.erase(it);
    }

    static void DestroyAll() {
        for (auto& pair : registry_) delete pair.second;
        registry_.clear();
        delete default_;
        default_ = nullptr;
    }

    static bool UsesTexture(const Texture* tex) {
        for (const auto& pair : registry_) {
            const Material* m = pair.second;
            if (m->texture == tex || m->normalMap == tex || m->metalMap == tex) return true;
        }
        return false;
    }

    static constexpr int kUnitAlbedo = 1;
    static constexpr int kUnitNormal = 6;
    static constexpr int kUnitMetal  = 7;

    static void Attach(GLint tintLoc, GLint tilingLoc, GLint offsetLoc,
                       GLint metallicLoc, GLint roughnessLoc, GLint normalStrengthLoc, GLint normalYSignLoc) {
        tintLoc_ = tintLoc;
        tilingLoc_ = tilingLoc;
        offsetLoc_ = offsetLoc;
        metallicLoc_ = metallicLoc;
        roughnessLoc_ = roughnessLoc;
        normalStrengthLoc_ = normalStrengthLoc;
        normalYSignLoc_ = normalYSignLoc;
        attached_ = true;
    }

    static void Detach() {
        attached_ = false;
        const int units[3] = {kUnitAlbedo, kUnitNormal, kUnitMetal};
        for (int unit : units) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
    }

    static void Bind(const Material* mat) {
        if (!attached_) return;
        if (!mat) mat = defaultMaterial();
        const Texture* albedo = mat->texture ? mat->texture : Textures::White();
        const Texture* normal = mat->normalMap ? mat->normalMap : Textures::FlatNormal();
        const Texture* metal  = mat->metalMap ? mat->metalMap : Textures::White();
        const MaterialDetails& d = mat->details;

        glActiveTexture(GL_TEXTURE0 + kUnitAlbedo);
        glBindTexture(GL_TEXTURE_2D, albedo->id);
        glActiveTexture(GL_TEXTURE0 + kUnitNormal);
        glBindTexture(GL_TEXTURE_2D, normal->id);
        glActiveTexture(GL_TEXTURE0 + kUnitMetal);
        glBindTexture(GL_TEXTURE_2D, metal->id);
        glActiveTexture(GL_TEXTURE0);

        glUniform3f(tintLoc_, d.r / 255.0f, d.g / 255.0f, d.b / 255.0f);
        glUniform2f(tilingLoc_, d.tiling_x, d.tiling_y);
        glUniform2f(offsetLoc_, d.offset_x, d.offset_y);
        glUniform1f(metallicLoc_, d.metallic);
        glUniform1f(roughnessLoc_, d.roughness);
        glUniform1f(normalStrengthLoc_, d.normalStrength);
        glUniform1f(normalYSignLoc_, d.normalFlipY ? -1.0f : 1.0f);
    }

private:
    inline static std::unordered_map<std::string, Material*> registry_;
    inline static Material* default_ = nullptr;
    inline static GLint tintLoc_ = -1, tilingLoc_ = -1, offsetLoc_ = -1;
    inline static GLint metallicLoc_ = -1, roughnessLoc_ = -1, normalStrengthLoc_ = -1, normalYSignLoc_ = -1;
    inline static bool attached_ = false;

    static const Material* defaultMaterial() {
        if (!default_) {
            default_ = new Material();
            default_->id = "__default";
        }
        return default_;
    }
};
