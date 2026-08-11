#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace kizuri {

// Grade de navegação (v1 dos pilares AAA): uma grade uniforme no plano XZ
// com células bloqueadas, caminho A* (8 direções) e suavização por linha de
// visão. Suficiente pra inimigos/companheiros andarem por obstáculos — a
// base do NavAgentComponent e do EnemyAIComponent.
class NavGrid {
public:
    NavGrid() = default;

    // Define a região da grade: canto (originX, originZ), tamanho da célula
    // e quantidade de células. Zera tudo (tudo livre).
    void Build(float originX, float originZ, float cellSize, int width, int depth);

    int GetWidth() const { return m_Width; }
    int GetDepth() const { return m_Depth; }
    float GetCellSize() const { return m_CellSize; }
    float GetOriginX() const { return m_OriginX; }
    float GetOriginZ() const { return m_OriginZ; }

    void SetBlocked(int x, int z, bool blocked);
    bool IsBlocked(int x, int z) const;
    void Clear();

    // Marca as células tocadas por uma caixa alinhada (AABB) no plano XZ.
    void RasterizeBox(const glm::vec3& center, const glm::vec3& halfExtents);

    bool WorldToCell(const glm::vec3& pos, int& outX, int& outZ) const;
    bool IsValidCell(int x, int z) const { return x >= 0 && z >= 0 && x < m_Width && z < m_Depth; }
    glm::vec3 CellToWorld(int x, int z) const; // centro da célula (y=0)

    // Caminho de 'from' até 'to' (pontos de mundo no plano XZ, começando no
    // centro da célula de 'to' e terminando no centro da célula de destino),
    // vazio se não houver caminho ou se from/to estiverem fora/em célula
    // bloqueada. Cada ponto tem y=0 — o agente decide a altura.
    std::vector<glm::vec3> FindPath(const glm::vec3& from, const glm::vec3& to) const;

    // Simplifica o caminho removendo waypoints intermediários com linha de
    // visão livre (Bresenham na grade) — caminho mais reto e natural.
    std::vector<glm::vec3> SmoothPath(const std::vector<glm::vec3>& path) const;

    // Linha de visão 2D entre dois pontos do mundo, na grade.
    bool HasLineOfSight(const glm::vec3& a, const glm::vec3& b) const;

private:
    int CellIndex(int x, int z) const { return z * m_Width + x; }

    float m_OriginX = 0.0f, m_OriginZ = 0.0f;
    float m_CellSize = 1.0f;
    int m_Width = 0, m_Depth = 0;
    std::vector<uint8_t> m_Blocked; // 0 = livre, 1 = bloqueada
};

} // namespace kizuri
