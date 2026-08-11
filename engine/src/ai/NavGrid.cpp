#include "kizuri/ai/NavGrid.hpp"
#include <algorithm>
#include <queue>
#include <cmath>

namespace kizuri {

void NavGrid::Build(float originX, float originZ, float cellSize, int width, int depth) {
    m_OriginX = originX;
    m_OriginZ = originZ;
    m_CellSize = std::max(cellSize, 0.05f);
    m_Width = std::max(width, 1);
    m_Depth = std::max(depth, 1);
    m_Blocked.assign((size_t)m_Width * m_Depth, 0);
}

void NavGrid::Clear() {
    std::fill(m_Blocked.begin(), m_Blocked.end(), 0);
}

void NavGrid::SetBlocked(int x, int z, bool blocked) {
    if (!IsValidCell(x, z)) return;
    m_Blocked[CellIndex(x, z)] = blocked ? 1 : 0;
}

bool NavGrid::IsBlocked(int x, int z) const {
    if (!IsValidCell(x, z)) return true; // fora da grade = bloqueado
    return m_Blocked[CellIndex(x, z)] != 0;
}

bool NavGrid::WorldToCell(const glm::vec3& pos, int& outX, int& outZ) const {
    outX = (int)std::floor((pos.x - m_OriginX) / m_CellSize);
    outZ = (int)std::floor((pos.z - m_OriginZ) / m_CellSize);
    return IsValidCell(outX, outZ);
}

glm::vec3 NavGrid::CellToWorld(int x, int z) const {
    return { m_OriginX + ((float)x + 0.5f) * m_CellSize, 0.0f, m_OriginZ + ((float)z + 0.5f) * m_CellSize };
}

void NavGrid::RasterizeBox(const glm::vec3& center, const glm::vec3& halfExtents) {
    float minX = center.x - halfExtents.x;
    float maxX = center.x + halfExtents.x;
    float minZ = center.z - halfExtents.z;
    float maxZ = center.z + halfExtents.z;
    int x0 = (int)std::floor((minX - m_OriginX) / m_CellSize);
    int x1 = (int)std::floor((maxX - m_OriginX) / m_CellSize);
    int z0 = (int)std::floor((minZ - m_OriginZ) / m_CellSize);
    int z1 = (int)std::floor((maxZ - m_OriginZ) / m_CellSize);
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x)
            if (IsValidCell(x, z)) m_Blocked[CellIndex(x, z)] = 1;
}

// ---------------------------------------------------------------------------
// A* em grade com 8 direções (4 ortogonais + 4 diagonais), custo pela
// distância Euclidiana. Heurística = distância diagonal otimizada.
// ---------------------------------------------------------------------------
struct AStarNode {
    int x, z;
    float g, f;
    int parentX, parentZ;
    bool closed;
    bool opened;
};

