#pragma once
// SpatialGrid.h
#pragma once

#include <vector>
#include <unordered_map>
#include <utility>
#include <cmath>
#include <cstddef>
#include <glm/vec2.hpp>

// Custom hash voor std::pair<int, int> om te gebruiken als map key
struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        // Hash combinatie van de X- en Y-celindex
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

class SpatialGrid {
public:
    using CellCoord = std::pair<int, int>;
    using CellMap = std::unordered_map<CellCoord, std::vector<std::size_t>, PairHash>;

    // Constructor: celgrootte moet minstens 2x de maximale hero radius zijn
    explicit SpatialGrid(float cellSize);

    // Maakt het grid leeg aan het begin van elk frame
    void clear();

    // Voegt een hero (via index) toe aan het grid op basis van de positie
    void insert(std::size_t index, const glm::vec2& position);

    // Zet een 2D wereldpositie om naar een 2D celcoördinaat (X, Y)
    CellCoord get_cell(const glm::vec2& position) const;

    // Geeft de complete map van actieve cellen terug
    const CellMap& get_cells() const;

private:
    float m_cellSize;
    CellMap m_grid;
};