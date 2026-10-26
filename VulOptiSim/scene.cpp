#include "pch.h"
#include "scene.h"
#include "SpatialGrid.h" 
#include <algorithm>     
#include <array>
#include <cstdio>
#include <chrono>

namespace
{
    // Meet hoe lang een blok code duurt (ms), gemiddeld over de laatste frames
    struct ScopedTimer
    {
        explicit ScopedTimer(double& out_ms) : out(out_ms), start(std::chrono::steady_clock::now()) {}
        ~ScopedTimer()
        {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            out = out * 0.9 + ms * 0.1;
        }
        double& out;
        std::chrono::steady_clock::time_point start;
    };

    double g_collision_ms = 0.0;
    double g_hero_update_ms = 0.0;
    double g_shield_ms = 0.0;
    double g_effects_ms = 0.0;
    double g_update_total_ms = 0.0;
    double g_draw_total_ms = 0.0;
    size_t g_pairs_checked = 0;
    size_t g_pairs_colliding = 0;
    size_t g_cell_count = 0;
}

Scene::Scene(vulvox::Renderer& renderer) : renderer(&renderer)
{
    glfwGetCursorPos(this->renderer->get_window(), &prev_mouse_pos.x, &prev_mouse_pos.y);

    glm::vec3 camera_pos{ -28.2815380f, 305.485260f, -30.0800228f };
    glm::vec3 camera_up{ 0.338442326f, 0.869414926f, 0.359964609f };
    glm::vec3 camera_direction{ 0.595541596f, -0.494082689f, 0.633413374f };

    camera = Camera(camera_pos, camera_up, camera_direction, 100.f, 100.f);
    terrain = Terrain(TERRAIN_PATH);

    load_models_and_textures();
    spawn_heroes();
    spawn_staves();

    std::cout << "Scene loaded." << std::endl;
}

//Laad alle modellen en textures van alle elementen alvast in de GPU geheugen.
void Scene::load_models_and_textures() const
{
    //Terrain textures
    std::vector<std::filesystem::path> texture_paths{
        CUBE_SEA_TEXTURE_PATH,              //Sea
        CUBE_GRASS_FLOWER_TEXTURE_PATH,     //Lab floor
        CUBE_CONCRETE_WALL_TEXTURE_PATH,    //Lab walls
        CUBE_MOSS_TEXTURE_PATH };           //Floor
    renderer->load_texture_array("texture_array_test", texture_paths);
    renderer->load_model("cube", CUBE_MODEL_PATH);

    //Heroes texture en model
    renderer->load_model("frieren-blob", FRIEREN_PATH); 
    std::vector<std::filesystem::path> frieren_texture{ FRIEREN_TEXTURE_PATH };
    renderer->load_texture_array("frieren-blob-array", frieren_texture);

    //Staf texture en model
    renderer->load_model("staff", STAFF_PATH);
    renderer->load_texture("staff", STAFF_TEXTURE_PATH);

    //Schild en effecten
    std::vector<std::filesystem::path> shield_path{ SHIELD_TEXTURE_PATH };
    renderer->load_texture_array("shield", shield_path);
    renderer->load_texture_array("lightning", LIGHTNING_TEXTURE_PATHS);
    renderer->load_texture_array("fireball", FIREBALL_TEXTURE_PATHS);
}

