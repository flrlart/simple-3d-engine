#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <GL/glu.h>

#include "math.h"
#include "lighting.h"
#include "world.h"
#include "controller.h"
#include "shadows.h"
#include "font.h"
#include "hud.h"

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int winWidth = 1280, winHeight = 720;
    SDL_Window* window = SDL_CreateWindow(
        "boxtest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winWidth, winHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Log("create window failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        SDL_Log("context failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        SDL_Log("glew init failed: %s", glewGetErrorString(glewStatus));
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    while (glGetError() != GL_NO_ERROR) {}

    SDL_GL_SetSwapInterval(1);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    glEnable(GL_DEPTH_TEST);
    
    TextHUD hud;
    if (!hud.init(hud_font_data, hud_font_data_len, 20)) {
        SDL_Log("hud init failed");
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    LightingSystem lighting;
    lighting.init();

    ShadowMapping shadows;
    if (!shadows.init(4096)) {
        SDL_Log("dynamic shadow init failed");
        shadows.cleanup();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GameWorld world;
    world.init();

    PlayerController player;
    player.init(world.dynamicsWorld);

    Uint64 prevCounter = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - prevCounter) / SDL_GetPerformanceFrequency();
        prevCounter = now;
        if (dt > 0.1f) dt = 0.1f;

        // events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                winWidth = event.window.data1;
                winHeight = event.window.data2;
            }
            player.handleEvent(event, world, running);
        }

        // update physics
        player.update(world);
        world.stepPhysics(dt, player.heldBoxIndex, player.body);

        // shadows
        shadows.beginDepthPass(lighting.sunDir);
        world.renderDepth();
        shadows.endDepthPass();

        // camera pass
        player.setupCamera(winWidth, winHeight);
        lighting.drawSkyboxAndSun(player.getEyePos());
        lighting.applyLights();

        shadows.beginScene(lighting.sunDir);
        world.render(lighting);
        shadows.endScene();
        
        hud.drawText(
            16.0f, 14.0f,
            "wasd - move\nspace - jump\nleft shift - run\ne - take cube\ne+mouse left - drop cube\nmouse scroll - change hold distance",
            winWidth, winHeight,
            1.0f, 1.0f, 1.0f,
            1.0f
        );

        player.drawCrosshair(winWidth, winHeight);
        SDL_GL_SwapWindow(window);
        
        static Uint64 lastFrame = SDL_GetPerformanceCounter();

        Uint64 now_ = SDL_GetPerformanceCounter();
        double elapsed = static_cast<double>(now_ - lastFrame) / SDL_GetPerformanceFrequency();
        lastFrame = now_;

        if (elapsed < 1.0 / 240.0) {
            SDL_Delay(1);
        }
    }

    hud.cleanup();
    player.cleanup(world.dynamicsWorld);
    world.cleanup();
    shadows.cleanup();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
