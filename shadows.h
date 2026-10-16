#pragma once

#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "vec3.h"
#include "consts.h"
#include "materials.h"

struct ShadowCamera {
    Vec3 pos;
    Vec3 forward;
    Vec3 right;
    float fovY = 1.0f;
    float aspect = 1.0f;
};

class ShadowMapping {
public:
    static constexpr int kCascades = consts::SHADOW_CASCADE_COUNT;
    static_assert(kCascades == 4, "only 4 cascades supported");

    bool init(int baseSize = consts::SHADOW_MAP_SIZE) {
        static const char* vertexSource = R"GLSL(
#version 120
uniform vec2 uTiling;
uniform vec2 uOffset;
varying vec2 vTexCoord;
varying vec3 vNormal;
varying vec4 vColor;
varying vec3 vEyePos;

void main() {
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    vNormal = normalize(gl_NormalMatrix * gl_Normal);
    vColor = gl_Color;
    vTexCoord = gl_MultiTexCoord0.xy * uTiling + uOffset;
    vEyePos = eyePosition.xyz;
    gl_Position = gl_ProjectionMatrix * eyePosition;
}
)GLSL";

        static const char* fragmentSource = R"GLSL(
#version 120
uniform sampler2D uAlbedo;
uniform vec3 uTint;

uniform sampler2D uShadowMap0;
uniform sampler2D uShadowMap1;
uniform sampler2D uShadowMap2;
uniform sampler2D uShadowMap3;
uniform mat4 uLightMatrix0;
uniform mat4 uLightMatrix1;
uniform mat4 uLightMatrix2;
uniform mat4 uLightMatrix3;
uniform vec4 uSplitFar;
uniform vec4 uTexel;
uniform vec4 uBiasMin;
uniform vec4 uBiasSlope;
uniform float uFadeStart;

uniform vec3 uSunDirEye;
uniform vec3 uAmbient;
uniform vec3 uSunColor;

uniform sampler2D uNormalMap;
uniform sampler2D uMetalMap;
uniform float uNormalStrength;
uniform float uNormalYSign;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uUpEye;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyNadir;
uniform float uSkyCurve;

varying vec2 vTexCoord;
varying vec3 vNormal;
varying vec4 vColor;
varying vec3 vEyePos;

bool insideMap(vec3 s) {
    return s.x >= 0.0 && s.x <= 1.0 &&
           s.y >= 0.0 && s.y <= 1.0 &&
           s.z >= 0.0 && s.z <= 1.0;
}

float filterShadow(sampler2D map, vec3 s, float texel, float bias) {
    float visible = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 uv = s.xy + vec2(float(x), float(y)) * texel;
            float stored = texture2D(map, uv).r;
            visible += (s.z - bias <= stored) ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
}

float shadowVisibility(vec3 normal, vec3 lightDirection) {
    float depth = -vEyePos.z;
    if (depth >= uSplitFar.w) return 1.0; // 4 каскад

    float slope = 1.0 - max(dot(normal, lightDirection), 0.0);
    vec4 eye = vec4(vEyePos, 1.0);
    float visible = 1.0;

    if (depth < uSplitFar.x) {
        vec3 s = (uLightMatrix0 * eye).xyz;
        if (insideMap(s)) visible = filterShadow(uShadowMap0, s, uTexel.x, max(uBiasMin.x, uBiasSlope.x * slope));
    } else if (depth < uSplitFar.y) {
        vec3 s = (uLightMatrix1 * eye).xyz;
        if (insideMap(s)) visible = filterShadow(uShadowMap1, s, uTexel.y, max(uBiasMin.y, uBiasSlope.y * slope));
    } else if (depth < uSplitFar.z) {
        vec3 s = (uLightMatrix2 * eye).xyz;
        if (insideMap(s)) visible = filterShadow(uShadowMap2, s, uTexel.z, max(uBiasMin.z, uBiasSlope.z * slope));
    } else {
        vec3 s = (uLightMatrix3 * eye).xyz;
        if (insideMap(s)) visible = filterShadow(uShadowMap3, s, uTexel.w, max(uBiasMin.w, uBiasSlope.w * slope));
    }

    float fade = smoothstep(uFadeStart, uSplitFar.w, depth);
    return mix(visible, 1.0, fade);
}

mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
    return mat3(T * invmax, B * invmax, N);
}

