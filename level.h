#pragma once
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <SDL2/SDL.h>
#include "paths.h"
#include "world.h"
#include "materials.h"
#include "textures.h"

class Level {
private:
    struct ObjectDef {
        bool dynamic = false;
        Vec3 pos, rot, size;
        std::string material;
    };

public:
    Level() = delete;

    static bool Load(GameWorld& world, const char* path) {
        const std::string full = exeDir() + path;
        std::ifstream in(full);
        if (!in) {
            SDL_Log("level file not found: %s", full.c_str());
            return false;
        }

        Unload(world);

        std::vector<ObjectDef> objects;
        std::string line;
        int lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);

            std::istringstream ss(line);
            std::string cmd;
            if (!(ss >> cmd)) continue;

            bool ok = true;
            if (cmd == "respawn")                                   ok = parseRespawn(ss, world);
            else if (cmd == "material" || cmd == "materialpersist") ok = parseMaterial(ss, cmd == "materialpersist");
            else if (cmd == "spawn")                                ok = parseSpawn(ss, world);
            else if (cmd == "static" || cmd == "box")               ok = parseObject(ss, cmd == "box", objects);
            else {
                SDL_Log("%s:%d: unknown command '%s'", path, lineNo, cmd.c_str());
                continue;
            }
            if (!ok) SDL_Log("%s:%d: bad arguments for '%s'", path, lineNo, cmd.c_str());
        }

        for (const ObjectDef& d : objects) {
            Material* mat = nullptr;
            if (!d.material.empty()) {
                mat = Materials::Find(d.material.c_str());
                if (!mat) SDL_Log("%s: material '%s' not found", path, d.material.c_str());
            }
            world.addObject(d.dynamic, d.pos, d.rot, d.size, mat);
        }

        current_ = path;
        SDL_Log("Level loaded: %s (%zu static, %zu box)", path, world.statics.size(), world.boxes.size());
        return true;
    }

    static void Unload(GameWorld& world) {
        world.clearLevel();

        for (const std::string& id : levelMaterials_) Materials::Destroy(id.c_str());
        levelMaterials_.clear();

        for (const auto& pair : levelTextures_) {
            if (pair.second && !Materials::UsesTexture(pair.second)) Textures::Unload(pair.first.c_str());
        }
        levelTextures_.clear();
        current_.clear();
    }

    static const std::string& Current() { return current_; }

private:
    inline static std::vector<std::string> levelMaterials_;
    inline static std::unordered_map<std::string, Texture*> levelTextures_;
    inline static std::unordered_set<std::string> persistent_;
    inline static std::string current_;

    static bool parsePair(const std::string& tok, float& a, float& b) {
        const char* s = tok.c_str();
        char* end = nullptr;
        a = std::strtof(s, &end);
        if (end == s) return false;
        if (*end == '\0') { b = a; return true; }
        if (*end != ';') return false;
        const char* second = end + 1;
        b = std::strtof(second, &end);
        return end != second && *end == '\0';
    }

    static bool parseRespawn(std::istringstream& ss, GameWorld& world) {
        float y;
        if (!(ss >> y)) return false;
        world.respawnY = y;
        return true;
    }

    static bool parseSpawn(std::istringstream& ss, GameWorld& world) {
        float x, y, z, angle = 0.0f;
        if (!(ss >> x >> y >> z)) return false;
        ss >> angle;
        world.playerSpawnPos = Vec3(x, y, z);
        world.playerSpawnYaw = angle;
        return true;
    }

    static bool parseFloat(const std::string& tok, float& out) {
        const char* s = tok.c_str();
        char* end = nullptr;
        out = std::strtof(s, &end);
        return end != s && *end == '\0';
    }

    static bool parseMaterial(std::istringstream& ss, bool persist) {
        std::string id, path;
        int r, g, b;
        if (!(ss >> id >> path >> r >> g >> b)) return false;

        MaterialDetails d;
        d.r = r; d.g = g; d.b = b;
        std::string normalPath, metalPath;
        int positional = 0;
        std::string tok;
        while (ss >> tok) {
            const size_t eq = tok.find('=');
            if (eq == std::string::npos) {
                if (positional == 0) { if (!parsePair(tok, d.tiling_x, d.tiling_y)) return false; }
                else if (positional == 1) { if (!parsePair(tok, d.offset_x, d.offset_y)) return false; }
                else return false;
                ++positional;
                continue;
            }
            const std::string key = tok.substr(0, eq);
            const std::string val = tok.substr(eq + 1);
            float f = 0.0f;
            if (key == "normalmap")            normalPath = (val == "-") ? "" : val;
            else if (key == "metalmap")        metalPath = (val == "-") ? "" : val;
            else if (key == "metallic")        { if (!parseFloat(val, f)) return false; d.metallic = f; }
            else if (key == "roughness")       { if (!parseFloat(val, f)) return false; d.roughness = f; }
            else if (key == "normalstrength")  { if (!parseFloat(val, f)) return false; d.normalStrength = f; }
            else if (key == "normalflipy")     { if (!parseFloat(val, f)) return false; d.normalFlipY = (f != 0.0f); }
            else return false;
        }

        if (Materials::Find(id.c_str()) && persistent_.count(id)) return true;

        Texture* tex = (path == "-") ? nullptr : Textures::Load(path.c_str());
        Texture* normalTex = normalPath.empty() ? nullptr : Textures::Load(normalPath.c_str());
        Texture* metalTex = metalPath.empty() ? nullptr : Textures::Load(metalPath.c_str());
        if (!persist) {
            if (tex) levelTextures_[path] = tex;
            if (normalTex) levelTextures_[normalPath] = normalTex;
            if (metalTex) levelTextures_[metalPath] = metalTex;
        }

        if (Material* mat = Materials::Create(id.c_str(), tex, d, normalTex, metalTex)) {
            if (persist) persistent_.insert(id);
            else levelMaterials_.push_back(mat->id);
        }
        return true;
    }

    static bool parseObject(std::istringstream& ss, bool dynamic, std::vector<ObjectDef>& out) {
        ObjectDef d;
        d.dynamic = dynamic;
        if (!(ss >> d.pos.x >> d.pos.y >> d.pos.z
                 >> d.rot.x >> d.rot.y >> d.rot.z
                 >> d.size.x >> d.size.y >> d.size.z)) return false;
        ss >> d.material;
        out.push_back(d);
        return true;
    }
};
