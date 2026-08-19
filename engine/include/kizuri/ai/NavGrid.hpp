#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace kizuri {

class NavGrid {
public:
    NavGrid() = default;

    void Build(float originX, float originZ, float cellSize, int width, int depth);

    int GetWidth() const { return m_Width; }
    int GetDepth() const { return m_Depth; }
    float GetCellSize() const { return m_CellSize; }
    float GetOriginX() const { return m_OriginX; }
    float GetOriginZ() const { return m_OriginZ; }

    void SetBlocked(int x, int z, bool blocked);
    bool IsBlocked(int x, int z) const;
    void Clear();

    void RasterizeBox(const glm::vec3& center, const glm::vec3& halfExtents);

    bool WorldToCell(const glm::vec3& pos, int& outX, int& outZ) const;
    bool IsValidCell(int x, int z) const { return x >= 0 && z >= 0 && x < m_Width && z < m_Depth; }
    glm::vec3 CellToWorld(int x, int z) const;

    std::vector<glm::vec3> FindPath(const glm::vec3& from, const glm::vec3& to) const;

    std::vector<glm::vec3> SmoothPath(const std::vector<glm::vec3>& path) const;

    bool HasLineOfSight(const glm::vec3& a, const glm::vec3& b) const;

private:
    int CellIndex(int x, int z) const { return z * m_Width + x; }

    float m_OriginX = 0.0f, m_OriginZ = 0.0f;
    float m_CellSize = 1.0f;
    int m_Width = 0, m_Depth = 0;
    std::vector<uint8_t> m_Blocked;
};

}
