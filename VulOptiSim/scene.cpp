#include "pch.h"
#include "scene.h"
#include "SpatialGrid.h" 
#include <algorithm>     

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

    //NPCs (wat is dit Gert? en waarom staat dit op commentaar?)
    //renderer->load_model("konata", MODEL_PATH);
    //renderer->load_texture("konata", KONATA_MODAL_TEXTURE_PATH);

    renderer->load_model("frieren-blob", FRIEREN_PATH); //Path finding algoritme denk ik controlleren en verbeteren (de anderen ook)
    renderer->load_texture("frieren-blob", FRIEREN_TEXTURE_PATH);

    renderer->load_model("staff", STAFF_PATH);
    renderer->load_texture("staff", STAFF_TEXTURE_PATH);

    renderer->load_model("cube", CUBE_MODEL_PATH);
    renderer->load_texture("cube", CUBE_SEA_TEXTURE_PATH);

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

    std::cout << "Spawning characters and calculating routes..." << std::endl;

    int spawn_count = 0;
    for (int s = 0; s < start_areas; s++) //BigO : O(9000) oftewel O(1)
    {
        float start_area_offset = static_cast<float>(s) * start_area_tile_offset * terrain.tile_width; //O(start_areas) = 10 (draait dus start areas hoeveelheid keer)

        for (int i = 0; i < 30; i++) //O(30) O(start_area * 30)
        {
            for (int j = 0; j < 30; j++) //O(30) O start_area * 30 * 30) = O(start_area * 900)
            {
                float x = start_corner_y + start_area_offset + ((float)i * spawn_offset);
                float z = spawn_start_y + ((float)j * spawn_offset);
                float y = terrain.get_height(glm::vec2(x, z)); //Wel een zware functie denk ik (path finding)

                hero_transform.position = glm::vec3(x, y, z);

                spawn_count++;

                heroes.emplace_back("frieren-blob", "frieren-blob", hero_transform, "Frieren" + std::to_string(spawn_count), 20.f);

                //Dit algoritme moet beter. deze is taai
                auto r = terrain.find_route(glm::uvec2(x, z), glm::uvec2(69 * terrain.tile_width, 160 * terrain.tile_width)); //Path finding (deze 9000 keer is wel een hele hoop. wellicht is het per spatial grid handiger of per groep, iets in die zin)
                heroes.back().set_route(r);
            }
        }
    }

    Log::get_instance()->add_log("Spawned %d characters.\n", spawn_count);
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

    //Make heroes collide with each other (Nu geoptimaliseerd met Spatial Grid)
    static SpatialGrid spatial_grid(40.0f); // LET OP: pas 40.0f aan naar ~2x de radius van je hero
    spatial_grid.clear();

    //Vul het grid
    for (size_t i = 0; i < heroes.size(); ++i)
    {
        if (heroes[i].is_active())
        {
            spatial_grid.insert(i, heroes[i].get_position2d());
        }
    }

    //Handel botsingen af per cel
    for (const auto& [cell_coord, cell_hero_indices] : spatial_grid.get_cells())
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                SpatialGrid::CellCoord neighbor_coord = { cell_coord.first + dx, cell_coord.second + dy };

                auto neighbor_it = spatial_grid.get_cells().find(neighbor_coord);
                if (neighbor_it == spatial_grid.get_cells().end()) continue;

                const auto& neighbor_hero_indices = neighbor_it->second;

                for (size_t i : cell_hero_indices)
                {
                    const glm::vec2 pos_i = heroes[i].get_position2d();
                    const float rad_i = heroes[i].get_collision_radius();

                    for (size_t j : neighbor_hero_indices)
                    {
                        if (j <= i) continue; // Voorkom dubbele check en check met zichzelf

                        const glm::vec2 pos_j = heroes[j].get_position2d();
                        const float rad_j = heroes[j].get_collision_radius();

                        const glm::vec2 direction = pos_j - pos_i;
                        const float rad_sum = rad_i + rad_j;

                        // Snelle afstand-check zonder wortel
                        const float dist_sq = glm::dot(direction, direction);
                        if (dist_sq >= rad_sum * rad_sum) continue;

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
                    }
                }
            }
        }
    }

    for (auto& hero : heroes)
    {
        hero.update(delta_time, terrain);
    }

    shield = Shield{ "shield", heroes };

    for (auto& staff : staves)
    {
        staff.update(delta_time, heroes, active_lightning, projectiles);
    }

    for (auto& lightning : active_lightning)
    {
        lightning.update(delta_time, camera, heroes);
    }

    //Remove inactive lightning
    const auto [first_l, last_l] = std::ranges::remove_if(active_lightning, [](const Lightning& l) { return !l.is_active(); });
    active_lightning.erase(first_l, last_l);

    for (auto& projectile : projectiles)
    {
        projectile.update(delta_time, camera, shield, heroes);
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

    for (const auto& hero : heroes)
    {
        hero.draw(renderer);
    }

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

/// <summary>
/// Sorts all health values and displays them in a window.
/// </summary>
void Scene::show_health_values() const
{
    std::vector<int> health_values;
    for (const auto& h : heroes)
    {
        health_values.push_back(h.get_health());
    }

    health_values = sort(health_values);

    ImGui::Begin("Heroes Health Bars");

    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.90f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.f, 0.5f, 0.f, 1.0f }); //Green
    for (const int& hp : health_values)
    {
        std::stringstream hp_text;
        hp_text << hp << "/" << 1000;
        ImGui::ProgressBar((float)hp / 1000, ImVec2(-FLT_MIN, 0.0f), hp_text.str().c_str());
    }
    ImGui::PopStyleColor(1);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    ImGui::End();
}

/// <summary>
/// Sorts all mana values and displays them in a window.
/// </summary>
void Scene::show_mana_values() const
{
    std::vector<int> mana_values;
    for (const auto& s : heroes)
    {
        mana_values.push_back(s.get_mana());
    }

    mana_values = sort(mana_values);

    ImGui::Begin("Heroes Mana Bars");

    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.90f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.f, 0.f, 0.5f, 1.0f }); //Blue
    for (const int& mana : mana_values)
    {
        std::stringstream mana_text;
        mana_text << mana << "/" << 1000;
        ImGui::ProgressBar((float)mana / 1000, ImVec2(-FLT_MIN, 0.0f), mana_text.str().c_str());
    }
    ImGui::PopStyleColor(1);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    ImGui::End();
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
