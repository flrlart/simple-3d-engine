#pragma once
#include <vector>
#include <GL/gl.h>
#include <btBulletDynamicsCommon.h>
#include "math.h"
#include "lighting.h"

struct Box {
    btRigidBody* body = nullptr;
    btCollisionShape* shape = nullptr;
    Vec3 halfSize;
    Vec3 color;
    Vec3 spawnPos;
    float mass = 1.0f;
};

class GameWorld {
public:
    btDefaultCollisionConfiguration* collisionConfig = nullptr;
    btCollisionDispatcher* dispatcher = nullptr;
    btBroadphaseInterface* broadphase = nullptr;
    btSequentialImpulseConstraintSolver* solver = nullptr;
    btDiscreteDynamicsWorld* dynamicsWorld = nullptr;

    btRigidBody* platformBody = nullptr;
    btCollisionShape* platformShape = nullptr;
    const float platformHalfSize = 10.0f;
    const float platformThickness = 1.2f;

    std::vector<Box> boxes;

    void init() {
        collisionConfig = new btDefaultCollisionConfiguration();
        dispatcher      = new btCollisionDispatcher(collisionConfig);
        broadphase      = new btDbvtBroadphase();
        solver          = new btSequentialImpulseConstraintSolver();
        dynamicsWorld   = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);
        dynamicsWorld->setGravity(btVector3(0.0f, -18.0f, 0.0f));

        createPlatform();
        createBoxes();
    }

    void resetBox(Box& b) {
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(b.spawnPos.x, b.spawnPos.y, b.spawnPos.z));
        b.body->setWorldTransform(trans);
        b.body->getMotionState()->setWorldTransform(trans);
        b.body->setLinearVelocity(btVector3(0, 0, 0));
        b.body->setAngularVelocity(btVector3(0, 0, 0));
        b.body->activate(true);
    }

    void resetAllBoxes() {
        for (auto& b : boxes) resetBox(b);
    }

    void stepPhysics(float dt, int& heldBoxIndex, btRigidBody* playerBody) {
        dynamicsWorld->stepSimulation(dt, 10, 1.0f / 120.0f);

        for (size_t i = 0; i < boxes.size(); ++i) {
            btTransform t;
            boxes[i].body->getMotionState()->getWorldTransform(t);
            if (t.getOrigin().y() < -30.0f) {
                if (static_cast<int>(i) == heldBoxIndex) {
                    boxes[i].body->setIgnoreCollisionCheck(playerBody, false);
                    playerBody->setIgnoreCollisionCheck(boxes[i].body, false);
                    heldBoxIndex = -1;
                }
                resetBox(boxes[i]);
            }
        }
    }

    void renderDepth() const {
        glDisable(GL_STENCIL_TEST);
        drawPlatformTop();
        drawPlatformSides();
        for (const auto& b : boxes) {
            drawBox(b, false);
        }
    }

    void render(const LightingSystem& lighting) const {
        // (void)lighting; // клод сказал закомментировать - я ему верю
        glDisable(GL_STENCIL_TEST);
        drawPlatformTop();
        drawPlatformSides();
        for (const auto& b : boxes) {
            drawBox(b, false);
        }
    }

    void cleanup() {
        for (auto& b : boxes) {
            dynamicsWorld->removeRigidBody(b.body);
            delete b.body->getMotionState();
            delete b.body;
            delete b.shape;
        }
        dynamicsWorld->removeRigidBody(platformBody);
        delete platformBody->getMotionState();
        delete platformBody;
        delete platformShape;

        delete dynamicsWorld;
        delete solver;
        delete broadphase;
        delete dispatcher;
        delete collisionConfig;
    }