std::vector<glm::vec3> NavGrid::FindPath(const glm::vec3& from, const glm::vec3& to) const {
    std::vector<glm::vec3> result;
    if (m_Width <= 0 || m_Depth <= 0 || m_Blocked.empty()) return result;

    int sx, sz, tx, tz;
    if (!WorldToCell(from, sx, sz)) return result;
    if (!WorldToCell(to, tx, tz)) return result;
    if (IsBlocked(sx, sz) || IsBlocked(tx, tz)) return result;
    if (sx == tx && sz == tz) { result.push_back(CellToWorld(tx, tz)); return result; }

    std::vector<AStarNode> nodes((size_t)m_Width * m_Depth);
    for (auto& n : nodes) { n.opened = false; n.closed = false; n.g = n.f = 1e18f; }

    // Índice linear de célula.
    auto idx = [this](int x, int z) { return (size_t)z * m_Width + x; };
    AStarNode& start = nodes[idx(sx, sz)];
    start.x = sx; start.z = sz; start.g = 0.0f;
    start.parentX = sx; start.parentZ = sz;
    float h0 = std::max(std::abs((float)sx - tx), std::abs((float)sz - tz)) * std::sqrt(2.0f);
    start.f = h0; start.opened = true;

    std::priority_queue<std::pair<float, size_t>,
        std::vector<std::pair<float, size_t>>,
        std::greater<std::pair<float, size_t>>> open;
    open.push({ start.f, idx(sx, sz) });

    static const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    static const int dz[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    static const float dc[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.4142f, 1.4142f, 1.4142f, 1.4142f };

    bool found = false;
    while (!open.empty()) {
        float f = open.top().first;
        size_t i = open.top().second;
        open.pop();
        AStarNode& cur = nodes[i];
        if (f > cur.f + 0.0001f) continue; // entrada obsoleta
        if (cur.closed) continue;
        cur.closed = true;
        if (cur.x == tx && cur.z == tz) { found = true; break; }

        for (int d = 0; d < 8; ++d) {
            int nx = cur.x + dx[d];
            int nz = cur.z + dz[d];
            if (!IsValidCell(nx, nz) || IsBlocked(nx, nz)) continue;
            // Diagonal não corta vértice de célula bloqueada (move mais limpo).
            if (d >= 4) {
                if (IsBlocked(cur.x + dx[d], cur.z) || IsBlocked(cur.x, cur.z + dz[d])) continue;
            }
            AStarNode& nb = nodes[idx(nx, nz)];
            float ng = cur.g + dc[d] * m_CellSize;
            if (ng < nb.g) {
                nb.x = nx; nb.z = nz;
                nb.g = ng;
                float h = std::max(std::abs((float)nx - tx), std::abs((float)nz - tz)) * std::sqrt(2.0f);
                nb.f = ng + h;
                nb.parentX = cur.x; nb.parentZ = cur.z;
                if (!nb.opened) { nb.opened = true; open.push({ nb.f, idx(nx, nz) }); }
                else { open.push({ nb.f, idx(nx, nz) }); }
            }
        }
    }

    if (!found) return result;

    // Reconstrói do destino até a origem.
    std::vector<std::pair<int, int>> cells;
    int cx = tx, cz = tz;
    while (!(cx == sx && cz == sz)) {
        cells.push_back({ cx, cz });
        const AStarNode& n = nodes[idx(cx, cz)];
        cx = n.parentX; cz = n.parentZ;
    }
    // Push reverso da origem (excluída) até o destino.
    for (auto it = cells.rbegin(); it != cells.rend(); ++it)
        result.push_back(CellToWorld(it->first, it->second));
    return result;
}

// Line of sight por Bresenham na grade.
bool NavGrid::HasLineOfSight(const glm::vec3& a, const glm::vec3& b) const {
    int ax, az, bx, bz;
    if (!WorldToCell(a, ax, az) || !WorldToCell(b, bx, bz)) return false;
    if (IsBlocked(ax, az) || IsBlocked(bx, bz)) return false;

    int dx = std::abs(bx - ax), dz = std::abs(bz - az);
    int sx = ax < bx ? 1 : -1;
    int sz = az < bz ? 1 : -1;
    int err = dx - dz;
    int cx = ax, cz = az;
    while (true) {
        if (IsBlocked(cx, cz)) return false;
        if (cx == bx && cz == bz) break;
        int e2 = 2 * err;
        if (e2 > -dz) { err -= dz; cx += sx; }
        if (e2 < dx) { err += dx; cz += sz; }
    }
    return true;
}

std::vector<glm::vec3> NavGrid::SmoothPath(const std::vector<glm::vec3>& path) const {
    std::vector<glm::vec3> result;
    if (path.size() <= 2) return path;
    result.push_back(path.front());
    size_t last = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        if (HasLineOfSight(path[last], path[i])) continue; // ainda vê direto — avança
        result.push_back(path[i - 1]); // waypoint anterior é virada
        last = i - 1;
    }
    result.push_back(path.back());
    return result;
}

} // namespace kizuri