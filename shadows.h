#pragma once

#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL2/SDL.h>
#include <cmath>
#include "math.h"

class ShadowMapping {
public:
    bool init(int mapSize = 2048) {
        mapSize_ = mapSize;

        // шейдеры через клода, я не шарю за glsl
        static const char* vertexSource = R"GLSL(
#version 120
uniform mat4 uLightMatrix;
varying vec3 vNormal;
varying vec4 vColor;
varying vec4 vShadowCoord;

void main() {
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    vNormal = normalize(gl_NormalMatrix * gl_Normal);
    vColor = gl_Color;
    vShadowCoord = uLightMatrix * eyePosition;
    gl_Position = gl_ProjectionMatrix * eyePosition;
}
)GLSL";

        static const char* fragmentSource = R"GLSL(
#version 120
uniform sampler2D uShadowMap;
uniform vec2 uTexelSize;
uniform vec3 uSunDirEye;
varying vec3 vNormal;
varying vec4 vColor;
varying vec4 vShadowCoord;

float shadowVisibility(vec3 normal, vec3 lightDirection) {
    if (vShadowCoord.w <= 0.0) return 1.0;

    vec3 shadow = vShadowCoord.xyz / vShadowCoord.w;
    if (shadow.x < 0.0 || shadow.x > 1.0 ||
        shadow.y < 0.0 || shadow.y > 1.0 ||
        shadow.z < 0.0 || shadow.z > 1.0) {
        return 1.0;
    }

    float bias = max(0.00045, 0.0018 * (1.0 - max(dot(normal, lightDirection), 0.0)));
    float visible = 0.0;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 sampleUV = shadow.xy + vec2(float(x), float(y)) * uTexelSize;
            float storedDepth = texture2D(uShadowMap, sampleUV).r;
            visible += (shadow.z - bias <= storedDepth) ? 1.0 : 0.0;
        }
    }

    return visible / 9.0;
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(uSunDirEye);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float visibility = (diffuse > 0.001) ? shadowVisibility(normal, lightDirection) : 1.0;

    vec3 ambient = vec3(0.22, 0.25, 0.31);
    vec3 sunlight = vec3(0.78, 0.75, 0.92) * diffuse * visibility;
    vec3 finalColor = vColor.rgb * min(ambient + sunlight, vec3(1.0));

    gl_FragColor = vec4(finalColor, vColor.a);
}
)GLSL";

        sceneProgram_ = createProgram(vertexSource, fragmentSource);
        if (!sceneProgram_) return false;

        lightMatrixLocation_ = glGetUniformLocation(sceneProgram_, "uLightMatrix");
        shadowMapLocation_ = glGetUniformLocation(sceneProgram_, "uShadowMap");
        texelSizeLocation_ = glGetUniformLocation(sceneProgram_, "uTexelSize");
        sunDirLocation_ = glGetUniformLocation(sceneProgram_, "uSunDirEye");

        glGenFramebuffers(1, &framebuffer_);
        glGenTextures(1, &depthTexture_);

        glBindTexture(GL_TEXTURE_2D, depthTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mapSize_, mapSize_, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SDL_Log("shadowbuffer incomplete: 0x%X", status);
            cleanup();
            return false;
        }

        return true;
    }

    void beginDepthPass(const Vec3& sunDirection) {
        const Vec3 sun = sunDirection.normalized();
        const GLfloat lightDistance = 35.0f;
        const GLfloat lightX = sun.x * lightDistance;
        const GLfloat lightY = sun.y * lightDistance;
        const GLfloat lightZ = sun.z * lightDistance;

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, mapSize_, mapSize_);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-12.0, 12.0, -12.0, 12.0, 1.0, 80.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(lightX, lightY, lightZ,
                  0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0);

        GLfloat lightProjection[16];
        GLfloat lightView[16];
        glGetFloatv(GL_PROJECTION_MATRIX, lightProjection);
        glGetFloatv(GL_MODELVIEW_MATRIX, lightView);
        multiply(lightProjection, lightView, lightViewProjection_);
    }

    void endDepthPass() {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);
        glEnable(GL_LIGHTING);
        glUseProgram(0);
    }

    void beginScene(const Vec3& sunDirection) {
        GLfloat cameraView[16];
        GLfloat inverseCameraView[16];
        GLfloat lightFromEye[16];
        GLfloat biasedLightFromEye[16];

        glGetFloatv(GL_MODELVIEW_MATRIX, cameraView);
        invertRigidMatrix(cameraView, inverseCameraView);
        multiply(lightViewProjection_, inverseCameraView, lightFromEye);

        static const GLfloat biasMatrix[16] = {
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f
        };
        multiply(biasMatrix, lightFromEye, biasedLightFromEye);

        GLfloat lx = cameraView[0] * sunDirection.x + cameraView[4] * sunDirection.y + cameraView[8] * sunDirection.z;
        GLfloat ly = cameraView[1] * sunDirection.x + cameraView[5] * sunDirection.y + cameraView[9] * sunDirection.z;
        GLfloat lz = cameraView[2] * sunDirection.x + cameraView[6] * sunDirection.y + cameraView[10] * sunDirection.z;
        const GLfloat length = std::sqrt(lx * lx + ly * ly + lz * lz);
        if (length > 0.00001f) {
            lx /= length;
            ly /= length;
            lz /= length;
        }

        glUseProgram(sceneProgram_);
        glUniformMatrix4fv(lightMatrixLocation_, 1, GL_FALSE, biasedLightFromEye);
        glUniform1i(shadowMapLocation_, 0);
        glUniform2f(texelSizeLocation_, 1.0f / mapSize_, 1.0f / mapSize_);
        glUniform3f(sunDirLocation_, lx, ly, lz);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture_);
    }

    void endScene() {
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void cleanup() {
        if (sceneProgram_) {
            glDeleteProgram(sceneProgram_);
            sceneProgram_ = 0;
        }
        if (depthTexture_) {
            glDeleteTextures(1, &depthTexture_);
            depthTexture_ = 0;
        }
        if (framebuffer_) {
            glDeleteFramebuffers(1, &framebuffer_);
            framebuffer_ = 0;
        }
    }

private:
    GLuint framebuffer_ = 0;
    GLuint depthTexture_ = 0;
    GLuint sceneProgram_ = 0;
    GLint lightMatrixLocation_ = -1;
    GLint shadowMapLocation_ = -1;
    GLint texelSizeLocation_ = -1;
    GLint sunDirLocation_ = -1;
    int mapSize_ = 2048;
    GLfloat lightViewProjection_[16] = {};

    static GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success != GL_TRUE) {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 1) {
                char* log = new char[static_cast<size_t>(logLength)];
                glGetShaderInfoLog(shader, logLength, nullptr, log);
                SDL_Log("shader compilation failed: %s", log);
                delete[] log;
            }
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        if (!vertexShader) return 0;

        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!fragmentShader) {
            glDeleteShader(vertexShader);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint success = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (success != GL_TRUE) {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 1) {
                char* log = new char[static_cast<size_t>(logLength)];
                glGetProgramInfoLog(program, logLength, nullptr, log);
                SDL_Log("shader error: %s", log);
                delete[] log;
            }
            glDeleteProgram(program);
            return 0;
        }

        return program;
    }

    static void multiply(const GLfloat a[16], const GLfloat b[16], GLfloat out[16]) {
        GLfloat result[16] = {};
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                for (int k = 0; k < 4; ++k) {
                    result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
                }
            }
        }
        for (int i = 0; i < 16; ++i) out[i] = result[i];
    }

    static void invertRigidMatrix(const GLfloat m[16], GLfloat out[16]) {
        out[0] = m[0];  out[1] = m[4];  out[2] = m[8];   out[3] = 0.0f;
        out[4] = m[1];  out[5] = m[5];  out[6] = m[9];   out[7] = 0.0f;
        out[8] = m[2];  out[9] = m[6];  out[10] = m[10]; out[11] = 0.0f;
        out[12] = -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14]);
        out[13] = -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14]);
        out[14] = -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14]);
        out[15] = 1.0f;
    }
};
