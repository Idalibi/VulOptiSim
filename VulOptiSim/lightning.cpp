#include "pch.h"
#include "lightning.h"
#include "hero.h"

Lightning::Lightning() = default;

Lightning::Lightning(glm::vec3 position)
    : animation_timer("lightning", 0, 10, 0.25f), transform(position)
{
    transform.scale = glm::vec3(plane_size.x, plane_size.y, 1.f);
    transform.position.y += plane_size.y / 4.f;

    // Stel de botsingsbox in op basis van de grootte van het vlak.
    collision_box_min = transform.get_position2d() - glm::vec2(plane_size.x / 2, plane_size.y / 2);
    collision_box_max = transform.get_position2d() + glm::vec2(plane_size.x / 2, plane_size.y / 2);
}

void Lightning::update(const float delta_time, const Camera& camera, std::vector<Hero>& heroes, const HeroPositions& hero_positions)
{
    if (!active)
    {
        return;
    }

    uptime += delta_time;

    if (uptime >= lifetime)
    {
        active = false;
        return;
    }

    rotate_to_camera(camera);
    animation_timer.update(delta_time);

    // Controleer of helden geraakt worden.
    check_hits(heroes, hero_positions);
}

void Lightning::register_draw(Sprite_Manager<Lightning>& sprite_manager) const
{
    if (active)
    {
        sprite_manager.register_draw(*this);
    }
}

void Lightning::check_hits(std::vector<Hero>& heroes, const HeroPositions& hero_positions) const
{
    if (!active)
    {
        return;
    }

    // Vergroot de botsingsbox om helden buiten bereik snel over te slaan.
    const float margin = hero_positions.max_radius + 0.01f;
    const glm::vec2 min = collision_box_min - glm::vec2(margin);
    const glm::vec2 max = collision_box_max + glm::vec2(margin);

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        const glm::vec3& position = hero_positions.positions[i];

        if (position.x < min.x || position.x > max.x ||
            position.z < min.y || position.z > max.y)
        {
            continue;
        }

        Hero& hero = heroes[i];

        if (hero.is_active() && hero.collision(collision_box_min, collision_box_max))
        {
            Log::get_instance()->add_log("%s is hit by lightning!\n", hero.get_name().c_str());
            hero.take_damage(damage_per_frame);
        }
    }
}

glm::mat4 Lightning::get_model_matrix() const {return transform.get_matrix();}

glm::uint32_t Lightning::get_texture_index() const {return animation_timer.get_current_frame();}

void Lightning::rotate_to_camera(const Camera& camera)
{
    // Draai de bliksem naar de camera, zodat de animatie zichtbaar blijft.
    glm::vec3 facing_direction = camera.get_position() - transform.position;
    facing_direction.y = 0.f;
    facing_direction = glm::normalize(facing_direction);

    const float angle = atan2f(facing_direction.x, facing_direction.z);

    transform.rotation = glm::rotate(
        glm::mat4(1.0f),
        angle,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}