private:
    void createPlatform() {
        platformShape = new btBoxShape(btVector3(platformHalfSize, platformThickness * 0.5f, platformHalfSize));
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(0.0f, -platformThickness * 0.5f, 0.0f));
        auto* motion = new btDefaultMotionState(trans);
        btRigidBody::btRigidBodyConstructionInfo ci(0.0f, motion, platformShape, btVector3(0, 0, 0));
        ci.m_friction = 0.90f;
        ci.m_restitution = 0.05f;
        platformBody = new btRigidBody(ci);
        dynamicsWorld->addRigidBody(platformBody);
    }

    void createBoxes() {
        struct BoxPreset { Vec3 pos; Vec3 halfSize; Vec3 color; };
        std::vector<BoxPreset> presets = {
            {{-2.0f, 0.5f, -1.5f},  {0.50f, 0.50f, 0.50f}, {0.72f, 0.48f, 0.24f}},
            {{-1.42f, 1.55f, -1.5f},{0.45f, 0.45f, 0.45f}, {0.76f, 0.52f, 0.28f}},
            {{ 1.5f, 0.5f, -2.0f},  {0.50f, 0.50f, 0.50f}, {0.68f, 0.44f, 0.22f}},
            {{ 0.0f, 0.3f,  0.8f},  {0.30f, 0.30f, 0.30f}, {0.82f, 0.68f, 0.22f}},
            {{-0.8f, 0.35f, 0.8f},  {0.35f, 0.35f, 0.35f}, {0.26f, 0.52f, 0.74f}},
            {{ 3.2f, 0.85f, -3.5f}, {0.85f, 0.85f, 0.85f}, {0.72f, 0.24f, 0.24f}},
            {{-3.8f, 0.6f, -3.5f},  {0.60f, 0.60f, 0.60f}, {0.28f, 0.62f, 0.38f}},
            {{ 0.5f, 0.4f, -4.0f},  {0.40f, 0.40f, 0.40f}, {0.58f, 0.36f, 0.68f}}
        };

        const float density = 22.0f;
        for (const auto& p : presets) {
            Box b;
            b.halfSize = p.halfSize;
            b.color = p.color;
            b.spawnPos = p.pos;

            float volume = (p.halfSize.x * 2.0f) * (p.halfSize.y * 2.0f) * (p.halfSize.z * 2.0f);
            b.mass = volume * density;

            b.shape = new btBoxShape(btVector3(p.halfSize.x, p.halfSize.y, p.halfSize.z));
            btTransform trans;
            trans.setIdentity();
            trans.setOrigin(btVector3(p.pos.x, p.pos.y, p.pos.z));

            btVector3 inertia(0, 0, 0);
            b.shape->calculateLocalInertia(b.mass, inertia);

            auto* motion = new btDefaultMotionState(trans);
            btRigidBody::btRigidBodyConstructionInfo rbInfo(b.mass, motion, b.shape, inertia);
            rbInfo.m_friction = 0.88f;
            rbInfo.m_restitution = 0.08f;
            rbInfo.m_rollingFriction = 0.14f;
            rbInfo.m_spinningFriction = 0.14f;

            b.body = new btRigidBody(rbInfo);
            b.body->setDamping(0.25f, 0.38f);
            b.body->setUserIndex(static_cast<int>(boxes.size()));
            dynamicsWorld->addRigidBody(b.body);

            boxes.push_back(b);
        }
    }

    static void renderUnitCube() {
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(-1, 1, -1); glVertex3f(-1, 1, 1); glVertex3f(1, 1, 1); glVertex3f(1, 1, -1);
        glNormal3f(0, -1, 0);
        glVertex3f(-1, -1, -1); glVertex3f(1, -1, -1); glVertex3f(1, -1, 1); glVertex3f(-1, -1, 1);
        glNormal3f(0, 0, 1);
        glVertex3f(-1, -1, 1); glVertex3f(1, -1, 1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
        glNormal3f(0, 0, -1);
        glVertex3f(-1, -1, -1); glVertex3f(-1, 1, -1); glVertex3f(1, 1, -1); glVertex3f(1, -1, -1);
        glNormal3f(1, 0, 0);
        glVertex3f(1, -1, -1); glVertex3f(1, 1, -1); glVertex3f(1, 1, 1); glVertex3f(1, -1, 1);
        glNormal3f(-1, 0, 0);
        glVertex3f(-1, -1, -1); glVertex3f(-1, -1, 1); glVertex3f(-1, 1, 1); glVertex3f(-1, 1, -1);
        glEnd();
    }

    static void drawBox(const Box& b, bool shadowPass) {
        btTransform trans;
        b.body->getMotionState()->getWorldTransform(trans);
        float glMat[16];
        trans.getOpenGLMatrix(glMat);

        glPushMatrix();
        glMultMatrixf(glMat);
        glScalef(b.halfSize.x, b.halfSize.y, b.halfSize.z);

        if (!shadowPass) {
            glColor3f(b.color.x, b.color.y, b.color.z);
        }

        renderUnitCube();
        glPopMatrix();
    }

    void drawPlatformTop() const {
        int grid = static_cast<int>(platformHalfSize);
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 1.0f, 0.0f);
        for (int x = -grid; x < grid; ++x) {
            for (int z = -grid; z < grid; ++z) {
                if (((x + z) & 1) != 0) glColor3f(0.42f, 0.45f, 0.49f);
                else      glColor3f(0.55f, 0.58f, 0.62f);

                glVertex3f(x, 0.0f, z);
                glVertex3f(x, 0.0f, z + 1.0f);
                glVertex3f(x + 1.0f, 0.0f, z + 1.0f);
                glVertex3f(x + 1.0f, 0.0f, z);
            }
        }
        glEnd();
    }

    void drawPlatformSides() const {
        float s = platformHalfSize;
        float b = -platformThickness;
        glColor3f(0.34f, 0.38f, 0.42f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0, 1);
        glVertex3f(-s, b, s); glVertex3f(s, b, s); glVertex3f(s, 0, s); glVertex3f(-s, 0, s);
        glNormal3f(0, 0, -1);
        glVertex3f(s, b, -s); glVertex3f(-s, b, -s); glVertex3f(-s, 0, -s); glVertex3f(s, 0, -s);
        glNormal3f(1, 0, 0);
        glVertex3f(s, b, s); glVertex3f(s, b, -s); glVertex3f(s, 0, -s); glVertex3f(s, 0, s);
        glNormal3f(-1, 0, 0);
        glVertex3f(-s, b, -s); glVertex3f(-s, b, s); glVertex3f(-s, 0, s); glVertex3f(-s, 0, -s);
        glNormal3f(0, -1, 0);
        glVertex3f(-s, b, -s); glVertex3f(s, b, -s); glVertex3f(s, b, s); glVertex3f(-s, b, s);
        glEnd();
    }
};