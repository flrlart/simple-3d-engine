#pragma once
#include <algorithm>
#include <vector>
#include <GL/glew.h>
#include <btBulletDynamicsCommon.h>
#include "vec3.h"
#include "consts.h"
#include "geometry.h"
#include "materials.h"

struct Box {
    btRigidBody* body = nullptr;
    btCollisionShape* shape = nullptr;
    Vec3 halfSize;
    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    float mass = 0.0f;
    bool isStatic = false;
    btTransform spawn;
    Material* material = nullptr;
};

class GameWorld {
public:
    btDefaultCollisionConfiguration* collisionConfig = nullptr;
    btCollisionDispatcher* dispatcher = nullptr;
    btBroadphaseInterface* broadphase = nullptr;
    btSequentialImpulseConstraintSolver* solver = nullptr;
    btDiscreteDynamicsWorld* dynamicsWorld = nullptr;

    std::vector<Box> boxes;
    std::vector<Box> statics;

    float respawnY = consts::DEFAULT_RESPAWN_Y;
    Vec3 playerSpawnPos = consts::PLAYER_SPAWN_POS;
    float playerSpawnYaw = 0.0f;

    void init() {
        collisionConfig = new btDefaultCollisionConfiguration();
        dispatcher      = new btCollisionDispatcher(collisionConfig);
        broadphase      = new btDbvtBroadphase();
        solver          = new btSequentialImpulseConstraintSolver();
        dynamicsWorld   = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);
        dynamicsWorld->setGravity(btVector3(0.0f, consts::GRAVITY_Y, 0.0f));
    }

    void stepPhysics(float dt) {
        dynamicsWorld->stepSimulation(dt, consts::PHYSICS_MAX_SUBSTEPS, consts::PHYSICS_STEP);
    }

    void addObject(bool dynamic, const Vec3& pos, const Vec3& rotDeg, const Vec3& size, Material* material) {
        using namespace consts;
        constexpr float kDegToRad = 3.14159265f / 180.0f;

        Box b;
        b.isStatic = !dynamic;
        b.halfSize = Vec3(std::max(size.x, 0.01f) * 0.5f, std::max(size.y, 0.01f) * 0.5f, std::max(size.z, 0.01f) * 0.5f);
        b.material = material;
        b.color = material ? Vec3(1.0f, 1.0f, 1.0f) : DEFAULT_OBJECT_COLOR;

        btQuaternion rot;
        rot.setEuler(rotDeg.y * kDegToRad, rotDeg.x * kDegToRad, rotDeg.z * kDegToRad);
        b.spawn.setIdentity();
        b.spawn.setOrigin(btVector3(pos.x, pos.y, pos.z));
        b.spawn.setRotation(rot);

        b.shape = new btBoxShape(btVector3(b.halfSize.x, b.halfSize.y, b.halfSize.z));
        btVector3 inertia(0, 0, 0);
        if (dynamic) {
            b.mass = (b.halfSize.x * 2.0f) * (b.halfSize.y * 2.0f) * (b.halfSize.z * 2.0f) * BOX_DENSITY;
            b.shape->calculateLocalInertia(b.mass, inertia);
        }

        auto* motion = new btDefaultMotionState(b.spawn);
        btRigidBody::btRigidBodyConstructionInfo ci(b.mass, motion, b.shape, inertia);
        if (dynamic) {
            ci.m_friction = BOX_FRICTION;
            ci.m_restitution = BOX_RESTITUTION;
            ci.m_rollingFriction = BOX_ROLLING_FRICTION;
            ci.m_spinningFriction = BOX_SPINNING_FRICTION;
        } else {
            ci.m_friction = STATIC_FRICTION;
            ci.m_restitution = STATIC_RESTITUTION;
        }

        b.body = new btRigidBody(ci);
        if (dynamic) {
            b.body->setDamping(BOX_LINEAR_DAMPING, BOX_ANGULAR_DAMPING);
            b.body->setUserIndex(static_cast<int>(boxes.size()));
        }
        dynamicsWorld->addRigidBody(b.body);
        (dynamic ? boxes : statics).push_back(b);
    }

    void clearLevel() {
        auto destroy = [this](std::vector<Box>& list) {
            for (auto& b : list) {
                dynamicsWorld->removeRigidBody(b.body);
                delete b.body->getMotionState();
                delete b.body;
                delete b.shape;
            }
            list.clear();
        };
        destroy(boxes);
        destroy(statics);

        respawnY = consts::DEFAULT_RESPAWN_Y;
        playerSpawnPos = consts::PLAYER_SPAWN_POS;
        playerSpawnYaw = 0.0f;
    }

    template <typename OnRespawn>
    void respawnFallenBoxes(OnRespawn&& onRespawn) {
        for (size_t i = 0; i < boxes.size(); ++i) {
            btTransform t;
            boxes[i].body->getMotionState()->getWorldTransform(t);
            if (t.getOrigin().y() < respawnY) {
                onRespawn(static_cast<int>(i));
                resetBox(boxes[i]);
            }
        }
    }

    void resetAllBoxes() {
        for (auto& b : boxes) resetBox(b);
    }

    void render() const {
        for (const auto& s : statics) drawBox(s);
        for (const auto& b : boxes) drawBox(b);
    }

    void cleanup() {
        clearLevel();
        delete dynamicsWorld;
        delete solver;
        delete broadphase;
        delete dispatcher;
        delete collisionConfig;
        dynamicsWorld = nullptr;
    }

private:
    static void resetBox(Box& b) {
        b.body->setWorldTransform(b.spawn);
        b.body->getMotionState()->setWorldTransform(b.spawn);
        b.body->setLinearVelocity(btVector3(0, 0, 0));
        b.body->setAngularVelocity(btVector3(0, 0, 0));
        b.body->clearForces();
        b.body->activate(true);
    }

    static void drawBox(const Box& b) {
        btTransform trans;
        b.body->getMotionState()->getWorldTransform(trans);
        float glMat[16];
        trans.getOpenGLMatrix(glMat);

        Materials::Bind(b.material);

        glPushMatrix();
        glMultMatrixf(glMat);
        glScalef(b.halfSize.x, b.halfSize.y, b.halfSize.z);
        glColor3f(b.color.x, b.color.y, b.color.z);

        if (b.isStatic)  	drawUnitCube(b.halfSize.x * 2.0f, b.halfSize.y * 2.0f, b.halfSize.z * 2.0f);
        else 				drawUnitCube();

        glPopMatrix();
    }
};
