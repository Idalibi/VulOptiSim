#include "pch.h"
#include "scene.h"
#include "SpatialGrid.h" 
#include <algorithm>     
#include <array>
#include <cstdio>

Scene::Scene(vulvox::Renderer& renderer) : renderer(&renderer) //Constructor
{
    glfwGetCursorPos(this->renderer->get_window(), &prev_mouse_pos.x, &prev_mouse_pos.y);

    //Definieer de plek waar de camera moet komen
    glm::vec3 camera_pos{ -28.2815380f, 305.485260f, -30.0800228f };
    glm::vec3 camera_up{ 0.338442326f, 0.869414926f, 0.359964609f };
    glm::vec3 camera_direction{ 0.595541596f, -0.494082689f, 0.633413374f };

    //Maak de camera aan
    camera = Camera(camera_pos, camera_up, camera_direction, 100.f, 100.f);

    //Maak terein aan (check terrain path)
    terrain = Terrain(TERRAIN_PATH);

    load_models_and_textures(); //Check van deze 3 functies waar ze hun info vandaan halen en hoe want dezen zijn traag)

    spawn_heroes(); //deze moet een ander sorteelalgoritme krijgen

    spawn_staves(); //deze zorgt nu voor een hoop elende

    std::cout << "Scene loaded." << std::endl;
}

void Scene::load_models_and_textures() const
{
    //Load all the models and textures we're going to need into GPU memory

    //Terrain textures
    std::vector<std::filesystem::path> texture_paths{
        CUBE_SEA_TEXTURE_PATH,  //Sea
        CUBE_GRASS_FLOWER_TEXTURE_PATH, //Lab floor
        CUBE_CONCRETE_WALL_TEXTURE_PATH, //Lab walls
        CUBE_MOSS_TEXTURE_PATH }; //Floor
    renderer->load_texture_array("texture_array_test", texture_paths);

    //NPCs (wat is dit Gert? en waarom staat dit op commentaar?) Ik kon ze niet vinden in de simulatie
    //renderer->load_model("konata", MODEL_PATH);
    //renderer->load_texture("konata", KONATA_MODAL_TEXTURE_PATH);

    renderer->load_model("frieren-blob", FRIEREN_PATH); //Path finding algoritme denk ik controlleren en verbeteren (de anderen ook)
    std::vector<std::filesystem::path> frieren_texture{ FRIEREN_TEXTURE_PATH };
    renderer->load_texture_array("frieren-blob-array", frieren_texture);

    renderer->load_model("staff", STAFF_PATH);
    renderer->load_texture("staff", STAFF_TEXTURE_PATH);

    renderer->load_model("cube", CUBE_MODEL_PATH);
    //renderer->load_texture("cube", CUBE_SEA_TEXTURE_PATH); Lijkt dubbel en onzichtbaar

    //Effects
    std::vector<std::filesystem::path> shield_path{ SHIELD_TEXTURE_PATH };
    renderer->load_texture_array("shield", shield_path);

    renderer->load_texture_array("lightning", LIGHTNING_TEXTURE_PATHS);

    renderer->load_texture_array("fireball", FIREBALL_TEXTURE_PATHS);
}

