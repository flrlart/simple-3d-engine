#pragma once
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/glu.h>
#include <btBulletDynamicsCommon.h>
#include "vec3.h"
#include "consts.h"
#include "world.h"

class PlayerController {
public:
    btRigidBody* body = nullptr;
    btCollisionShape* shape = nullptr;

    float yaw = 0.0f;
    float pitch = consts::INITIAL_PITCH;

    int heldBoxIndex = -1;
    int hoveredBoxIndex = -1;
    float holdDistance = consts::HOLD_DIST_DEFAULT;

    void init(btDiscreteDynamicsWorld* dynamicsWorld) {
        using namespace consts;
        shape = new btCapsuleShape(PLAYER_RADIUS, PLAYER_CYLINDER_HEIGHT);
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(PLAYER_SPAWN_POS.x, PLAYER_SPAWN_POS.y, PLAYER_SPAWN_POS.z));

        auto* motion = new btDefaultMotionState(trans);
        btVector3 inertia(0, 0, 0);
        shape->calculateLocalInertia(PLAYER_MASS, inertia);

        btRigidBody::btRigidBodyConstructionInfo ci(PLAYER_MASS, motion, shape, inertia);
        ci.m_friction = 0.0f;
        ci.m_restitution = 0.0f;

        body = new btRigidBody(ci);
        body->setAngularFactor(btVector3(0, 0, 0));
        body->setActivationState(DISABLE_DEACTIVATION);
        dynamicsWorld->addRigidBody(body);
    }

    Vec3 getEyePos() const {
        btTransform trans;
        body->getMotionState()->getWorldTransform(trans);
        const btVector3 p = trans.getOrigin();
        return {p.x(), p.y() + consts::PLAYER_EYE_OFFSET, p.z()};
    }

    Vec3 getForward() const {
        const float ry = yaw * DEG_TO_RAD;
        const float rp = pitch * DEG_TO_RAD;
        return {
            std::sin(ry) * std::cos(rp),
            -std::sin(rp),
            -std::cos(ry) * std::cos(rp)
        };
    }

    Vec3 getForwardFlat() const {
        const float ry = yaw * DEG_TO_RAD;
        return {std::sin(ry), 0.0f, -std::cos(ry)};
    }

    Vec3 getRight() const {
        const float ry = yaw * DEG_TO_RAD;
        return {std::cos(ry), 0.0f, std::sin(ry)};
    }

    void teleportToSpawn(const GameWorld& world) {
        using namespace consts;
        const float centerOffset = PLAYER_CYLINDER_HEIGHT * 0.5f + PLAYER_RADIUS;
        btTransform t;
        t.setIdentity();
        t.setOrigin(btVector3(world.playerSpawnPos.x, world.playerSpawnPos.y + centerOffset, world.playerSpawnPos.z));
        body->setWorldTransform(t);
        body->getMotionState()->setWorldTransform(t);
        body->setLinearVelocity(btVector3(0, 0, 0));
        body->clearForces();
        yaw = world.playerSpawnYaw;
        pitch = INITIAL_PITCH;
    }

    void releaseHeldBox(GameWorld& world) {
        if (heldBoxIndex == -1) return;
        btRigidBody* box = world.boxes[heldBoxIndex].body;
        box->setIgnoreCollisionCheck(body, false);
        body->setIgnoreCollisionCheck(box, false);
        heldBoxIndex = -1;
    }

    bool isGrounded(const GameWorld& world) const {
        using namespace consts;
        btTransform trans;
        body->getMotionState()->getWorldTransform(trans);
        const btVector3 from = trans.getOrigin();
        const btVector3 to = from - btVector3(0.0f, PLAYER_CYLINDER_HEIGHT * 0.5f + PLAYER_RADIUS + GROUND_CHECK_MARGIN, 0.0f);

        RayIgnoringCallback ray(from, to, body, heldBody(world));
        world.dynamicsWorld->rayTest(from, to, ray);
        return ray.hasHit();
    }

    void handleEvent(const SDL_Event& event, GameWorld& world, bool& running) {
        using namespace consts;

        if (event.type == SDL_MOUSEMOTION) {
            yaw += event.motion.xrel * MOUSE_SENSITIVITY;
            pitch += event.motion.yrel * MOUSE_SENSITIVITY;
            pitch = std::clamp(pitch, -PITCH_LIMIT, PITCH_LIMIT);
        }

        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_e:
                if (heldBoxIndex != -1) releaseHeldBox(world);
                else if (hoveredBoxIndex != -1) grabBox(world, hoveredBoxIndex);
                break;
            case SDLK_r:
                releaseHeldBox(world);
                world.resetAllBoxes();
                break;
            case SDLK_SPACE:
                if (isGrounded(world)) {
                    const btVector3 v = body->getLinearVelocity();
                    body->setLinearVelocity(btVector3(v.x(), JUMP_SPEED, v.z()));
                }
                break;
            default:
                break;
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && heldBoxIndex != -1) {
            throwHeldBox(world);
        }

        if (event.type == SDL_MOUSEWHEEL && heldBoxIndex != -1) {
            holdDistance = std::clamp(holdDistance + event.wheel.y * HOLD_DIST_WHEEL_STEP, HOLD_DIST_MIN, HOLD_DIST_MAX);
        }
    }

    void update(GameWorld& world) {
        applyMovement(world);
        respawnIfFallen(world);
        updateHeldBox(world);
        updateHover(world);
    }

    void setupCamera(int winWidth, int winHeight) const {
        using namespace consts;
        glViewport(0, 0, winWidth, winHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(CAMERA_FOV_DEG, static_cast<double>(winWidth) / winHeight, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

        const Vec3 eyePos = getEyePos();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-eyePos.x, -eyePos.y, -eyePos.z);
    }

    void drawCrosshair(int width, int height) const {
        using namespace consts;
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        const int cx = width / 2;
        const int cy = height / 2;
        const int h = CROSSHAIR_HALF_LENGTH;

        const Vec3& c = (heldBoxIndex != -1)    ? CROSSHAIR_COLOR_HOLDING
                      : (hoveredBoxIndex != -1) ? CROSSHAIR_COLOR_HOVER
                                                : CROSSHAIR_COLOR_IDLE;
        glColor3f(c.x, c.y, c.z);

        glLineWidth(CROSSHAIR_LINE_WIDTH);
        glBegin(GL_LINES);
        glVertex2i(cx - h, cy); glVertex2i(cx + h, cy);
        glVertex2i(cx, cy - h); glVertex2i(cx, cy + h);
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glEnable(GL_DEPTH_TEST);
    }

    void cleanup(btDiscreteDynamicsWorld* dynamicsWorld) {
        dynamicsWorld->removeRigidBody(body);
        delete body->getMotionState();
        delete body;
        delete shape;
    }

private:
    struct RayIgnoringCallback : btCollisionWorld::ClosestRayResultCallback {
        const btCollisionObject* ignoreA;
        const btCollisionObject* ignoreB;

        RayIgnoringCallback(const btVector3& from, const btVector3& to,
                            const btCollisionObject* a, const btCollisionObject* b = nullptr)
            : ClosestRayResultCallback(from, to), ignoreA(a), ignoreB(b) {}

        bool needsCollision(btBroadphaseProxy* proxy) const override {
            const void* obj = proxy->m_clientObject;
            if (obj == ignoreA || (ignoreB && obj == ignoreB)) return false;
            return ClosestRayResultCallback::needsCollision(proxy);
        }
    };

    const btCollisionObject* heldBody(const GameWorld& world) const {
        return heldBoxIndex != -1 ? world.boxes[heldBoxIndex].body : nullptr;
    }

    void grabBox(GameWorld& world, int index) {
        heldBoxIndex = index;
        btRigidBody* box = world.boxes[index].body;
        box->activate(true);
        box->setIgnoreCollisionCheck(body, true);
        body->setIgnoreCollisionCheck(box, true);
    }

    void throwHeldBox(GameWorld& world) {
        using namespace consts;
        Box& b = world.boxes[heldBoxIndex];
        const Vec3 dir = getForward();
        releaseHeldBox(world);

        const float effectiveMass = std::max(b.mass, THROW_MIN_MASS);
        const float massScale = std::clamp(std::pow(THROW_REF_MASS / effectiveMass, THROW_MASS_EXPONENT),
                                           THROW_SCALE_MIN, THROW_SCALE_MAX);

        b.body->activate(true);
        const btVector3 currentVel = b.body->getLinearVelocity();
        b.body->setLinearVelocity(currentVel * THROW_KEEP_VELOCITY +
                                  btVector3(dir.x, dir.y, dir.z) * (THROW_SPEED * massScale));
    }

    void applyMovement(const GameWorld& world) {
        using namespace consts;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        Vec3 moveDir = {0, 0, 0};
        const Vec3 fwd = getForwardFlat();
        const Vec3 right = getRight();
        if (keys[SDL_SCANCODE_W]) moveDir += fwd;
        if (keys[SDL_SCANCODE_S]) moveDir -= fwd;
        if (keys[SDL_SCANCODE_D]) moveDir += right;
        if (keys[SDL_SCANCODE_A]) moveDir -= right;
        moveDir = moveDir.normalized();

        const bool grounded = isGrounded(world);
        const float maxSpeed = keys[SDL_SCANCODE_LSHIFT] ? RUN_SPEED : WALK_SPEED;
        const float gain = grounded ? GROUND_ACCEL_GAIN : AIR_ACCEL_GAIN;
        const float maxAccel = grounded ? GROUND_MAX_ACCEL : AIR_MAX_ACCEL;

        const btVector3 curVel = body->getLinearVelocity();
        const Vec3 desiredVel = moveDir * maxSpeed;

        btVector3 accel(desiredVel.x - curVel.x(), 0.0f, desiredVel.z - curVel.z());
        accel *= gain;
        if (accel.length() > maxAccel) accel = accel.normalized() * maxAccel;

        body->applyCentralForce(accel * PLAYER_MASS);
    }

    void respawnIfFallen(GameWorld& world) {
        btTransform t;
        body->getMotionState()->getWorldTransform(t);
        if (t.getOrigin().y() >= world.respawnY) return;

        releaseHeldBox(world);
        teleportToSpawn(world);
    }

    void updateHeldBox(GameWorld& world) {
        using namespace consts;
        if (heldBoxIndex == -1) return;

        Box& b = world.boxes[heldBoxIndex];
        b.body->activate(true);

        btTransform boxTrans;
        b.body->getMotionState()->getWorldTransform(boxTrans);
        const btVector3 bPos = boxTrans.getOrigin();

        const Vec3 target = getEyePos() + getForward() * holdDistance;
        const btVector3 toTarget(target.x - bPos.x(), target.y - bPos.y(), target.z - bPos.z());

        const float massFactor = std::clamp(std::pow(HOLD_REF_MASS / std::max(b.mass, HOLD_MIN_MASS), HOLD_MASS_EXPONENT),
                                            HOLD_FACTOR_MIN, HOLD_FACTOR_MAX);
        b.body->setLinearVelocity(toTarget * (HOLD_GAIN * massFactor));
        b.body->setAngularVelocity(b.body->getAngularVelocity() * HOLD_ANGULAR_DAMPING);

        if (toTarget.length() > HOLD_MAX_LAG) releaseHeldBox(world);
    }

    void updateHover(const GameWorld& world) {
        hoveredBoxIndex = -1;
        if (heldBoxIndex != -1) return;

        const Vec3 eye = getEyePos();
        const Vec3 look = getForward();
        const btVector3 from(eye.x, eye.y, eye.z);
        const btVector3 to = from + btVector3(look.x, look.y, look.z) * consts::MAX_PICKUP_DIST;

        RayIgnoringCallback ray(from, to, body);
        world.dynamicsWorld->rayTest(from, to, ray);

        if (ray.hasHit()) {
            const int idx = ray.m_collisionObject->getUserIndex();
            if (idx >= 0 && idx < static_cast<int>(world.boxes.size())) hoveredBoxIndex = idx;
        }
    }
};
