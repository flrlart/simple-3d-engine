#pragma once
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <btBulletDynamicsCommon.h>
#include "Math.h"
#include "World.h"

class PlayerController {
public:
    btRigidBody* body = nullptr;
    btCollisionShape* shape = nullptr;

    float yaw = 0.0f;
    float pitch = 10.0f;
    const float radius = 0.38f;
    const float cylinderHeight = 1.0f;
    const float playerMass = 75.0f;

    int heldBoxIndex = -1;
    int hoveredBoxIndex = -1;
    float holdDistance = 2.4f;
    const float maxPickupDist = 4.2f;

    void init(btDiscreteDynamicsWorld* dynamicsWorld) {
        shape = new btCapsuleShape(radius, cylinderHeight);
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(0.0f, 2.0f, 6.5f));

        auto* motion = new btDefaultMotionState(trans);
        btVector3 inertia(0, 0, 0);
        shape->calculateLocalInertia(playerMass, inertia);

        btRigidBody::btRigidBodyConstructionInfo ci(playerMass, motion, shape, inertia);
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
        btVector3 p = trans.getOrigin();
        return {p.x(), p.y() + 0.72f, p.z()};
    }

    Vec3 getForward() const {
        float ry = yaw * 3.14159265f / 180.0f;
        float rp = pitch * 3.14159265f / 180.0f;
        return {
            std::sin(ry) * std::cos(rp),
            -std::sin(rp),
            -std::cos(ry) * std::cos(rp)
        };
    }

    Vec3 getForwardFlat() const {
        float ry = yaw * 3.14159265f / 180.0f;
        return {std::sin(ry), 0.0f, -std::cos(ry)};
    }

    Vec3 getRight() const {
        float ry = yaw * 3.14159265f / 180.0f;
        return {std::cos(ry), 0.0f, std::sin(ry)};
    }

    void releaseHeldBox(GameWorld& world) {
        if (heldBoxIndex != -1) {
            world.boxes[heldBoxIndex].body->setIgnoreCollisionCheck(body, false);
            body->setIgnoreCollisionCheck(world.boxes[heldBoxIndex].body, false);
            heldBoxIndex = -1;
        }
    }

    bool isGrounded(btDiscreteDynamicsWorld* dynamicsWorld) const {
        btTransform trans;
        body->getMotionState()->getWorldTransform(trans);
        btVector3 orig = trans.getOrigin();
        btVector3 rayTo = orig - btVector3(0.0f, cylinderHeight * 0.5f + radius + 0.15f, 0.0f);
        btCollisionWorld::ClosestRayResultCallback rayCallback(orig, rayTo);
        dynamicsWorld->rayTest(orig, rayTo, rayCallback);
        return rayCallback.hasHit();
    }

    void handleEvent(const SDL_Event& event, GameWorld& world, bool& running) {
        if (event.type == SDL_MOUSEMOTION) {
            const float sensitivity = 0.15f;
            yaw += event.motion.xrel * sensitivity;
            pitch += event.motion.yrel * sensitivity;
            pitch = std::clamp(pitch, -89.0f, 89.0f);
        }

        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            if (event.key.keysym.sym == SDLK_ESCAPE) running = false;

            if (event.key.keysym.sym == SDLK_e) {
                if (heldBoxIndex != -1) {
                    releaseHeldBox(world);
                } else if (hoveredBoxIndex != -1) {
                    heldBoxIndex = hoveredBoxIndex;
                    world.boxes[heldBoxIndex].body->activate(true);
                    world.boxes[heldBoxIndex].body->setIgnoreCollisionCheck(body, true);
                    body->setIgnoreCollisionCheck(world.boxes[heldBoxIndex].body, true);
                }
            }

            if (event.key.keysym.sym == SDLK_r) {
                releaseHeldBox(world);
                world.resetAllBoxes();
            }

            if (event.key.keysym.sym == SDLK_SPACE && isGrounded(world.dynamicsWorld)) {
                btVector3 v = body->getLinearVelocity();
                body->setLinearVelocity(btVector3(v.x(), 6.8f, v.z()));
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            if (heldBoxIndex != -1) {
                Box& b = world.boxes[heldBoxIndex];
                Vec3 dir = getForward();
                releaseHeldBox(world);

                const float refMass = 22.0f;
                float effectiveMass = std::max(b.mass, 6.0f);
                float massScale = std::pow(refMass / effectiveMass, 0.32f);
                massScale = std::clamp(massScale, 0.45f, 1.25f);

                float throwSpeed = 12.0f * massScale;

                b.body->activate(true);
                btVector3 currentVel = b.body->getLinearVelocity();
                b.body->setLinearVelocity(currentVel * 0.25f + btVector3(dir.x, dir.y, dir.z) * throwSpeed);
            }
        }

        if (event.type == SDL_MOUSEWHEEL && heldBoxIndex != -1) {
            holdDistance = std::clamp(holdDistance + event.wheel.y * 0.3f, 1.6f, 4.0f);
        }
    }

    void update(GameWorld& world) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Vec3 moveDir = {0, 0, 0};
        Vec3 fwd = getForwardFlat();
        Vec3 right = getRight();

        if (keys[SDL_SCANCODE_W]) moveDir += fwd;
        if (keys[SDL_SCANCODE_S]) moveDir -= fwd;
        if (keys[SDL_SCANCODE_D]) moveDir += right;
        if (keys[SDL_SCANCODE_A]) moveDir -= right;

        if (moveDir.length() > 0.0f) moveDir = moveDir.normalized();

        bool grounded = isGrounded(world.dynamicsWorld);
        float maxSpeed = keys[SDL_SCANCODE_LSHIFT] ? 7.8f : 5.0f;

        btVector3 curVel = body->getLinearVelocity();
        Vec3 desiredVel = moveDir * maxSpeed;
        btVector3 velDiff(desiredVel.x - curVel.x(), 0.0f, desiredVel.z - curVel.z());

        float accelGain = grounded ? 25.0f : 8.0f;
        float maxPushForce = keys[SDL_SCANCODE_LSHIFT] ? 1800.0f : 1400.0f;

        btVector3 moveForce = velDiff * (playerMass * accelGain);
        if (moveForce.length() > maxPushForce) {
            moveForce = moveForce.normalized() * maxPushForce * 10; // TODO: убрать " * 10", заменить на переменную acceleration
        }
        body->applyCentralForce(moveForce);

        btTransform pTrans;
        body->getMotionState()->getWorldTransform(pTrans);
        if (pTrans.getOrigin().y() < -25.0f) {
            releaseHeldBox(world);
            btTransform resetT;
            resetT.setIdentity();
            resetT.setOrigin(btVector3(0.0f, 3.0f, 6.0f));
            body->setWorldTransform(resetT);
            body->getMotionState()->setWorldTransform(resetT);
            body->setLinearVelocity(btVector3(0, 0, 0));
        }

        if (heldBoxIndex != -1) {
            Box& b = world.boxes[heldBoxIndex];
            b.body->activate(true);

            btTransform boxTrans;
            b.body->getMotionState()->getWorldTransform(boxTrans);
            btVector3 bPos = boxTrans.getOrigin();

            Vec3 eye = getEyePos();
            Vec3 target = eye + getForward() * holdDistance;
            btVector3 toTarget(target.x - bPos.x(), target.y - bPos.y(), target.z - bPos.z());

            float massFactor = std::clamp(std::pow(20.0f / std::max(b.mass, 5.0f), 0.30f), 0.40f, 1.0f);
            b.body->setLinearVelocity(toTarget * (14.0f * massFactor));
            b.body->setAngularVelocity(b.body->getAngularVelocity() * 0.82f);

            if (toTarget.length() > 4.5f) {
                releaseHeldBox(world);
            }
        }

        hoveredBoxIndex = -1;
        if (heldBoxIndex == -1) {
            Vec3 eye = getEyePos();
            Vec3 look = getForward();
            btVector3 rayFrom(eye.x, eye.y, eye.z);
            btVector3 rayTo = rayFrom + btVector3(look.x, look.y, look.z) * maxPickupDist;

            btCollisionWorld::ClosestRayResultCallback rayCallback(rayFrom, rayTo);
            world.dynamicsWorld->rayTest(rayFrom, rayTo, rayCallback);

            if (rayCallback.hasHit()) {
                int idx = rayCallback.m_collisionObject->getUserIndex();
                if (idx >= 0 && idx < static_cast<int>(world.boxes.size())) {
                    hoveredBoxIndex = idx;
                }
            }
        }
    }

    void setupCamera(int winWidth, int winHeight) const {
        glViewport(0, 0, winWidth, winHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(75.0, static_cast<double>(winWidth) / winHeight, 0.1, 150.0);

        Vec3 eyePos = getEyePos();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-eyePos.x, -eyePos.y, -eyePos.z);
    }

    void drawCrosshair(int width, int height) const {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        int cx = width / 2;
        int cy = height / 2;

        if (heldBoxIndex != -1)         glColor3f(1.0f, 0.6f, 0.0f);
        else if (hoveredBoxIndex != -1) glColor3f(0.2f, 1.0f, 0.3f);
        else                            glColor3f(0.90f, 0.90f, 0.90f);

        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2i(cx - 4, cy); glVertex2i(cx + 4, cy);
        glVertex2i(cx, cy - 4); glVertex2i(cx, cy + 4);
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
    }

    void cleanup(btDiscreteDynamicsWorld* dynamicsWorld) {
        dynamicsWorld->removeRigidBody(body);
        delete body->getMotionState();
        delete body;
        delete shape;
    }
};