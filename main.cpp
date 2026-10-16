#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <GL/glu.h>

#include "vec3.h"
#include "consts.h"
#include "sky.h"
#include "world.h"
#include "controller.h"
#include "shadows.h"
#include "font.h"
#include "hud.h"
#include "textures.h"
#include "materials.h"
#include "level.h"

int main(int argc, char* argv[]) {
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    TextHUD hud;
    ShadowMapping shadows;

    auto fail = [&](const char* message) {
        SDL_Log("%s", message);
        hud.cleanup();
        shadows.cleanup();
        Materials::DestroyAll();
        Textures::UnloadAll();
        if (glContext) SDL_GL_DeleteContext(glContext);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    };

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return fail(SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, consts::GL_CONTEXT_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, consts::GL_CONTEXT_MINOR);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, consts::DEPTH_BUFFER_BITS);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int winWidth = consts::WINDOW_WIDTH, winHeight = consts::WINDOW_HEIGHT;
    window = SDL_CreateWindow(
        consts::WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winWidth, winHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_Log("create window failed: %s", SDL_GetError());
        return fail("window creation failed");
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        SDL_Log("context failed: %s", SDL_GetError());
        return fail("opengl context creation failed");
    }

    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        SDL_Log("glew init failed: %s", glewGetErrorString(glewStatus));
        return fail("glew initialization failed");
    }
    while (glGetError() != GL_NO_ERROR) {}

    SDL_GL_SetSwapInterval(consts::VSYNC_ENABLED ? 1 : 0);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    glEnable(GL_DEPTH_TEST);

    if (!hud.init(hud_font_data, hud_font_data_len, consts::HUD_FONT_PIXEL_HEIGHT)) {
        return fail("hud init failed");
    }
    if (!shadows.init(consts::SHADOW_MAP_SIZE)) {
        return fail("dynamic shadow init failed");
    }

    Sky sky;
    GameWorld world;
    world.init();

    PlayerController player;
    player.init(world.dynamicsWorld);

    std::string currentLevel = (argc > 1) ? argv[1] : consts::DEFAULT_LEVEL; // либо аргументом, либо из consts
    auto loadLevel = [&](const std::string& path) {
        player.releaseHeldBox(world);
        if (!Level::Load(world, path.c_str())) return false;
        currentLevel = path;
        player.teleportToSpawn(world);
        return true;
    };
    if (!loadLevel(currentLevel)) return fail("level load failed");

    const Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 prevCounter = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        const Uint64 frameStart = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(frameStart - prevCounter) / static_cast<float>(perfFreq);
        prevCounter = frameStart;
        dt = std::min(dt, consts::MAX_FRAME_DT);

        // события
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                winWidth = event.window.data1;
                winHeight = event.window.data2;
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_F5) {
                loadLevel(currentLevel);
                continue;
            }
            player.handleEvent(event, world, running);
        }

        // физика
        player.update(world);
        world.stepPhysics(dt);
        world.respawnFallenBoxes([&](int boxIndex) {
            if (boxIndex == player.heldBoxIndex) player.releaseHeldBox(world);
        });

        // каскадные карты теней (считаются от камеры)
        ShadowCamera shadowCam;
        shadowCam.pos = player.getEyePos();
        shadowCam.forward = player.getForward();
        shadowCam.right = player.getRight();
        shadowCam.fovY = static_cast<float>(consts::CAMERA_FOV_DEG) * 3.14159265f / 180.0f;
        shadowCam.aspect = static_cast<float>(winWidth) / static_cast<float>(std::max(winHeight, 1));
        shadows.update(shadowCam, consts::SUN_DIR);
        for (int cascade = 0; cascade < ShadowMapping::kCascades; ++cascade) {
            shadows.beginDepthPass(cascade);
            world.render();
            shadows.endDepthPass();
        }

        // карта
        player.setupCamera(winWidth, winHeight);
        sky.draw(player.getEyePos());
        shadows.beginScene(consts::SUN_DIR);
        world.render();
        shadows.endScene();

        // hud
        hud.drawText(consts::HUD_MARGIN_X, consts::HUD_MARGIN_Y, consts::HUD_HELP_TEXT,
                     winWidth, winHeight,
                     consts::HUD_TEXT_COLOR.x, consts::HUD_TEXT_COLOR.y, consts::HUD_TEXT_COLOR.z,
                     consts::HUD_TEXT_SCALE);
        player.drawCrosshair(winWidth, winHeight);

        SDL_GL_SwapWindow(window);

        if (!consts::VSYNC_ENABLED && consts::FPS_LIMIT > 0) {
            const double frameTime = static_cast<double>(SDL_GetPerformanceCounter() - frameStart) / static_cast<double>(perfFreq);
            const double targetTime = 1.0 / consts::FPS_LIMIT;
            if (frameTime < targetTime) {
                SDL_Delay(static_cast<Uint32>((targetTime - frameTime) * 1000.0));
            }
        }
    }

    hud.cleanup();
    player.cleanup(world.dynamicsWorld);
    world.cleanup();
    shadows.cleanup();
    Materials::DestroyAll();
    Textures::UnloadAll();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
