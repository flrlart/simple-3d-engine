#pragma once
#include <GL/glew.h>

inline void drawUnitCube(float uvX = 1.0f, float uvY = 1.0f, float uvZ = 1.0f) {
    auto V = [](float x, float y, float z, float u, float v) {
        glTexCoord2f(u, v);
        glVertex3f(x, y, z);
    };
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);    // верх
    V(-1, 1, -1, 0, 0);   V(-1, 1, 1, 0, uvZ);   V(1, 1, 1, uvX, uvZ);   V(1, 1, -1, uvX, 0);
    glNormal3f(0, -1, 0);   // низ
    V(-1, -1, -1, 0, 0);  V(1, -1, -1, uvX, 0);  V(1, -1, 1, uvX, uvZ);  V(-1, -1, 1, 0, uvZ);
    glNormal3f(0, 0, 1);    // вперёд
    V(-1, -1, 1, 0, uvY); V(1, -1, 1, uvX, uvY); V(1, 1, 1, uvX, 0);     V(-1, 1, 1, 0, 0);
    glNormal3f(0, 0, -1);   // назад
    V(-1, -1, -1, uvX, uvY); V(-1, 1, -1, uvX, 0); V(1, 1, -1, 0, 0);    V(1, -1, -1, 0, uvY);
    glNormal3f(1, 0, 0);    // лево
    V(1, -1, -1, uvZ, uvY);  V(1, 1, -1, uvZ, 0);  V(1, 1, 1, 0, 0);     V(1, -1, 1, 0, uvY);
    glNormal3f(-1, 0, 0);   // право
    V(-1, -1, -1, 0, uvY);   V(-1, -1, 1, uvZ, uvY); V(-1, 1, 1, uvZ, 0); V(-1, 1, -1, 0, 0);
    glEnd();
}
