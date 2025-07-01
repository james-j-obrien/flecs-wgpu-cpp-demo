#include "../include.hpp"
#include <box2d/box2d.h>

namespace physics {

module::module(flecs::world &world) {
    b2Vec2 gravity{0.0f, -10.0f};
    b2World *b_world = new b2World(gravity);
    world.set<PhysicsWorld>({b_world});

    world.observer<Quad, DynamicBody>()
        .without<BodyPtr>()
        .event(flecs::OnSet)
        .each([](flecs::entity e, Quad &rect, DynamicBody &def) {
            b2World *p_world = e.world().ensure<PhysicsWorld>().ptr;
            Position &pos = e.ensure<Position>();

            // Create dynamic box
            b2BodyDef body_def;
            body_def.type = b2_dynamicBody;
            body_def.position.Set(pos.x, pos.y);
            b2Body *body = p_world->CreateBody(&body_def);

            b2PolygonShape box;
            box.SetAsBox(rect.width / 2.0f, rect.height / 2.0f);

            b2FixtureDef box_fixture;
            box_fixture.shape = &box;
            box_fixture.density = def.density;
            box_fixture.friction = def.friction;

            body->CreateFixture(&box_fixture);

            e.set<BodyPtr>({body});
        });

    world.observer<Circle, DynamicBody>()
        .without<BodyPtr>()
        .event(flecs::OnSet)
        .each([](flecs::entity e, Circle &circle, DynamicBody &def) {
            b2World *p_world = e.world().ensure<PhysicsWorld>().ptr;
            Position &pos = e.ensure<Position>();

            // Create dynamic box
            b2BodyDef body_def;
            body_def.type = b2_dynamicBody;
            body_def.position.Set(pos.x, pos.y);
            b2Body *body = p_world->CreateBody(&body_def);

            b2CircleShape shape;
            // shape.m_p.Set(pos.x, pos.y);
            shape.m_radius = circle.radius;

            b2FixtureDef fixture;
            fixture.shape = &shape;
            fixture.density = def.density;
            fixture.friction = def.friction;

            body->CreateFixture(&fixture);

            e.set<BodyPtr>({body});
        });

    world.observer<Quad, StaticBody>()
        .without<BodyPtr>()
        .event(flecs::OnSet)
        .each([](flecs::entity e, Quad &rect, StaticBody) {
            b2World *p_world = e.world().ensure<PhysicsWorld>().ptr;
            Position &pos = e.ensure<Position>();

            // Create static box
            b2BodyDef body_def;
            body_def.type = b2_staticBody;
            body_def.position.Set(pos.x, pos.y);
            b2Body *body = p_world->CreateBody(&body_def);

            b2PolygonShape box;
            box.SetAsBox(rect.width / 2.0f, rect.height / 2.0f);
            body->CreateFixture(&box, 0.0f);

            e.set<BodyPtr>({body});
        });

    world.system<BodyPtr, Impulse>().each([](flecs::entity e, BodyPtr &body, Impulse &impulse) {
        b2Vec2 vec{impulse.x * 300.0f, impulse.y * 300.0f};
        body.ptr->ApplyLinearImpulseToCenter(vec, true);
        e.remove<Impulse>();
    });

    world.system<PhysicsWorld>().each(
        [](PhysicsWorld &p_world) { p_world.ptr->Step(1.0f / 60.0f, 8, 3); });

    world.system<BodyPtr, Position>().with<DynamicBody>().each(
        [](BodyPtr &body, Position &position) {
            auto pos = body.ptr->GetPosition();
            auto rot = body.ptr->GetAngle();
            position.x = pos.x;
            position.y = pos.y;
            position.rotation = rot;
        });
}

} // namespace physics