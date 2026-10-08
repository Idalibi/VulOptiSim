#pragma once
#include <vector>
#include <glm/glm.hpp>

// Compacte kopie van de hero-posities (zelfde index als Scene::heroes), één keer per frame gevuld.
struct HeroPositions
{
    std::vector<glm::vec3> positions;
    float max_radius = 0.5f; // grootste collision-radius van alle heroes
};

class Hero
{
public:

    Hero(const std::string& model, const std::string& texture, const Transform& transform, const std::string& name, const float speed);

    void update(const float delta_time, const Terrain& terrain);
    void draw(vulvox::Renderer* renderer) const;

    void set_route(const std::vector<glm::vec2>& new_route);

    void push(glm::vec2 direction, float magnitude);

    void take_damage(int damage);
    void drain_mana(int cost);

    /// <summary>
    /// Collision check with a sphere
    /// </summary>
    bool collision(const glm::vec3& position, float radius) const;

    /// <summary>
    /// Collision check with a 2d collision box
    /// </summary>
    bool collision(const glm::vec2& min, const glm::vec2 max) const;

    glm::vec3 get_position() const;
    glm::vec2 get_position2d() const;

    float get_collision_radius() const;

    int get_health() const { return health; };
    int get_mana() const { return mana; };
    glm::mat4 get_matrix() const { return transform.get_matrix(); }

    bool is_active() const { return active; }; //Wat betekend is active?

    std::string get_name() const { return name; };


private:

    void face_target(const glm::vec2& target);

    std::string name;

    bool active = true;
    int health = 1000;
    int mana = 1000;

    std::string model;
    std::string texture;

    Transform transform;
    float speed;

    float collision_radius = 0.5f;
    glm::vec2 force = glm::vec2{ 0.f,0.f };

    std::vector<glm::vec2> route;

};

