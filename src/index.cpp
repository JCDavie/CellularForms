#include "index.h"

#include "util.h"

#define DEBUG_INDEX 0

// TODO: grow-able index
Index::Index(const float cellSize) :
    m_CellSize(cellSize),
    m_Start(-250/4, -250/4, -250/4),
    m_Size(500/4, 500/4, 500/4),
    m_Cells(m_Size.x * m_Size.y * m_Size.z),
    m_Locks(1024)
{
}

glm::ivec3 Index::KeyForPoint(const glm::vec3 &point) const {
    const int x = std::roundf(point.x / m_CellSize);
    const int y = std::roundf(point.y / m_CellSize);
    const int z = std::roundf(point.z / m_CellSize);
    return glm::ivec3(x, y, z);
}

int Index::IndexForKey(const glm::ivec3 &key) const {
    const auto d = key - m_Start;
    return d.x + (d.y * m_Size.x) + (d.z * m_Size.x * m_Size.y);
}

const std::vector<int> &Index::Nearby(const glm::vec3 &point) const {
    return m_Cells[IndexForKey(KeyForPoint(point))];
}

void Index::Add(const glm::vec3 &point, const int id) {
    const glm::ivec3 key = KeyForPoint(point);
    const auto k0 = key - 1;
    const auto k1 = key + 1;
    for (int x = k0.x; x <= k1.x; x++) {
        for (int y = k0.y; y <= k1.y; y++) {
            for (int z = k0.z; z <= k1.z; z++) {
                auto &ids = m_Cells[IndexForKey(glm::ivec3(x, y, z))];
                #if DEBUG_INDEX
                    const auto it = std::find(ids.begin(), ids.end(), id);
                    if (it != ids.end()) {
                        Panic("id already present in Add");
                    }
                #endif
                ids.push_back(id);
            }
        }
    }
}

void Index::Remove(const glm::vec3 &point, const int id) {
    const glm::ivec3 key = KeyForPoint(point);
    const auto k0 = key - 1;
    const auto k1 = key + 1;
    for (int x = k0.x; x <= k1.x; x++) {
        for (int y = k0.y; y <= k1.y; y++) {
            for (int z = k0.z; z <= k1.z; z++) {
                auto &ids = m_Cells[IndexForKey(glm::ivec3(x, y, z))];
                const auto it = std::find(ids.begin(), ids.end(), id);
                #if DEBUG_INDEX
                    if (it == ids.end()) {
                        Panic("id not found in Remove");
                    }
                #endif
                std::swap(*it, ids.back());
                ids.pop_back();
            }
        }
    }
}

bool Index::Update(const glm::vec3 &p0, const glm::vec3 &p1, const int id) {
    const auto key0 = KeyForPoint(p0);
    const auto key1 = KeyForPoint(p1);

    if (key0 == key1) {
        return false;
    }

    const auto k00 = key0 - 1;
    const auto k01 = key0 + 1;
    const auto k10 = key1 - 1;
    const auto k11 = key1 + 1;

    const auto in0 = [&k00, &k01](const int x, const int y, const int z) {
        return
            x >= k00.x && x <= k01.x &&
            y >= k00.y && y <= k01.y &&
            z >= k00.z && z <= k01.z;
    };

    const auto in1 = [&k10, &k11](const int x, const int y, const int z) {
        return
            x >= k10.x && x <= k11.x &&
            y >= k10.y && y <= k11.y &&
            z >= k10.z && z <= k11.z;
    };

    // remove if in key0 and not in key1
    for (int x = k00.x; x <= k01.x; x++) {
        for (int y = k00.y; y <= k01.y; y++) {
            for (int z = k00.z; z <= k01.z; z++) {
                if (in1(x, y, z)) {
                    continue;
                }
                auto &ids = m_Cells[IndexForKey(glm::ivec3(x, y, z))];
                std::lock_guard<std::mutex> guard(
                    m_Locks[(x + y + z) % m_Locks.size()]);
                const auto it = std::find(ids.begin(), ids.end(), id);
                #if DEBUG_INDEX
                    if (it == ids.end()) {
                        Panic("id not found in Remove");
                    }
                #endif
                std::swap(*it, ids.back());
                ids.pop_back();
            }
        }
    }

    // add if in key1 and not in key0
    for (int x = k10.x; x <= k11.x; x++) {
        for (int y = k10.y; y <= k11.y; y++) {
            for (int z = k10.z; z <= k11.z; z++) {
                if (in0(x, y, z)) {
                    continue;
                }
                auto &ids = m_Cells[IndexForKey(glm::ivec3(x, y, z))];
                std::lock_guard<std::mutex> guard(
                    m_Locks[(x + y + z) % m_Locks.size()]);
                #if DEBUG_INDEX
                    const auto it = std::find(ids.begin(), ids.end(), id);
                    if (it != ids.end()) {
                        Panic("id already present in Add");
                    }
                #endif
                ids.push_back(id);
            }
        }
    }

    return true;
}
