#pragma once
#include <array>
#include "vec3.h"

namespace consts {

// window
inline constexpr const char* WINDOW_TITLE   = "boxtest";
inline constexpr int WINDOW_WIDTH           = 1280;
inline constexpr int WINDOW_HEIGHT          = 720;
inline constexpr int GL_CONTEXT_MAJOR       = 2;
inline constexpr int GL_CONTEXT_MINOR       = 1;
inline constexpr int DEPTH_BUFFER_BITS      = 24;
inline constexpr bool VSYNC_ENABLED         = true;
inline constexpr int  FPS_LIMIT             = 240;

// physics
inline constexpr float GRAVITY_Y            = -18.0f;
inline constexpr float PHYSICS_STEP         = 1.0f / 120.0f;
inline constexpr float MAX_FRAME_DT         = 0.1f;
inline constexpr int   PHYSICS_MAX_SUBSTEPS = static_cast<int>(MAX_FRAME_DT / PHYSICS_STEP) + 1;
inline constexpr float DEFAULT_RESPAWN_Y    = -30.0f;

// level
inline constexpr const char* DEFAULT_LEVEL  = "levels/level1.txt";
inline constexpr float STATIC_FRICTION      = 0.90f;
inline constexpr float STATIC_RESTITUTION   = 0.05f;
inline const Vec3 DEFAULT_OBJECT_COLOR      = {0.70f, 0.72f, 0.76f};

// box physics
inline constexpr float BOX_DENSITY              = 22.0f;   // mass = V * p
inline constexpr float BOX_FRICTION             = 0.88f;
inline constexpr float BOX_RESTITUTION          = 0.08f;
inline constexpr float BOX_ROLLING_FRICTION     = 0.14f;
inline constexpr float BOX_SPINNING_FRICTION    = 0.14f;
inline constexpr float BOX_LINEAR_DAMPING       = 0.25f;
inline constexpr float BOX_ANGULAR_DAMPING      = 0.38f;

// player
inline constexpr float PLAYER_RADIUS            = 0.38f;
inline constexpr float PLAYER_CYLINDER_HEIGHT   = 1.0f;
inline constexpr float PLAYER_MASS              = 75.0f;
inline constexpr float PLAYER_EYE_OFFSET        = 0.72f;
inline const Vec3 PLAYER_SPAWN_POS              = {0.0f, 2.0f, 6.5f};
inline constexpr float MOUSE_SENSITIVITY        = 0.15f;
inline constexpr float PITCH_LIMIT              = 89.0f;
inline constexpr float INITIAL_PITCH            = 10.0f;
inline constexpr double CAMERA_FOV_DEG          = 75.0;
inline constexpr double CAMERA_NEAR_PLANE       = 0.1;
inline constexpr double CAMERA_FAR_PLANE        = 150.0;
inline constexpr float WALK_SPEED               = 5.0f;
inline constexpr float RUN_SPEED                = 7.8f;
inline constexpr float JUMP_SPEED               = 6.8f;
inline constexpr float GROUND_CHECK_MARGIN      = 0.15f;
inline constexpr float GROUND_ACCEL_GAIN        = 25.0f;
inline constexpr float GROUND_MAX_ACCEL         = 190.0f;
inline constexpr float AIR_ACCEL_GAIN           = 8.0f;
inline constexpr float AIR_MAX_ACCEL            = 40.0f;
inline constexpr float MAX_PICKUP_DIST          = 4.2f;
inline constexpr float HOLD_DIST_DEFAULT        = 2.4f;
inline constexpr float HOLD_DIST_MIN            = 1.6f;
inline constexpr float HOLD_DIST_MAX            = 4.0f;
inline constexpr float HOLD_DIST_WHEEL_STEP     = 0.3f;
inline constexpr float HOLD_MAX_LAG             = 4.5f;
inline constexpr float HOLD_GAIN                = 14.0f;
inline constexpr float HOLD_REF_MASS            = 20.0f;
inline constexpr float HOLD_MIN_MASS            = 5.0f;
inline constexpr float HOLD_MASS_EXPONENT       = 0.30f;
inline constexpr float HOLD_FACTOR_MIN          = 0.40f;
inline constexpr float HOLD_FACTOR_MAX          = 1.0f;
inline constexpr float HOLD_ANGULAR_DAMPING     = 0.82f;
inline constexpr float THROW_SPEED              = 12.0f;
inline constexpr float THROW_REF_MASS           = 22.0f;
inline constexpr float THROW_MIN_MASS           = 6.0f;
inline constexpr float THROW_MASS_EXPONENT      = 0.32f;
inline constexpr float THROW_SCALE_MIN          = 0.45f;
inline constexpr float THROW_SCALE_MAX          = 1.25f;
inline constexpr float THROW_KEEP_VELOCITY      = 0.25f;

// sky & sun
inline const Vec3 SUN_DIR                   = Vec3(0.55f, 0.78f, 0.30f).normalized();
inline const Vec3 SKY_ZENITH                = {0.28f, 0.42f, 0.78f};
inline const Vec3 SKY_HORIZON               = {0.56f, 0.78f, 0.90f};
inline const Vec3 SKY_NADIR                 = {0.26f, 0.44f, 0.64f};
inline constexpr float SKY_RADIUS           = 60.0f;
inline constexpr int   SKY_SLICES           = 32;
inline constexpr int   SKY_STACKS           = 16;
inline constexpr float SKY_GRADIENT_CURVE   = 0.65f;
inline constexpr float SUN_DISC_DISTANCE    = 50.0f;
inline constexpr float SUN_DISC_HALF_SIZE   = 2.0f;
inline const Vec3 SUN_DISC_COLOR            = {0.96f, 0.92f, 0.76f};

// lighting & shadows
inline const Vec3 AMBIENT_LIGHT                     = {0.32f, 0.35f, 0.41f};
inline const Vec3 SUN_LIGHT_COLOR                   = {0.78f, 0.75f, 0.92f};
inline constexpr int SHADOW_CASCADE_COUNT           = 4; // do not change
inline constexpr int SHADOW_MAP_SIZE                = 2048;
inline constexpr std::array<float, SHADOW_CASCADE_COUNT> SHADOW_CASCADE_QUALITY = {1.0f, 1.0f, 0.75f, 0.5f};
inline constexpr std::array<float, SHADOW_CASCADE_COUNT> SHADOW_CASCADE_FAR = {6.0f, 16.0f, 30.0f, 60.0f};
inline constexpr float SHADOW_CASTER_RANGE          = 30.0f;
inline constexpr float SHADOW_FADE_FRACTION         = 0.15f;
inline constexpr float SHADOW_POLY_OFFSET_FACTOR    = 2.0f;
inline constexpr float SHADOW_POLY_OFFSET_UNITS     = 4.0f;
inline constexpr float SHADOW_BIAS_MIN_TEXELS       = 1.0f;
inline constexpr float SHADOW_BIAS_SLOPE_TEXELS     = 2.0f;

// hud
inline constexpr unsigned int HUD_FONT_PIXEL_HEIGHT = 20;
inline constexpr float HUD_MARGIN_X                 = 16.0f;
inline constexpr float HUD_MARGIN_Y                 = 14.0f;
inline constexpr float HUD_TEXT_SCALE               = 1.0f;
inline const Vec3 HUD_TEXT_COLOR                    = {1.0f, 1.0f, 1.0f};
inline constexpr const char* HUD_HELP_TEXT =
    "wasd - move\n"
    "space - jump\n"
    "left shift - run\n"
    "e - take / drop cube\n"
    "mouse left - throw held cube\n"
    "mouse scroll - change hold distance\n"
    "r - reset cubes\n"
    "f5 - reload level\n"
    "esc - quit";

inline constexpr int CROSSHAIR_HALF_LENGTH  = 4;
inline constexpr float CROSSHAIR_LINE_WIDTH = 2.0f;
inline const Vec3 CROSSHAIR_COLOR_IDLE      = {0.90f, 0.90f, 0.90f};
inline const Vec3 CROSSHAIR_COLOR_HOVER     = {0.20f, 1.00f, 0.30f};
inline const Vec3 CROSSHAIR_COLOR_HOLDING   = {1.00f, 0.60f, 0.00f};

}