//Plaatst de heroes in de spawnvakken.
void Scene::spawn_heroes() 
{
    Transform hero_transform;
    hero_transform.rotation = glm::quatLookAt(glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 1.f, 0.f));
    hero_transform.scale = glm::vec3(1.f);

    float spawn_offset = terrain.tile_width / 3.f;

    int start_areas = 10;
    float start_area_tile_offset = 12.f;
    float spawn_start_y = terrain.tile_width * 3.f;

    float start_corner_y = 9.f * terrain.tile_width;

    // NIEUW: Hoe ver een hero van het midden van de tegels af mag lopen. Maximaal de halve tegel,
    // min de straal van de hero zelf, anders steekt zijn lichaam in de muur ernaast en komt die er bovenop.
    const float hero_radius = 0.5f;
    const float max_offset_x = std::max(0.f, 0.5f * terrain.tile_width - hero_radius);
    const float max_offset_z = std::max(0.f, 0.5f * terrain.tile_length - hero_radius);
    const float route_spread = 0.8f;     // deel van die vrije ruimte dat gebruikt wordt (0 = uit, 1 = tot de rand)
    const float offset_range_x = route_spread * max_offset_x;
    const float offset_range_z = route_spread * max_offset_z;

    std::cout << "Route spread: tegel " << terrain.tile_width << " x " << terrain.tile_length << ", verschuiving tot " << offset_range_x << " x " << offset_range_z << std::endl;
    std::cout << "Spawning characters and calculating routes...(met een threadpool hopelijk)" << std::endl;

    unsigned int num_threads = std::thread::hardware_concurrency();
    ThreadPool pool(num_threads > 0 ? num_threads : 4);

    //Helper struct voor de informatie van heroes
    struct SpawnedHeroInfo {
        std::string mesh1, mesh2;
        Transform transform;
        std::string name;
        float speed;
        std::vector<glm::vec2> route;
    };

    //Opslaan zodat we wachten tot alle groepen klaar zijn.
    std::vector<std::future<std::vector<SpawnedHeroInfo>>> futures;
    futures.reserve(start_areas);

    //Verdeel de 10 startgebieden over de workers in de threadpool.
    for (int s = 0; s < start_areas; s++) {

        futures.push_back(pool.enqueue([=, this]() {
            std::vector<SpawnedHeroInfo> local_heroes;
            local_heroes.reserve(30 * 30);

            float start_area_offset = static_cast<float>(s) * start_area_tile_offset * terrain.tile_width;
            int local_spawn_count = s * 900; // Om unieke namen te behouden.

            // NIEUW: Een eigen random generator per startgebied (thread-veilig). Een vast zaadje geeft elke run dezelfde uitkomst.
            std::mt19937 rng(static_cast<unsigned int>(s) + 1u);
            std::uniform_real_distribution<float> unit(-1.f, 1.f);

            for (int i = 0; i < 30; i++) //O(30) O(start_area * 30)
            {
                for (int j = 0; j < 30; j++) //O(30) O start_area * 30 * 30) = O(start_area * 900)
                {
                    float x = start_corner_y + start_area_offset + ((float)i * spawn_offset);
                    float z = spawn_start_y + ((float)j * spawn_offset);
                    float y = terrain.get_height(glm::vec2(x, z));

                    Transform local_transform = hero_transform;
                    local_transform.position = glm::vec3(x, y, z);

                    local_spawn_count++;

                    auto r = terrain.find_route(
                        glm::vec2(x, z),
                        glm::vec2(69 * terrain.tile_width, 160 * terrain.tile_width)
                    );

                    // Elke hero krijgt een vaste verschuiving binnen de tegel, zodat niet iedereen,
                    // over exact dezelfde lijn van tegelmiddens loopt.
                    const glm::vec2 offset(unit(rng)* offset_range_x, unit(rng)* offset_range_z);
                    for (auto& waypoint : r)
                    {
                        waypoint += offset;
                    }

                    local_heroes.push_back({ "frieren-blob", "frieren-blob", local_transform, "Frieren" + std::to_string(local_spawn_count), 20.f, r });
                }
            }
            return local_heroes;
        }));
    }

    int total_spawn_count = 0;
    for (auto& f : futures) {
        std::vector<SpawnedHeroInfo> area_heroes = f.get();
        for (const auto& h : area_heroes) {
            heroes.emplace_back(h.mesh1, h.mesh2, h.transform, h.name, h.speed);
            heroes.back().set_route(h.route);
            total_spawn_count++;
        }
    }
    Log::get_instance()->add_log("Spawned %d characters using ThreadPool.\n", total_spawn_count);
    std::cout << "Spawning finished successfully!" << std::endl;
}


void Scene::spawn_staves()
{
    glm::vec2 spawn_start{ terrain.tile_width * 15.f,  terrain.tile_length * 48.f };
    float height = terrain.get_height(spawn_start) + 50.f;

    float spawn_offset_x = 12.f * terrain.tile_height;
    float spawn_offset_y = 40.f * terrain.tile_length;

    int spawn_count = 0;
    for (int i = 0; i < 10; i++) // O(10)
    {
        for (int j = 0; j < 2; j++) // O(2) = O(20)
        {
            spawn_count++;
            glm::vec3 position{ spawn_start.x + i * spawn_offset_x, height, spawn_start.y + j * spawn_offset_y };
            staves.emplace_back("Staff" + std::to_string(spawn_count), position, &terrain);
        }
    }

    Log::get_instance()->add_log("Spawned %d staves.\n", spawn_count);
}

