#include "pch.h"
#include "projectile.h"
#include "hero.h"


Projectile::Projectile()
{
}

Projectile::Projectile(glm::vec3 spawn_position, Hero* target) : target(target), transform(spawn_position), animation_timer("fireball", 0, 33, 0.1f)
{
    transform.scale = glm::vec3(10.f);
    direction = glm::normalize(target->get_position() - spawn_position);
}

void Projectile::update(const float delta_time, const Camera& camera, const Shield& shield, std::vector<Hero>& heroes, const HeroPositions& hero_positions) 
{
    if (active)
    {
        if (exploding)
        {
            explosion_timer += delta_time;
            animation_timer.update(delta_time);
            rotate_to_camera(camera);

            if (explosion_timer >= explosion_duration)
            {
                active = false;
            }
            return;
        }
        uptime += delta_time;

        if (uptime >= lifetime)
        {
            active = false;
            return;
        }

        if (target)
        {
            direction = glm::normalize(target->get_position() - transform.position);
        }

        transform.position += direction * speed * delta_time;

        rotate_to_camera(camera);

        animation_timer.update(delta_time);

        //Disable if the projectile collides with the shield
        if (shield.intersects(transform.get_position2d(), radius))
        {
            Log::get_instance()->add_log("The projectile hits the shield, draining mana.\n");

            shield.absorb(heroes, transform.get_position2d());
            active = false;
        }

        check_collisions(heroes, hero_positions);
    }
}

void Projectile::check_collisions(std::vector<Hero>& heroes, const HeroPositions& hero_positions)
{
    // Voorfilter in het xz-vlak. De echte 3D-afstand is nooit kleiner dan de afstand in xz,
    // dus wat hier te ver weg is, kan nooit raken.
    const float reach = radius + hero_positions.max_radius + 0.01f;
    const float reach_sq = reach * reach;

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        const glm::vec3& p = hero_positions.positions[i];
        const float dx = p.x - transform.position.x;
        const float dz = p.z - transform.position.z;
        if (dx * dx + dz * dz > reach_sq)
        {
            continue;
        }

        if (heroes[i].collision(transform.position, radius))
        {
            Log::get_instance()->add_log("The projectile explodes near %s.\n", heroes[i].get_name());

            explode(heroes, hero_positions);

            break; //Projectile exploded, exit
        }
    }
}

void Projectile::explode(std::vector<Hero>& heroes, const HeroPositions& hero_positions)
{
    const float reach = explosion_radius + hero_positions.max_radius + 0.01f;
    const float reach_sq = reach * reach;

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        const glm::vec3& p = hero_positions.positions[i];
        const float dx = p.x - transform.position.x;
        const float dz = p.z - transform.position.z;
        if (dx * dx + dz * dz > reach_sq)
        {
            continue;
        }

        if (heroes[i].collision(transform.position, explosion_radius))
        {
            heroes[i].take_damage(damage);
        }
    }

    exploding = true;
    explosion_timer = 0.f;
    transform.scale = glm::vec3(explosion_radius * 2.f); // pas de grootte aan naar smaak
}

void Projectile::register_draw(Sprite_Manager<Projectile>& sprite_manager) const
{
    if (active)
    {
        sprite_manager.register_draw(*this);
    }
}

glm::mat4 Projectile::get_model_matrix() const
{
    return transform.get_matrix();
}

glm::uint32_t Projectile::get_texture_index() const
{
    return animation_timer.get_current_frame();
}


void Projectile::rotate_to_camera(const Camera& camera)
{
    ////Rotate so the animation is always facing the camera
    glm::vec3 projectile_direction = glm::normalize(direction);
    glm::vec3 rot_axis = glm::normalize(glm::cross(glm::vec3(0, 1, 0), projectile_direction));
    float angle = acosf(glm::dot(glm::vec3(0, 1, 0), projectile_direction));

    glm::mat4 rotate_to_target = glm::rotate(glm::mat4(1.0f), angle, rot_axis);

    glm::vec3 normal_vec = rotate_to_target * glm::vec4(0.f, 0.f, 1.f, 0.f);
    glm::vec3 camera_direction = glm::normalize(camera.get_position() - transform.position);

    float camera_angle = acosf(glm::dot(normal_vec, camera_direction));

    glm::mat4 rotate_to_camera = glm::rotate(glm::mat4(1.0f), camera_angle, projectile_direction);


    transform.rotation = rotate_to_camera * rotate_to_target;



    //glm::vec3 facing_direction = glm::normalize(camera.get_position() - transform.position);



    //glm::vec3 facing_direction = glm::normalize(camera.get_position() - transform.position);

    //glm::vec3 rotation_axis = glm::normalize(direction);

    //float angle = acosf(glm::dot(glm::vec3(0, 1, 0), facing_direction));

    //glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, rotation_axis);

    //transform.rotation = rotation;

}