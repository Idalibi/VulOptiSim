#include "SpatialGrid.h"

SpatialGrid::SpatialGrid(float cellSize)
    : m_cellSize(cellSize) {
}

void SpatialGrid::clear() {
    for (auto it = m_grid.begin(); it != m_grid.end(); ) {
        if (it->second.empty()) {
            it = m_grid.erase(it);      // cel was al een frame leeg: opruimen
        }
        else {
            it->second.clear();         // lijst leegmaken, maar de capaciteit blijft behouden
            ++it;
        }
    }
}

SpatialGrid::CellCoord SpatialGrid::get_cell(const glm::vec2& position) const {
    return {
        static_cast<int>(std::floor(position.x / m_cellSize)),
        static_cast<int>(std::floor(position.y / m_cellSize))
    };
}

void SpatialGrid::insert(std::size_t index, const glm::vec2& position) {
    m_grid[get_cell(position)].push_back(index);
}

const SpatialGrid::CellMap& SpatialGrid::get_cells() const {
    return m_grid;
}