void main() {
    vec3 geoNormal = normalize(vNormal);
    vec3 lightDirection = normalize(uSunDirEye);
    vec3 viewDirection = normalize(-vEyePos);

    vec3 tn = texture2D(uNormalMap, vTexCoord).xyz * 2.0 - 1.0;
    tn.xy *= uNormalStrength;
    tn.y *= -uNormalYSign;
    vec3 normal = normalize(cotangentFrame(geoNormal, vEyePos, vTexCoord) * tn);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float visibility = (diffuse > 0.001) ? shadowVisibility(geoNormal, lightDirection) : 1.0;

    vec3 albedo = vColor.rgb * uTint * texture2D(uAlbedo, vTexCoord).rgb;
    float metal = clamp(uMetallic * texture2D(uMetalMap, vTexCoord).r, 0.0, 1.0);
    vec3 f0 = mix(vec3(0.04), albedo, metal);

    float gloss = 1.0 - uRoughness;
    float shininess = mix(8.0, 256.0, gloss * gloss);
    vec3 halfVec = normalize(lightDirection + viewDirection);
    float specPower = pow(max(dot(normal, halfVec), 0.0), shininess) * (shininess + 8.0) / 25.13;
    float facing = smoothstep(0.0, 0.1, dot(geoNormal, lightDirection));
    vec3 specular = f0 * uSunColor * specPower * diffuse * visibility * facing;

    float nDotV = max(dot(normal, viewDirection), 0.0);
    vec3 fresnel = f0 + (max(vec3(gloss), f0) - f0) * pow(1.0 - nDotV, 5.0);
    float h = dot(reflect(-viewDirection, normal), uUpEye);
    float t = pow(clamp(abs(h), 0.0, 1.0), uSkyCurve);
    vec3 sky = (h > 0.0) ? mix(uSkyHorizon, uSkyZenith, t) : mix(uSkyHorizon, uSkyNadir, t);
    vec3 skyReflection = fresnel * sky * gloss * gloss;

    vec3 sunlight = uSunColor * diffuse * visibility;
    vec3 diffuseColor = albedo * (1.0 - metal);
    vec3 finalColor = diffuseColor * min(uAmbient + sunlight, vec3(1.0)) + specular + skyReflection;

    gl_FragColor = vec4(finalColor, vColor.a);
}
)GLSL";

        sceneProgram_ = createProgram(vertexSource, fragmentSource);
        if (!sceneProgram_) return false;

        for (int i = 0; i < kCascades; ++i) {
            const std::string n = std::to_string(i);
            lightMatrixLoc_[i] = glGetUniformLocation(sceneProgram_, ("uLightMatrix" + n).c_str());
            shadowMapLoc_[i] = glGetUniformLocation(sceneProgram_, ("uShadowMap" + n).c_str());
        }
        splitFarLoc_ = glGetUniformLocation(sceneProgram_, "uSplitFar");
        texelLoc_ = glGetUniformLocation(sceneProgram_, "uTexel");
        biasMinLoc_ = glGetUniformLocation(sceneProgram_, "uBiasMin");
        biasSlopeLoc_ = glGetUniformLocation(sceneProgram_, "uBiasSlope");
        fadeStartLoc_ = glGetUniformLocation(sceneProgram_, "uFadeStart");
        sunDirLoc_ = glGetUniformLocation(sceneProgram_, "uSunDirEye");
        ambientLoc_ = glGetUniformLocation(sceneProgram_, "uAmbient");
        sunColorLoc_ = glGetUniformLocation(sceneProgram_, "uSunColor");
        albedoLoc_ = glGetUniformLocation(sceneProgram_, "uAlbedo");
        tintLoc_ = glGetUniformLocation(sceneProgram_, "uTint");
        tilingLoc_ = glGetUniformLocation(sceneProgram_, "uTiling");
        offsetLoc_ = glGetUniformLocation(sceneProgram_, "uOffset");
        normalMapLoc_ = glGetUniformLocation(sceneProgram_, "uNormalMap");
        metalMapLoc_ = glGetUniformLocation(sceneProgram_, "uMetalMap");
        normalStrengthLoc_ = glGetUniformLocation(sceneProgram_, "uNormalStrength");
        normalYSignLoc_ = glGetUniformLocation(sceneProgram_, "uNormalYSign");
        metallicLoc_ = glGetUniformLocation(sceneProgram_, "uMetallic");
        roughnessLoc_ = glGetUniformLocation(sceneProgram_, "uRoughness");
        upEyeLoc_ = glGetUniformLocation(sceneProgram_, "uUpEye");
        skyZenithLoc_ = glGetUniformLocation(sceneProgram_, "uSkyZenith");
        skyHorizonLoc_ = glGetUniformLocation(sceneProgram_, "uSkyHorizon");
        skyNadirLoc_ = glGetUniformLocation(sceneProgram_, "uSkyNadir");
        skyCurveLoc_ = glGetUniformLocation(sceneProgram_, "uSkyCurve");

        for (int i = 0; i < kCascades; ++i) {
            mapSize_[i] = std::max(64, static_cast<int>(std::lround(baseSize * consts::SHADOW_CASCADE_QUALITY[i])));
            if (!createCascadeTarget(i)) {
                cleanup();
                return false;
            }
        }
        return true;
    }

    void update(const ShadowCamera& cam, const Vec3& sunDirection) {
        sunDir_ = sunDirection.normalized();

        lightUpRef_ = (std::fabs(sunDir_.y) > 0.999f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
        const Vec3 lightForward = sunDir_ * -1.0f;
        lightRight_ = cross3(lightForward, lightUpRef_).normalized();
        lightUp_ = cross3(lightRight_, lightForward);

        const float tanHalf = std::tan(cam.fovY * 0.5f);
        const float diagonal = std::sqrt(1.0f + cam.aspect * cam.aspect);
        const Vec3 camUp = cross3(cam.right, cam.forward);
        (void)camUp;

        float nearDist = static_cast<float>(consts::CAMERA_NEAR_PLANE);
        for (int i = 0; i < kCascades; ++i) {
            const float farDist = std::max(consts::SHADOW_CASCADE_FAR[i], nearDist + 0.01f);

            const float rn = nearDist * tanHalf * diagonal;
            const float rf = farDist * tanHalf * diagonal;
            float t = (farDist * farDist - nearDist * nearDist + rf * rf - rn * rn) / (2.0f * (farDist - nearDist));
            t = std::clamp(t, nearDist, farDist);
            float radius = std::max(std::sqrt((t - nearDist) * (t - nearDist) + rn * rn),
                                    std::sqrt((farDist - t) * (farDist - t) + rf * rf));
            radius = std::ceil(radius * 16.0f) / 16.0f;
            Vec3 center = cam.pos + cam.forward * t;

            const float texelWorld = 2.0f * radius / static_cast<float>(mapSize_[i]);
            const float cx = dot3(center, lightRight_);
            const float cy = dot3(center, lightUp_);
            const float sx = std::floor(cx / texelWorld + 0.5f) * texelWorld;
            const float sy = std::floor(cy / texelWorld + 0.5f) * texelWorld;
            center = center + lightRight_ * (sx - cx) + lightUp_ * (sy - cy);

            Cascade& c = cascades_[i];
            c.center = center;
            c.radius = radius;
            c.eyeDistance = radius + consts::SHADOW_CASTER_RANGE;
            c.nearPlane = 0.5f;
            c.farPlane = c.eyeDistance + radius + 1.0f;
            c.splitFar = farDist;
            c.texelWorld = texelWorld;

            nearDist = farDist;
        }
    }

    void beginDepthPass(int cascade) {
        Cascade& c = cascades_[cascade];
        const Vec3 eye = c.center + sunDir_ * c.eyeDistance;

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_[cascade]);
        glViewport(0, 0, mapSize_[cascade], mapSize_[cascade]);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(consts::SHADOW_POLY_OFFSET_FACTOR, consts::SHADOW_POLY_OFFSET_UNITS);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-c.radius, c.radius, -c.radius, c.radius, c.nearPlane, c.farPlane);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(eye.x, eye.y, eye.z,
                  c.center.x, c.center.y, c.center.z,
                  lightUpRef_.x, lightUpRef_.y, lightUpRef_.z);

        GLfloat lightProjection[16];
        GLfloat lightView[16];
        glGetFloatv(GL_PROJECTION_MATRIX, lightProjection);
        glGetFloatv(GL_MODELVIEW_MATRIX, lightView);
        multiply(lightProjection, lightView, c.viewProjection);
    }

    void endDepthPass() {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);
        glUseProgram(0);
    }

    void beginScene(const Vec3& sunDirection) {
        GLfloat cameraView[16];
        GLfloat inverseCameraView[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, cameraView);
        invertRigidMatrix(cameraView, inverseCameraView);

        static const GLfloat biasMatrix[16] = {
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f
        };

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

        GLfloat split[4], texel[4], biasMin[4], biasSlope[4];
        for (int i = 0; i < kCascades; ++i) {
            const Cascade& c = cascades_[i];

            GLfloat lightFromEye[16];
            GLfloat biasedLightFromEye[16];
            multiply(c.viewProjection, inverseCameraView, lightFromEye);
            multiply(biasMatrix, lightFromEye, biasedLightFromEye);
            glUniformMatrix4fv(lightMatrixLoc_[i], 1, GL_FALSE, biasedLightFromEye);

            const float depthRange = c.farPlane - c.nearPlane;
            split[i] = c.splitFar;
            texel[i] = 1.0f / static_cast<float>(mapSize_[i]);
            biasMin[i] = consts::SHADOW_BIAS_MIN_TEXELS * c.texelWorld / depthRange;
            biasSlope[i] = consts::SHADOW_BIAS_SLOPE_TEXELS * c.texelWorld / depthRange;

            glActiveTexture(GL_TEXTURE2 + i);
            glBindTexture(GL_TEXTURE_2D, depthTexture_[i]);
            glUniform1i(shadowMapLoc_[i], 2 + i);
        }
        glActiveTexture(GL_TEXTURE0);

        glUniform4f(splitFarLoc_, split[0], split[1], split[2], split[3]);
        glUniform4f(texelLoc_, texel[0], texel[1], texel[2], texel[3]);
        glUniform4f(biasMinLoc_, biasMin[0], biasMin[1], biasMin[2], biasMin[3]);
        glUniform4f(biasSlopeLoc_, biasSlope[0], biasSlope[1], biasSlope[2], biasSlope[3]);
        glUniform1f(fadeStartLoc_, split[kCascades - 1] * (1.0f - consts::SHADOW_FADE_FRACTION));

        glUniform3f(sunDirLoc_, lx, ly, lz);
        glUniform3f(ambientLoc_, consts::AMBIENT_LIGHT.x, consts::AMBIENT_LIGHT.y, consts::AMBIENT_LIGHT.z);
        glUniform3f(sunColorLoc_, consts::SUN_LIGHT_COLOR.x, consts::SUN_LIGHT_COLOR.y, consts::SUN_LIGHT_COLOR.z);

        glUniform1i(albedoLoc_, Materials::kUnitAlbedo);
        glUniform1i(normalMapLoc_, Materials::kUnitNormal);
        glUniform1i(metalMapLoc_, Materials::kUnitMetal);

        glUniform3f(upEyeLoc_, cameraView[4], cameraView[5], cameraView[6]);
        glUniform3f(skyZenithLoc_, consts::SKY_ZENITH.x, consts::SKY_ZENITH.y, consts::SKY_ZENITH.z);
        glUniform3f(skyHorizonLoc_, consts::SKY_HORIZON.x, consts::SKY_HORIZON.y, consts::SKY_HORIZON.z);
        glUniform3f(skyNadirLoc_, consts::SKY_NADIR.x, consts::SKY_NADIR.y, consts::SKY_NADIR.z);
        glUniform1f(skyCurveLoc_, consts::SKY_GRADIENT_CURVE);

        Materials::Attach(tintLoc_, tilingLoc_, offsetLoc_, metallicLoc_, roughnessLoc_, normalStrengthLoc_, normalYSignLoc_);
    }

    void endScene() {
        Materials::Detach();
        glUseProgram(0);
        for (int i = 0; i < kCascades; ++i) {
            glActiveTexture(GL_TEXTURE2 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void cleanup() {
        if (sceneProgram_) {
            glDeleteProgram(sceneProgram_);
            sceneProgram_ = 0;
        }
        for (int i = 0; i < kCascades; ++i) {
            if (depthTexture_[i]) {
                glDeleteTextures(1, &depthTexture_[i]);
                depthTexture_[i] = 0;
            }
            if (framebuffer_[i]) {
                glDeleteFramebuffers(1, &framebuffer_[i]);
                framebuffer_[i] = 0;
            }
        }
    }

private:
    struct Cascade {
        Vec3 center;
        float radius = 1.0f;
        float eyeDistance = 1.0f;
        float nearPlane = 0.5f;
        float farPlane = 2.0f;
        float splitFar = 1.0f;
        float texelWorld = 0.01f;
        GLfloat viewProjection[16] = {};
    };

    Cascade cascades_[kCascades];
    GLuint framebuffer_[kCascades] = {};
    GLuint depthTexture_[kCascades] = {};
    int mapSize_[kCascades] = {};

    Vec3 sunDir_ = Vec3(0.0f, 1.0f, 0.0f);
    Vec3 lightUpRef_ = Vec3(0.0f, 1.0f, 0.0f);
    Vec3 lightRight_ = Vec3(1.0f, 0.0f, 0.0f);
    Vec3 lightUp_ = Vec3(0.0f, 0.0f, 1.0f);

    // что тут нахуй происходит чатгпт ебаный
    GLuint sceneProgram_ = 0;
    GLint lightMatrixLoc_[kCascades] = {-1, -1, -1, -1};
    GLint shadowMapLoc_[kCascades] = {-1, -1, -1, -1};
    GLint splitFarLoc_ = -1, texelLoc_ = -1, biasMinLoc_ = -1, biasSlopeLoc_ = -1, fadeStartLoc_ = -1;
    GLint sunDirLoc_ = -1, ambientLoc_ = -1, sunColorLoc_ = -1;
    GLint albedoLoc_ = -1, tintLoc_ = -1, tilingLoc_ = -1, offsetLoc_ = -1;
    GLint normalMapLoc_ = -1, metalMapLoc_ = -1, normalStrengthLoc_ = -1, normalYSignLoc_ = -1;
    GLint metallicLoc_ = -1, roughnessLoc_ = -1, upEyeLoc_ = -1;
    GLint skyZenithLoc_ = -1, skyHorizonLoc_ = -1, skyNadirLoc_ = -1, skyCurveLoc_ = -1;

    static float dot3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static Vec3 cross3(const Vec3& a, const Vec3& b) { return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

    bool createCascadeTarget(int i) {
        glGenFramebuffers(1, &framebuffer_[i]);
        glGenTextures(1, &depthTexture_[i]);

        glBindTexture(GL_TEXTURE_2D, depthTexture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mapSize_[i], mapSize_[i], 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture_[i], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SDL_Log("shadowbuffer %d (%dx%d) incomplete: 0x%X", i, mapSize_[i], mapSize_[i], status);
            return false;
        }
        return true;
    }

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
