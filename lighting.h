#pragma once
#include <GL/gl.h>
#include "math.h"
#include "consts.h"

class LightingSystem {
public:
    Vec3 sunDir = Vec3(0.55f, 0.78f, 0.30f).normalized();

    void init() {
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glShadeModel(GL_SMOOTH);

        GLfloat zeroSpec[] = {0.0f, 0.0f, 0.0f, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, zeroSpec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalSkyAmbient);

        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_AMBIENT, sunAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, zeroSpec);

        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT1, GL_AMBIENT, skyAmbient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, skyDiffuse);
        glLightfv(GL_LIGHT1, GL_SPECULAR, zeroSpec);
    }

    void applyLights() const {
        GLfloat sunPos[] = {sunDir.x, sunDir.y, sunDir.z, 0.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, sunPos);

        GLfloat skyPos[] = {-sunDir.x * 0.3f, 1.0f, -sunDir.z * 0.3f, 0.0f};
        glLightfv(GL_LIGHT1, GL_POSITION, skyPos);
    }

    void computeShadowMatrix(float mat[16]) const {
        const float plane[4] = {0.0f, 1.0f, 0.0f, 0.0f};
        const float light[4] = {sunDir.x, sunDir.y, sunDir.z, 0.0f};

        float dot = plane[0] * light[0] + plane[1] * light[1] +
                    plane[2] * light[2] + plane[3] * light[3];

        mat[0]  = dot - light[0] * plane[0];
        mat[4]  = 0.0f - light[0] * plane[1];
        mat[8]  = 0.0f - light[0] * plane[2];
        mat[12] = 0.0f - light[0] * plane[3];

        mat[1]  = 0.0f - light[1] * plane[0];
        mat[5]  = dot - light[1] * plane[1];
        mat[9]  = 0.0f - light[1] * plane[2];
        mat[13] = 0.0f - light[1] * plane[3];

        mat[2]  = 0.0f - light[2] * plane[0];
        mat[6]  = 0.0f - light[2] * plane[1];
        mat[10] = dot - light[2] * plane[2];
        mat[14] = 0.0f - light[2] * plane[3];

        mat[3]  = 0.0f - light[3] * plane[0];
        mat[7]  = 0.0f - light[3] * plane[1];
        mat[11] = 0.0f - light[3] * plane[2];
        mat[15] = dot - light[3] * plane[3];
    }

    void drawSkyboxAndSun(const Vec3& camPos) const {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glPushMatrix();
        glTranslatef(camPos.x, camPos.y, camPos.z);

        const int slices = 32;
        const int stacks = 16;
        const float radius = 60.0f;
        const float pi = 3.14159265f;

        auto setSkyColor = [](float ny) {
            float t = std::clamp(ny, 0.0f, 1.0f);
            float curve = std::pow(t, 0.65f);

            Vec3 col = (ny >= 0.0f)
                ? (horizon * (1.0f - curve) + zenith * curve)
                : (horizon * (1.0f - std::clamp(-ny, 0.0f, 1.0f)) + nadir * std::clamp(-ny, 0.0f, 1.0f));

            glColor3f(col.x, col.y, col.z);
        };

        for (int i = 0; i < stacks; ++i) {
            float phi1 = -pi * 0.5f + (static_cast<float>(i) / stacks) * pi;
            float phi2 = -pi * 0.5f + (static_cast<float>(i + 1) / stacks) * pi;

            glBegin(GL_QUAD_STRIP);
            for (int j = 0; j <= slices; ++j) {
                float theta = (static_cast<float>(j) / slices) * 2.0f * pi;

                float y1 = std::sin(phi1), r1 = std::cos(phi1);
                float x1 = r1 * std::cos(theta), z1 = r1 * std::sin(theta);
                float y2 = std::sin(phi2), r2 = std::cos(phi2);
                float x2 = r2 * std::cos(theta), z2 = r2 * std::sin(theta);

                setSkyColor(y2);
                glVertex3f(x2 * radius, y2 * radius, z2 * radius);
                setSkyColor(y1);
                glVertex3f(x1 * radius, y1 * radius, z1 * radius);
            }
            glEnd();
        }

        glPushMatrix();
        Vec3 sunSkyPos = sunDir * 50.0f;
        glTranslatef(sunSkyPos.x, sunSkyPos.y, sunSkyPos.z);
        glColor3f(0.96f, 0.92f, 0.76f);
        float s = 2.0f;
        glBegin(GL_QUADS);
        glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s); glVertex3f(s, s, -s); glVertex3f(-s, s, -s);
        glVertex3f(-s, -s,  s); glVertex3f(s, -s,  s); glVertex3f(s, s,  s); glVertex3f(-s, s,  s);
        glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(-s, s, s); glVertex3f(-s, s, -s);
        glVertex3f( s, -s, -s); glVertex3f( s, -s, s); glVertex3f( s, s, s); glVertex3f( s, s, -s);
        glVertex3f(-s,  s, -s); glVertex3f(-s,  s, s); glVertex3f( s, s, s); glVertex3f( s, s, -s);
        glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f( s,-s, s); glVertex3f( s,-s, -s);
        glEnd();
        glPopMatrix();
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
    }
};