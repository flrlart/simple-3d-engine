#pragma once
#include <GL/glew.h>
#include "vec3.h"
#include "consts.h"
#include "geometry.h"

class Sky {
public:
    void draw(const Vec3& camPos) const {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glPushMatrix();
        glTranslatef(camPos.x, camPos.y, camPos.z);

        drawDome();
        drawSunDisc();

        glPopMatrix();

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

private:
    static void setSkyColor(float ny) {
        using namespace consts;
        const float t = std::clamp(ny, 0.0f, 1.0f);
        const float curve = std::pow(t, SKY_GRADIENT_CURVE);

        const Vec3 col = (ny >= 0.0f)
            ? (SKY_HORIZON * (1.0f - curve) + SKY_ZENITH * curve)
            : (SKY_HORIZON * (1.0f - std::clamp(-ny, 0.0f, 1.0f)) + SKY_NADIR * std::clamp(-ny, 0.0f, 1.0f));

        glColor3f(col.x, col.y, col.z);
    }

    static void drawDome() {
        using namespace consts;
        for (int i = 0; i < SKY_STACKS; ++i) {
            const float phi1 = -PI * 0.5f + (static_cast<float>(i) / SKY_STACKS) * PI;
            const float phi2 = -PI * 0.5f + (static_cast<float>(i + 1) / SKY_STACKS) * PI;

            const float y1 = std::sin(phi1), r1 = std::cos(phi1);
            const float y2 = std::sin(phi2), r2 = std::cos(phi2);

            glBegin(GL_QUAD_STRIP);
            for (int j = 0; j <= SKY_SLICES; ++j) {
                const float theta = (static_cast<float>(j) / SKY_SLICES) * 2.0f * PI;
                const float c = std::cos(theta), s = std::sin(theta);

                setSkyColor(y2);
                glVertex3f(r2 * c * SKY_RADIUS, y2 * SKY_RADIUS, r2 * s * SKY_RADIUS);
                setSkyColor(y1);
                glVertex3f(r1 * c * SKY_RADIUS, y1 * SKY_RADIUS, r1 * s * SKY_RADIUS);
            }
            glEnd();
        }
    }

    static void drawSunDisc() {
        using namespace consts;
        const Vec3 pos = SUN_DIR * SUN_DISC_DISTANCE;
        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glScalef(SUN_DISC_HALF_SIZE, SUN_DISC_HALF_SIZE, SUN_DISC_HALF_SIZE);
        glColor3f(SUN_DISC_COLOR.x, SUN_DISC_COLOR.y, SUN_DISC_COLOR.z);
        drawUnitCube();
        glPopMatrix();
    }
};