void Scene::spawn_heroes() //Deze is eenmalig
{
    Transform hero_transform;
    hero_transform.rotation = glm::quatLookAt(glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 1.f, 0.f));
    hero_transform.scale = glm::vec3(1.f);

    float spawn_offset = terrain.tile_width / 3.f;

    int start_areas = 10;
    float start_area_tile_offset = 12.f;
    float spawn_start_y = terrain.tile_width * 3.f;

    float start_corner_y = 9.f * terrain.tile_width;

    std::cout << "Spawning characters and calculating routes...(met een threadpool hopelijk)" << std::endl;

    //initialiseer de threadpool
    unsigned int num_threads = std::thread::hardware_concurrency();
    ThreadPool pool(num_threads > 0 ? num_threads : 4);

    //wederom lokale hulp struct
    struct SpawnedHeroInfo {
        std::string mesh1, mesh2;
        Transform transform;
        std::string name;
        float speed;
        std::vector<glm::vec2> route;
    };

    //opslaan zodat we wachten tot alle groepen klaar zijn
    std::vector<std::future<std::vector<SpawnedHeroInfo>>> futures;
    futures.reserve(start_areas);

    //Verdeel de 10 startgebieden over de threadpool
    for (int s = 0; s < start_areas; s++) {

        futures.push_back(pool.enqueue([=, this]() {
            std::vector<SpawnedHeroInfo> local_heroes;
            local_heroes.reserve(30 * 30);

            float start_area_offset = static_cast<float>(s) * start_area_tile_offset * terrain.tile_width;
            int local_spawn_count = s * 900; // voor unieke namen te behouden

            for (int i = 0; i < 30; i++) //O(30) O(start_area * 30)
            {
                for (int j = 0; j < 30; j++) //O(30) O start_area * 30 * 30) = O(start_area * 900)
                {
                    float x = start_corner_y + start_area_offset + ((float)i * spawn_offset);
                    float z = spawn_start_y + ((float)j * spawn_offset);
                    float y = terrain.get_height(glm::vec2(x, z)); //Wel een zware functie denk ik (path finding)

                    Transform local_transform = hero_transform;
                    local_transform.position = glm::vec3(x, y, z);

                    local_spawn_count++;


                    //Dit algoritme moet beter. deze is taai
                    auto r = terrain.find_route(
                        glm::vec2(x, z), 
                        glm::vec2(69 * terrain.tile_width, 160 * terrain.tile_width)
                    ); //Path finding (deze 9000 keer is wel een hele hoop. wellicht is het per spatial grid handiger of per groep, iets in die zin)
                    local_heroes.push_back({ "frieren-blob", "frieren-blob", local_transform, "Frieren" + std::to_string(local_spawn_count), 20.f,r });

                }
            }
            return local_heroes;
        }));
    }

    int total_spawn_count = 0;
    for (auto& f : futures) {
        std::vector<SpawnedHeroInfo> area_heroes = f.get();
        for (const auto& h : area_heroes) {
            // GEBRUIK HIER h.transform IN PLAATS VAN DE ALGEMENE hero_transform!
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

size_t Scene::get_character_count() const
{
    return heroes.size();
}

size_t Scene::get_staff_count() const
{
    return staves.size();

}

void Scene::update(const float delta_time)
{
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

    //Make heroes collide with each other (Spatial Grid, half-stencil)
    static SpatialGrid spatial_grid(5.0f); // LET OP: pas 40.0f aan naar ~2x de radius van je hero //20 maakte het een heel stuk sneller op begin (45 fps eerst wtff) (later werd het + - 25) (radius is 0.5f;)
    spatial_grid.clear();

    //Posities en radii één keer per frame in compacte arrays (cache-vriendelijk)
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

    //Los één botsend paar op
    auto resolve_pair = [&](size_t i, size_t j)
        {
            const glm::vec2 direction = positions[j] - positions[i];
            const float rad_sum = radii[i] + radii[j];

            // Snelle afstand-check zonder wortel
            const float dist_sq = glm::dot(direction, direction);
            if (dist_sq >= rad_sum * rad_sum) return;

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

            heroes[i].push(-norm_direction, overlap * 0.5f);
            heroes[j].push(norm_direction, overlap * 0.5f);
        };

    //Alleen 4 van de 8 buren: elk celpaar wordt zo precies één keer bezocht
    static const std::array<std::pair<int, int>, 4> half_neighbors{ { {1, 0}, {-1, 1}, {0, 1}, {1, 1} } };

    const auto& cells = spatial_grid.get_cells();
    for (const auto& [cell_coord, cell_hero_indices] : cells)
    {
        //Paren binnen dezelfde cel
        for (size_t a = 0; a < cell_hero_indices.size(); ++a)
        {
            for (size_t b = a + 1; b < cell_hero_indices.size(); ++b)
            {
                resolve_pair(cell_hero_indices[a], cell_hero_indices[b]);
            }
        }

        //Paren met de 4 "voorwaartse" buurcellen
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

    static HeroPositions hero_positions;
    hero_positions.positions.resize(heroes.size());
    hero_positions.max_radius = 0.f;

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        heroes[i].update(delta_time, terrain);

        // Het object zit nu toch al in de cache, dus dit kost bijna niets extra
        hero_positions.positions[i] = heroes[i].get_position();
        hero_positions.max_radius = std::max(hero_positions.max_radius, heroes[i].get_collision_radius());
    }

    shield = Shield{ "shield", heroes };

    for (auto& staff : staves)
    {
        staff.update(delta_time, heroes, active_lightning, projectiles);
    }

    for (auto& lightning : active_lightning)
    {
        lightning.update(delta_time, camera, heroes, hero_positions);   // NIEUW: extra argument
    }

    //Remove inactive lightning
    const auto [first_l, last_l] = std::ranges::remove_if(active_lightning, [](const Lightning& l) { return !l.is_active(); });
    active_lightning.erase(first_l, last_l);

    for (auto& projectile : projectiles)
    {
        projectile.update(delta_time, camera, shield, heroes, hero_positions);   // NIEUW: extra argument
    }

    //Remove inactive projectiles
    const auto [first_p, last_p] = std::ranges::remove_if(projectiles, [](const Projectile& p) { return !p.is_active(); });
    projectiles.erase(first_p, last_p);
}

void Scene::draw()
{
    //On using concurrency here:
    //  The graphics library copies the data to GPU memory so you can change the data after the draw functions of the renderer return.
    //  The actual drawing runs parallel to the host (CPU) execution.
    //  Make sure the data needed for drawing (position etc.) is ready before calling the corresponding draw functions or weird things happen.
    //  Calling draw functions outside of this functions lifetime will crash the program!

    ///Nieuw
    static std::vector<glm::mat4> hero_matrices;
    static std::vector<uint32_t> hero_texture_indices; // zelfde type als texture_indices in terrain.h
    hero_matrices.clear();
    hero_texture_indices.clear();

    for (const auto& hero : heroes)
    {
        if (!hero.is_active()) continue;
        hero_matrices.push_back(hero.get_matrix());
        hero_texture_indices.push_back(0);
    }

    renderer->draw_instanced_with_texture_array("frieren-blob", "frieren-blob-array", hero_matrices, hero_texture_indices);
    //Nieuw

    terrain.draw(renderer);

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



    show_health_values();
    show_mana_values();

    Log::get_instance()->draw("Log");

    show_controls();
}

namespace
{
    constexpr int    MAX_STAT = 1000;       // Health en mana lopen van 0 t/m 1000
    constexpr double REFRESH_INTERVAL = 0.25; // Hoe vaak (in seconden) de lijst opnieuw gesorteerd wordt

    struct StatCache
    {
        std::vector<int> sorted;
        double last_update = -1.0;
    };

    // Counting sort op een vaste range: O(n + 1000), geen vergelijkingen nodig.
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




void quick_sort_recursive(std::vector<int>& arr, int low, int high)
{
    if (low >= high) return;

    // Kies het midden als pivot (voorkomt O(N^2) bij al gesorteerde data)
    int pivot = arr[low + (high - low) / 2];
    int i = low;
    int j = high;

    while (i <= j)
    {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;

        if (i <= j)
        {
            std::swap(arr[i], arr[j]);
            i++;
            j--;
        }
    }

    if (low < j) quick_sort_recursive(arr, low, j);
    if (i < high) quick_sort_recursive(arr, i, high);
}

std::vector<int> Scene::sort(const std::vector<int>& to_sort) const //Een sorteer algoritme, deze kan waarschijnlijk beter/geschikter BigO = O(n^2)
{   
    if (to_sort.empty()) return to_sort;

    std::string sorteeralgoritme = "counting";
    std::vector<int> sorted_list = to_sort;

    if (sorteeralgoritme == "insert") {
        //Orginele sorteeralgoritme: Insertion Sort
        //summary
        //  Dit algoritme pakt een element, checkt alle volgende elementen tot het element erna groter is dan het element dat het vast pakt en plaatst het ervoor.
        //summary

        for (size_t i = 0; i < sorted_list.size(); i++)
        {
            int current_value = sorted_list.at(i);

            //For all values before the current index,
            //move all bigger values than current value one index forward
            size_t j = i;
            for (; j > 0 && sorted_list.at(j - 1) > current_value; j--)
            {
                sorted_list.at(j) = sorted_list.at(j - 1);
            }
            //Place the current value in the created gap
            sorted_list.at(j) = current_value;
        }

        return sorted_list;
    }
    else if (sorteeralgoritme == "quick") {
        //summary
        // Quick Sort of ... is een sorteeralgoritme dat een willekeurig (midden)punt pakt (pivot) 
        // en de hogere elementen aan de ene kant plaatst en de lagere aan de andere kant.
        // Deze is het snelst als de set al een beetje op volgorde is en het traagst als dat helemaal niet zo is.
        // Ook is dit algorite recursief wat het dus trager maakt dan bijvoorbeeld lineaire algoritmen.
        //summary
        quick_sort_recursive(sorted_list, 0, static_cast<int>(sorted_list.size()) - 1);
        return sorted_list;
    }
    else if (sorteeralgoritme == "counting") {
        //summary
        // Counting Sort is een algoritme dat niet de waarden vergelijkt met elkaar maar door te tellen hoe vaak elke waarde voorkomt.
        // Deze is lineair wat het wel een stuk sneller maakt dan vele anderen.
        //summary

        // Zoek de kleinste en grootste waarde in de lijst
        int min_val = to_sort[0]; //Minimum
        int max_val = to_sort[0]; //Maximum
        for (int val : to_sort)
        {
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }

        // Maak een frequentietabel (hoe vaak komt elke HP/Mana waarde voor?)
        int range = max_val - min_val + 1; //hoeveel verschillende waarden komen er voor
        std::vector<int> count(range, 0); //tel dezen

        for (int val : to_sort) //voor iedere waarde(val) in de ongesorteerde lijst
        {
            count[val - min_val]++; //tel 1 keer een unieke waarde
        }

        // Bouw de gesorteerde lijst op
        //std::vector<int> sorted_list; //definieer nieuwe lijst
        sorted_list.clear();
        sorted_list.reserve(to_sort.size()); //De grootte van de lijst staat gelijk aan de aantal elementen van to_sort

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
