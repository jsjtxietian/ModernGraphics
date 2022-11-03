#include "physics.h"

// converts between Bullet and glm 3D vector representations:
static inline btVector3 Vec3ToBulletVec3(const vec3 &v)
{
	return btVector3(v.x, v.y, v.z);
}

void Physics::addBox(const glm::vec3 &halfSize, const btQuaternion &orientation, const vec3 &position, float mass)
{
	// The creation of a single solid box object
	boxTransform.push_back(glm::mat4(1.0f));

	btCollisionShape *collisionShape = new btBoxShape(Vec3ToBulletVec3(halfSize));
	btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(orientation, Vec3ToBulletVec3(position)));

	btVector3 localInertia(0, 0, 0);
	collisionShape->calculateLocalInertia(mass, localInertia);

	btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
		mass, motionState, collisionShape, localInertia);

	rigidBodyCI.m_friction = 0.1f;
	rigidBodyCI.m_rollingFriction = 0.1f;

	// A new btRigidBody object is created and stored in the rigidBodies array for
	// later use. We register the rigid body in the dynamicsWorld simulation object:
	rigidBodies.emplace_back(std::make_unique<btRigidBody>(rigidBodyCI));
	dynamicsWorld.addRigidBody(rigidBodies.back().get());
}

void Physics::update(float deltaSeconds)
{
	// calculates new positions and orientations for each rigid body participating in the simulation
	dynamicsWorld.stepSimulation(deltaSeconds, 10, 0.01f);

	// sync with physics
	// The synchronization itself consists of fetching the transformation for each
	// active body and storing that transformation in glm::mat4 format in the
	// boxTransform array. Our rendering application subsequently fetches that array
	// and uploads it into a GPU buffer for rendering:
	for (size_t i = 0; i != rigidBodies.size(); i++)
	{
		if (!rigidBodies[i]->isActive())
			continue;

		btTransform trans;
		rigidBodies[i]->getMotionState()->getWorldTransform(trans);
		trans.getOpenGLMatrix(glm::value_ptr(boxTransform[i]));
	}
}