//Geeft hoeveelheid heroes terug
size_t Scene::get_character_count() const {return heroes.size();}
//Geeft hoeveelheid staffs terug
size_t Scene::get_staff_count() const {return staves.size();}

void Scene::update(const float delta_time)
{
    //Meten is weten :D
    ScopedTimer total_timer(g_update_total_ms);
    handle_input(delta_time);

    if (follow_mode)
    {
        auto it = std::ranges::max_element(heroes,
            [](const Hero& a, const Hero& b) {
                return a.get_position().z < b.get_position().z;
            }
        );

        glm::vec3 new_camera_position{ -28.0f, 305.5f, it->get_position().z };
        camera.set_position(new_camera_position);

        glm::vec3 camera_to_furthest = it->get_position() - camera.get_position();
        camera.set_direction(camera_to_furthest);
    }

    renderer->set_view_matrix(camera.get_view_matrix());


    //Meten is weten :D
    ScopedTimer t_collision(g_collision_ms);

    static SpatialGrid spatial_grid(5.0f);
    spatial_grid.clear();

    //Posities en radii één keer per frame in compacte arrays (cache-vriendelijk).
    static std::vector<glm::vec2> positions;
    static std::vector<float> radii;
    positions.resize(heroes.size());
    radii.resize(heroes.size());

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        if (heroes[i].is_active())
        {
            positions[i] = heroes[i].get_position2d();
            radii[i] = heroes[i].get_collision_radius();
            spatial_grid.insert(i, positions[i]);
        }
    }

    //Meten is weten :D
    size_t pairs_checked = 0;       
    size_t pairs_colliding = 0;     
    static std::vector<glm::vec2> pushes;
    pushes.assign(heroes.size(), glm::vec2(0.f));

    //Los één botsend paar op.
    auto resolve_pair = [&](size_t i, size_t j)
        {
            ++pairs_checked;
            
            const glm::vec2 direction = positions[j] - positions[i];
            const float rad_sum = radii[i] + radii[j];

            // Snelle afstand-check zonder wortel.
            const float dist_sq = glm::dot(direction, direction);
            if (dist_sq >= rad_sum * rad_sum) return;

            ++pairs_colliding;

            float distance = std::sqrt(dist_sq);
            glm::vec2 norm_direction;

            if (distance > 0.0001f)
            {
                norm_direction = direction / distance;
            }
            else
            {
                norm_direction = glm::vec2(1.0f, 0.0f);
                distance = 0.0001f;
            }

            const float overlap = rad_sum - distance;

            pushes[i] -= norm_direction * (overlap * 0.5f);
            pushes[j] += norm_direction * (overlap * 0.5f);
        };

    //Alleen 4 van de 8 buren: elk celpaar wordt zo precies één keer bezocht.
    static const std::array<std::pair<int, int>, 4> half_neighbors{ { {1, 0}, {-1, 1}, {0, 1}, {1, 1} } };

    const auto& cells = spatial_grid.get_cells();
    for (const auto& [cell_coord, cell_hero_indices] : cells)
    {
        //Paren binnen dezelfde cel.
        for (size_t a = 0; a < cell_hero_indices.size(); ++a)
        {
            for (size_t b = a + 1; b < cell_hero_indices.size(); ++b)
            {
                resolve_pair(cell_hero_indices[a], cell_hero_indices[b]);
            }
        }

        //Paren met de 4 "voorwaartse" buurcellen.
        for (const auto& [dx, dy] : half_neighbors)
        {
            SpatialGrid::CellCoord neighbor_coord = { cell_coord.first + dx, cell_coord.second + dy };
            auto neighbor_it = cells.find(neighbor_coord);
            if (neighbor_it == cells.end()) continue;

            for (size_t i : cell_hero_indices)
            {
                for (size_t j : neighbor_it->second)
                {
                    resolve_pair(i, j);
                }
            }
        }
    }

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        if (pushes[i] != glm::vec2(0.f))
        {
            heroes[i].push(pushes[i], 1.0f);
        }
    }

    g_pairs_checked = pairs_checked;
    g_pairs_colliding = pairs_colliding;
    g_cell_count = cells.size();

    //Meten is weten :D
    ScopedTimer t_hero(g_hero_update_ms);

    static HeroPositions hero_positions;
    static const unsigned int thread_count = std::max(1u, std::thread::hardware_concurrency());
    static ThreadPool pool(thread_count);

    hero_positions.positions.resize(heroes.size());

    std::vector<std::future<float>> futures;
    //Reserveer nodige opslag voor de threads om naar te schrijven. (tegen dubbel schrijven naar hetzelfde adres)
    futures.reserve(thread_count);
    const size_t chunk = (heroes.size() + thread_count - 1) / thread_count;

    for (size_t begin = 0; begin < heroes.size(); begin += chunk)
    {
        const size_t end = std::min(begin + chunk, heroes.size());

        futures.push_back(pool.enqueue([&, begin, end]()
            {
                float local_max = 0.f;
                for (size_t i = begin; i < end; ++i)
                {
                    heroes[i].update(delta_time, terrain);
                    hero_positions.positions[i] = heroes[i].get_position();
                    local_max = std::max(local_max, heroes[i].get_collision_radius());
                }
                return local_max;
            }));
    }

    hero_positions.max_radius = 0.f;
    for (auto& f : futures)
    {
        hero_positions.max_radius = std::max(hero_positions.max_radius, f.get());
    }

    //Meten is weten :D
    ScopedTimer t_shield(g_shield_ms);
    constexpr float SHIELD_REFRESH_INTERVAL = 0.05f;               // Aantal seconden tussen twee herberekeningen van de shield.
    static float shield_timer = SHIELD_REFRESH_INTERVAL;           // Begint "vol", dus de eerste frame bouwt hij meteen een scherm.

    shield_timer += delta_time;
    if (shield_timer >= SHIELD_REFRESH_INTERVAL)
    {
        shield = Shield{ "shield", heroes };
        shield_timer = 0.f;
    }

    //Meten is weten :D
    ScopedTimer t_effects(g_effects_ms);
    for (auto& staff : staves)
    {
        staff.update(delta_time, heroes, active_lightning, projectiles);
    }

    for (auto& lightning : active_lightning)
    {
        lightning.update(delta_time, camera, heroes, hero_positions);   
    }

    //Verwijder inactieve lightning.
    const auto [first_l, last_l] = std::ranges::remove_if(active_lightning, [](const Lightning& l) { return !l.is_active(); });
    active_lightning.erase(first_l, last_l);

    for (auto& projectile : projectiles)
    {
        projectile.update(delta_time, camera, shield, heroes, hero_positions);   
    }

    //Verwijder inactieve projectiles.
    const auto [first_p, last_p] = std::ranges::remove_if(projectiles, [](const Projectile& p) { return !p.is_active(); });
    projectiles.erase(first_p, last_p);
}

