#pragma once

// class written by claude

#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class TextHUD {
public:
    TextHUD() = default;
    TextHUD(const TextHUD&) = delete;
    TextHUD& operator=(const TextHUD&) = delete;

    bool init(const unsigned char* fontBytes, unsigned long fontSize, unsigned int pixelHeight = 20) {
        cleanup();
        if (!fontBytes || fontSize == 0 || FT_Init_FreeType(&ft_) != 0) return false;
        ftReady_ = true;
        if (FT_New_Memory_Face(ft_, fontBytes, static_cast<FT_Long>(fontSize), 0, &face_) != 0) {
            cleanup();
            return false;
        }
        if (FT_Set_Pixel_Sizes(face_, 0, pixelHeight) != 0) {
            cleanup();
            return false;
        }
        lineHeight_ = static_cast<float>(face_->size->metrics.height >> 6);
        if (lineHeight_ < static_cast<float>(pixelHeight)) lineHeight_ = static_cast<float>(pixelHeight);
        ascender_ = static_cast<float>(face_->size->metrics.ascender >> 6);
        return true;
    }

    // x/y are in screen pixels, with (0,0) at the top-left. UTF-8 is supported.
    void drawText(float x, float y, const std::string& utf8,
                  int windowWidth, int windowHeight,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f,
                  float scale = 1.0f) {
        if (!face_ || windowWidth <= 0 || windowHeight <= 0) return;

        GLint previousProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, static_cast<GLdouble>(windowWidth),
                static_cast<GLdouble>(windowHeight), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glUseProgram(0);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glColor4f(r, g, b, 1.0f);
        glActiveTexture(GL_TEXTURE0);

        float penX = x;
        float lineTop = y;
        for (std::size_t i = 0; i < utf8.size();) {
            std::uint32_t cp = decodeUTF8(utf8, i);
            if (cp == '\n') {
                penX = x;
                lineTop += lineHeight_ * scale;
                continue;
            }
            if (cp == '\r') continue;
            if (cp == '\t') {
                penX += advanceFor(' ') * 4.0f * scale;
                continue;
            }
            if (!loadGlyph(cp)) cp = '?';
            auto it = glyphs_.find(cp);
            if (it == glyphs_.end()) continue;
            const Glyph& glyph = it->second;
            if (glyph.texture && glyph.width > 0 && glyph.height > 0) {
                const float left = penX + glyph.bearingX * scale;
                const float top = lineTop + (ascender_ - glyph.bearingY) * scale;
                const float right = left + glyph.width * scale;
                const float bottom = top + glyph.height * scale;
                glBindTexture(GL_TEXTURE_2D, glyph.texture);
                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(left, top);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f(right, top);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f(right, bottom);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(left, bottom);
                glEnd();
            }
            penX += glyph.advanceX * scale;
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();
        glUseProgram(static_cast<GLuint>(previousProgram));
    }

    void cleanup() {
        for (auto& pair : glyphs_) {
            if (pair.second.texture) glDeleteTextures(1, &pair.second.texture);
        }
        glyphs_.clear();
        if (face_) { FT_Done_Face(face_); face_ = nullptr; }
        if (ftReady_) { FT_Done_FreeType(ft_); ftReady_ = false; }
        lineHeight_ = 20.0f;
        ascender_ = 16.0f;
    }

    ~TextHUD() { cleanup(); }

private:
    struct Glyph {
        GLuint texture = 0;
        int width = 0, height = 0;
        int bearingX = 0, bearingY = 0;
        float advanceX = 0.0f;
    };

    FT_Library ft_ = nullptr;
    FT_Face face_ = nullptr;
    bool ftReady_ = false;
    float lineHeight_ = 20.0f;
    float ascender_ = 16.0f;
    std::unordered_map<std::uint32_t, Glyph> glyphs_;

    static std::uint32_t decodeUTF8(const std::string& s, std::size_t& i) {
        const auto first = static_cast<unsigned char>(s[i++]);
        if (first < 0x80) return first;
        if ((first & 0xE0) == 0xC0 && i < s.size()) {
            std::uint32_t cp = (first & 0x1F) << 6;
            cp |= static_cast<unsigned char>(s[i++]) & 0x3F;
            return cp;
        }
        if ((first & 0xF0) == 0xE0 && i + 1 < s.size()) {
            std::uint32_t cp = (first & 0x0F) << 12;
            cp |= (static_cast<unsigned char>(s[i++]) & 0x3F) << 6;
            cp |= static_cast<unsigned char>(s[i++]) & 0x3F;
            return cp;
        }
        if ((first & 0xF8) == 0xF0 && i + 2 < s.size()) {
            std::uint32_t cp = (first & 0x07) << 18;
            cp |= (static_cast<unsigned char>(s[i++]) & 0x3F) << 12;
            cp |= (static_cast<unsigned char>(s[i++]) & 0x3F) << 6;
            cp |= static_cast<unsigned char>(s[i++]) & 0x3F;
            return cp;
        }
        return '?';
    }

    float advanceFor(std::uint32_t cp) {
        if (!loadGlyph(cp)) cp = '?';
        auto it = glyphs_.find(cp);
        return it == glyphs_.end() ? 8.0f : it->second.advanceX;
    }

    bool loadGlyph(std::uint32_t cp) {
        if (glyphs_.find(cp) != glyphs_.end()) return true;
        if (FT_Load_Char(face_, static_cast<FT_ULong>(cp), FT_LOAD_RENDER) != 0) return false;
        const FT_GlyphSlot slot = face_->glyph;
        Glyph glyph;
        glyph.width = static_cast<int>(slot->bitmap.width);
        glyph.height = static_cast<int>(slot->bitmap.rows);
        glyph.bearingX = slot->bitmap_left;
        glyph.bearingY = slot->bitmap_top;
        glyph.advanceX = static_cast<float>(slot->advance.x >> 6);

        if (glyph.width > 0 && glyph.height > 0) {
            std::vector<unsigned char> pixels(static_cast<std::size_t>(glyph.width) * glyph.height);
            const int pitch = slot->bitmap.pitch;
            for (int row = 0; row < glyph.height; ++row) {
                const unsigned char* source = pitch >= 0
                    ? slot->bitmap.buffer + row * pitch
                    : slot->bitmap.buffer + (glyph.height - 1 - row) * (-pitch);
                std::copy(source, source + glyph.width,
                          pixels.begin() + static_cast<std::size_t>(row) * glyph.width);
            }
            glGenTextures(1, &glyph.texture);
            glBindTexture(GL_TEXTURE_2D, glyph.texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, glyph.width, glyph.height,
                         0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glyphs_.emplace(cp, glyph);
        return true;
    }
};
