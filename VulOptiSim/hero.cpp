#include "pch.h"
#include "hero.h"

// Initialiseert de hero met een model, texture, transform, naam en bewegingssnelheid.
Hero::Hero(const std::string& model, const std::string& texture, const Transform& transform, const std::string& name, const float speed)
    : model(model), texture(texture), transform(transform), name(name), speed(speed)
{
}

// Werkt de positie en hoogte van de hero bij op basis van de verstreken tijd en het terrein.
void Hero::update(const float delta_time, const Terrain& terrain)
{
    // Inactieve heroes hoeven niet bijgewerkt te worden.
    if (!active)
    {
        return;
    }

    float distance = delta_time * speed;
    glm::vec2 position = transform.get_position2d();

    // Verwerk de opgebouwde krachten en reset deze daarna.
    position += force;
    force = glm::vec2{ 0.f, 0.f };

    // Volg het ingestelde pad zolang er routepunten en bewegingsafstand over zijn.
    if (!route.empty())
    {
        while (!route.empty() && distance > 0.f)
        {
            glm::vec2 target = route.back();
            glm::vec2 target_direction = target - position;
            float distance_to_target = glm::length(target_direction);

            // Beweeg naar het doel zonder er voorbij te schieten.
            if (distance_to_target - distance > 0.0001f)
            {
                position += (distance / distance_to_target) * target_direction;
                distance = 0.f;
            }
            else
            {
                // Bereik het routepunt en gebruik de resterende afstand voor het volgende punt.
                position = target;
                distance -= distance_to_target;
            }

            // Laat de hero naar het huidige routepunt kijken.
            face_target(target_direction);

            // Verwijder het routepunt wanneer de hero dicht genoeg bij het doel is.
            if (glm::length2(target - position) < terrain.tile_width)
            {
                route.pop_back();
            }
        }
    }

    // Houd de positie binnen de grenzen van het terrein.
    if (!terrain.in_bounds(position))
    {
        terrain.clamp_to_bounds(position);
    }

    transform.set_position2d(position);

    // Pas de hoogte aan op basis van de terreinhoogte op de huidige positie.
    transform.set_height(terrain.get_height(position));
}

// Stelt een nieuwe route in voor de hero.
void Hero::set_route(const std::vector<glm::vec2>& new_route) {route = new_route;}

// Voegt een kracht toe die bij de volgende update op de hero wordt toegepast.
void Hero::push(glm::vec2 direction, float magnitude)
{
    if (!active)
    {
        return;
    }

    force += direction * magnitude;
}

// Vermindert de gezondheid van de hero en deactiveert deze wanneer de gezondheid op is.
void Hero::take_damage(int damage)
{
    if (!active)
    {
        return;
    }

    health -= damage;

    Log::get_instance()->add_log("%s takes %d damage.\n", name.c_str(), damage);

    if (health <= 0)
    {
        health = 0;
        active = false;

        Log::get_instance()->add_log("%s is down for the count!\n", name.c_str());
    }
}

// Vermindert de mana van de hero en zet deze op nul wanneer de mana op is.
void Hero::drain_mana(int cost)
{
    mana -= cost;

    Log::get_instance()->add_log("%s loses %d mana.\n", name.c_str(), cost);

    if (mana < 0)
    {
        mana = 0;

        Log::get_instance()->add_log("%s is oom, weakening the shield!\n", name.c_str());
    }
}

// Controleert of de hero botst met een bolvormig gebied in de 3D-ruimte.
bool Hero::collision(const glm::vec3& position, float radius) const
{
    return circle_collision(transform.position, collision_radius, position, radius);
}

// Controleert of de hero botst met een rechthoekig gebied in de 2D-ruimte.
bool Hero::collision(const glm::vec2& min, const glm::vec2 max) const
{
    glm::vec2 position = transform.get_position2d();

    return aabb_circle_collision(min, max, position, collision_radius);
}

// Geeft de huidige 3D-positie van de hero terug.
glm::vec3 Hero::get_position() const  {return transform.position;}

// Geeft de huidige 2D-positie van de hero terug.
glm::vec2 Hero::get_position2d() const  {return transform.get_position2d();}

// Geeft de botsingsradius van de hero terug.
float Hero::get_collision_radius() const  {return collision_radius;}

// Draait de hero zodat deze naar het opgegeven doel kijkt.
void Hero::face_target(const glm::vec2& target)
{
    glm::vec2 facing_direction = glm::normalize(target);
    float angle = atan2f(facing_direction.x, facing_direction.y);

    // Zet de berekende hoek om in een rotatiematrix.
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));

    transform.rotation = rotation;
}

// Tekent de hero met het bijbehorende model, de texture en de transformatiematrix.
void Hero::draw(vulvox::Renderer* renderer) const
{
    renderer->draw_model(model, texture, transform.get_matrix());
}