void Scene::draw()
{
    //Meten is weten :D
    ScopedTimer total_timer(g_draw_total_ms);
    //On using concurrency here:
    //  The graphics library copies the data to GPU memory so you can change the data after the draw functions of the renderer return.
    //  The actual drawing runs parallel to the host (CPU) execution.
    //  Make sure the data needed for drawing (position etc.) is ready before calling the corresponding draw functions or weird things happen.
    //  Calling draw functions outside of this functions lifetime will crash the program!

    //  Is goed Gert! :D

    static std::vector<glm::mat4> hero_matrices;
    static std::vector<uint32_t> hero_texture_indices;
    hero_matrices.clear();
    hero_texture_indices.clear();

    for (const auto& hero : heroes)
    {
        if (!hero.is_active()) continue;
        hero_matrices.push_back(hero.get_matrix());
        hero_texture_indices.push_back(0);
    }

    renderer->draw_instanced_with_texture_array("frieren-blob", "frieren-blob-array", hero_matrices, hero_texture_indices);
    terrain.draw(renderer);

    //Meten is weten :D
    ScopedTimer t_effects(g_effects_ms);

    for (const auto& staff : staves)
    {
        staff.draw(renderer);
    }

    for (const auto& lightning : active_lightning)
    {
        lightning.register_draw(lightning_sprite_manager);
    }

    lightning_sprite_manager.draw(renderer);
    lightning_sprite_manager.reset();

    for (const auto& projectile : projectiles)
    {
        projectile.register_draw(projectile_sprite_manager);
    }

    projectile_sprite_manager.draw(renderer);
    projectile_sprite_manager.reset();

    shield.draw(renderer);

    //Ik had het gevoel alsof ik een fps cap had op 60 dus check dat hiermee. Meten is weten :D.
    const double frame_ms = 1000.0 / ImGui::GetIO().Framerate;
    ImGui::Begin("Timings");
    ImGui::Text("Frame:        %.2f ms (%.0f fps)", frame_ms, ImGui::GetIO().Framerate);
    ImGui::Text("Update totaal:%.2f ms", g_update_total_ms);
    ImGui::Text("Draw totaal:  %.2f ms", g_draw_total_ms);
    ImGui::Text("Overig/wacht: %.2f ms", frame_ms - g_update_total_ms - g_draw_total_ms);
    ImGui::Separator();
    ImGui::Text("Botsingen:    %.2f ms", g_collision_ms);
    ImGui::Text("Hero update:  %.2f ms", g_hero_update_ms);
    ImGui::Text("Schild:       %.2f ms", g_shield_ms);
    ImGui::Text("Effecten:     %.2f ms", g_effects_ms);
    ImGui::Separator();
    ImGui::Text("Paren gecontroleerd: %zu", g_pairs_checked);
    ImGui::Text("Paren die botsen:    %zu", g_pairs_colliding);
    ImGui::Text("Cellen: %zu (ca. %.1f heroes per cel)", g_cell_count,
        g_cell_count ? static_cast<double>(heroes.size()) / g_cell_count : 0.0);
    ImGui::End();

    show_health_values();
    show_mana_values();

    Log::get_instance()->draw("Log");

    show_controls();
}

