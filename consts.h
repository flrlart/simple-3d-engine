#pragma once
#include <GL/gl.h>
#include "math.h"

GLfloat globalSkyAmbient[] = { 0.22f, 0.24f, 0.29f, 1.0f };
GLfloat sunAmbient[] = { 0.30f, 0.30f,  0.30f,  1.0f };
GLfloat sunDiffuse[] = { 1.00f, 0.88f,  0.68f,  1.0f };
GLfloat skyAmbient[] = { 0.10f, 0.10f,  0.10f,  1.0f };
GLfloat skyDiffuse[] = { 0.20f, 0.28f,  0.42f,  1.0f };

Vec3 zenith  = {0.28f, 0.42f, 0.78f}; // верх неба
Vec3 horizon = {0.56f, 0.78f, 0.90f}; // горизонт
Vec3 nadir   = {0.26f, 0.44f, 0.64f}; // нижняя часть