namespace
{
    constexpr int    MAX_STAT = 1000;       
    constexpr double REFRESH_INTERVAL = 0.25; // Hoe vaak (in seconden) de lijst opnieuw gesorteerd wordt in plaats van ieder frame.

    struct StatCache
    {
        std::vector<int> sorted;
        double last_update = -1.0;
    };

    // Counting sort op een vaste range: O(n + 1000), geen vergelijkingen nodig oftewel sneller :D.
    template <typename Getter>
    void rebuild_sorted(const std::vector<Hero>& heroes, Getter get_value, std::vector<int>& out)
    {
        std::array<int, MAX_STAT + 1> counts{}; // Alles op 0

        for (const auto& h : heroes)
        {
            if (!h.is_active()) continue;
            ++counts[std::clamp(get_value(h), 0, MAX_STAT)];
        }

        out.clear();
        out.reserve(heroes.size());
        for (int v = 0; v <= MAX_STAT; ++v)
        {
            out.insert(out.end(), static_cast<size_t>(counts[v]), v);
        }
    }

    // Gedeelde tekenfunctie voor het health- en mana-venster
    template <typename Getter>
    void draw_stat_window(const char* title, const ImVec4& color,
        const std::vector<Hero>& heroes, Getter get_value, StatCache& cache)
    {
        // Als het venster dicht of ingeklapt is, sla alle berekeningen over
        if (!ImGui::Begin(title))
        {
            ImGui::End();
            return;
        }

        // Alleen opnieuw sorteren als de cache oud genoeg is
        const double now = ImGui::GetTime();
        if (now - cache.last_update >= REFRESH_INTERVAL)
        {
            rebuild_sorted(heroes, get_value, cache.sorted);
            cache.last_update = now;
        }

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);

        // Virtual scrolling: alleen de zichtbare balkjes worden getekend
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(cache.sorted.size()));

        char buf[32]; // Stackbuffer, geen stringstream
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const int value = cache.sorted[i];
                snprintf(buf, sizeof(buf), "%d/%d", value, MAX_STAT);
                ImGui::ProgressBar(static_cast<float>(value) / MAX_STAT, ImVec2(-FLT_MIN, 0.0f), buf);
            }
        }

        ImGui::PopStyleColor(1);
        ImGui::End();
    }
}

/// <summary>
/// Shows all active heroes' health values, sorted, in a window (cached, with virtual scrolling).
/// </summary>
void Scene::show_health_values() const
{
    static StatCache cache;
    draw_stat_window("Heroes Health Bars", ImVec4(0.0f, 0.5f, 0.0f, 1.0f),
        heroes, [](const Hero& h) { return h.get_health(); }, cache);
}

/// <summary>
/// Shows all active heroes' mana values, sorted, in a window (cached, with virtual scrolling).
/// </summary>
void Scene::show_mana_values() const
{
    static StatCache cache;
    draw_stat_window("Heroes Mana Bars", ImVec4(0.0f, 0.0f, 0.5f, 1.0f),
        heroes, [](const Hero& h) { return h.get_mana(); }, cache);
}

//Sorteerfunctie Counting Sort
std::vector<int> Scene::sort(const std::vector<int>& to_sort) const 
{   
    // Counting Sort is een algoritme dat niet de waarden vergelijkt met elkaar maar door te tellen hoe vaak elke waarde voorkomt.
    // Deze is lineair wat het wel een stuk sneller maakt dan vele anderen. O(n)
    if (to_sort.empty()) return to_sort;
    std::vector<int> sorted_list = to_sort;

    // Zoek de kleinste en grootste waarde in de lijst
    int min_val = to_sort[0]; 
    int max_val = to_sort[0]; 
    for (int val : to_sort)
    {
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    // Maak een frequentietabel (hoe vaak komt elke HP/Mana waarde voor?)
    int range = max_val - min_val + 1; 
    std::vector<int> count(range, 0);

    for (int val : to_sort)
    {
        count[val - min_val]++;
    }

    // Bouw de gesorteerde lijst op
    sorted_list.clear();
    sorted_list.reserve(to_sort.size());

    for (int i = 0; i < range; ++i)
    {
        while (count[i] > 0)
        {
            sorted_list.push_back(i + min_val);
            count[i]--;
        }
    }
    return sorted_list;
}

void Scene::handle_input(const float delta_time)
{
    //Toggle follow mode
    if (glfwGetKey(renderer->get_window(), GLFW_KEY_TAB) == GLFW_PRESS) { follow_mode = !follow_mode; }

    if (!follow_mode)
    {
        //Update camera on key presses
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_W) == GLFW_PRESS) { camera.move_forward(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_S) == GLFW_PRESS) { camera.move_backward(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_Q) == GLFW_PRESS) { camera.move_left(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_E) == GLFW_PRESS) { camera.move_right(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_A) == GLFW_PRESS) { camera.rotate_left(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_D) == GLFW_PRESS) { camera.rotate_right(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_SPACE) == GLFW_PRESS) { camera.move_up(delta_time); }
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_Z) == GLFW_PRESS) { camera.move_down(delta_time); }

        glm::dvec2 mouse_pos;
        glfwGetCursorPos(renderer->get_window(), &mouse_pos.x, &mouse_pos.y);

        glm::dvec2 mouse_offset = mouse_pos - prev_mouse_pos;
        prev_mouse_pos = mouse_pos;

        mouse_offset.x *= delta_time;
        mouse_offset.y *= delta_time;

        //Only move the camera using the mouse when shift is pressed
        if (glfwGetKey(renderer->get_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
            camera.update_direction(mouse_offset);
        }

    }
}

void Scene::show_controls()
{
    ImGui::Begin("Camera Controls Guide");

    // Follow Mode Toggle
    ImGui::Text("Follow Mode: %s", follow_mode ? "Enabled" : "Disabled");
    ImGui::Separator();

    // Movement Controls
    ImGui::Text("Movement Controls (When Follow Mode is Disabled):");
    ImGui::BulletText("[W] - Move Forward");
    ImGui::BulletText("[S] - Move Backward");
    ImGui::BulletText("[Q] - Move Left");
    ImGui::BulletText("[E] - Move Right");
    ImGui::BulletText("[A] - Rotate Left");
    ImGui::BulletText("[D] - Rotate Right");
    ImGui::BulletText("[SPACE] - Move Up");
    ImGui::BulletText("[Z] - Move Down");

    ImGui::Separator();

    // Mouse Controls
    ImGui::Text("Mouse Controls:");
    ImGui::BulletText("[Mouse + SHIFT] - Look Around");

    ImGui::Separator();

    // Additional Info
    ImGui::Text("Current Mouse Position:");
    ImGui::Text("X: %.2f, Y: %.2f", prev_mouse_pos.x, prev_mouse_pos.y);

    ImGui::Text("Camera Position:");
    glm::vec3 camera_pos = camera.get_position();
    ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f", camera_pos.x, camera_pos.y, camera_pos.z);

    ImGui::Text("Camera Direction:");
    glm::vec3 camera_dir = camera.get_direction();
    ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f", camera_dir.x, camera_dir.y, camera_dir.z);

    ImGui::Separator();

    ImGui::Text("Toggle Follow Mode: [TAB]");

    ImGui::End();
}
