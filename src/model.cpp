#include "model.h"

#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <unordered_map>

#include "util.h"

Model::Model(const std::vector<Triangle> &triangles) {
    // default parameters
    m_LinkRestLength = 1;
    m_SpringFactor = 1;
    m_PlanarFactor = 0.1;
    m_BulgeFactor = 0.01;
    m_RadiusOfInfluence = 1;
    m_RepulsionStrength = 1;

    // find unique vertices
    std::unordered_map<glm::vec3, int> indexes;
    for (const auto &t : triangles) {
        for (const auto &v : {t.A(), t.B(), t.C()}) {
            if (indexes.find(v) == indexes.end()) {
                indexes[v] = m_Cells.size();
                m_Cells.push_back(v);
            }
        }
    }

    // create links
    m_Links.resize(m_Cells.size());
    for (const auto &t : triangles) {
        const int a = indexes[t.A()];
        const int b = indexes[t.B()];
        const int c = indexes[t.C()];
        m_Links[a].push_back(b);
        m_Links[a].push_back(c);
        m_Links[b].push_back(a);
        m_Links[b].push_back(c);
        m_Links[c].push_back(a);
        m_Links[c].push_back(b);
    }

    // make links unique
    for (int i = 0; i < m_Links.size(); i++) {
        auto &links = m_Links[i];
        std::sort(links.begin(), links.end());
        links.resize(std::distance(
            links.begin(), std::unique(links.begin(), links.end())));
    }
}

void Model::Update() {
    std::vector<glm::vec3> linkedCells;
    std::vector<glm::vec3> cells;
    cells.resize(m_Cells.size());
    for (int i = 0; i < m_Cells.size(); i++) {
        // get linked cells
        linkedCells.resize(0);
        for (const int j : m_Links[i]) {
            linkedCells.push_back(m_Cells[j]);
        }

        // get point and normal
        const glm::vec3 &P = m_Cells[i];
        const glm::vec3 N = PlaneNormalFromPoints(linkedCells);

        // accumulate
        glm::vec3 springTarget(0);
        glm::vec3 planarTarget(0);
        float bulgeDistance = 0;
        for (const glm::vec3 &L : linkedCells) {
            springTarget += L + glm::normalize(P - L) * m_LinkRestLength;
            planarTarget += L;
            const glm::vec3 D = L - P;
            if (m_LinkRestLength > glm::length(D)) {
                const float dot = glm::dot(D, N);
                bulgeDistance += std::sqrt(
                    m_LinkRestLength * m_LinkRestLength -
                    glm::dot(D, D) + dot * dot) + dot;
            }
        }

        // average
        const float m = 1 / static_cast<float>(linkedCells.size());
        springTarget *= m;
        planarTarget *= m;
        bulgeDistance *= m;
        const glm::vec3 bulgeTarget = P + bulgeDistance * N;

        // repulsion
        glm::vec3 collisionOffset(0);
        const float roi2 = m_RadiusOfInfluence * m_RadiusOfInfluence;
        for (int j = 0; j < m_Cells.size(); j++) {
            if (j == i) {
                continue;
            }
            const auto &links = m_Links[i];
            if (std::find(links.begin(), links.end(), j) != links.end()) {
                continue;
            }
            const glm::vec3 D = P - m_Cells[j];
            const float distance = glm::length(D);
            if (distance < m_RadiusOfInfluence) {
                const float d = (roi2 - distance * distance) / roi2;
                collisionOffset += glm::normalize(D) * d;
            }
        }

        cells[i] = P +
            m_SpringFactor * (springTarget - P) +
            m_PlanarFactor * (planarTarget - P) +
            m_BulgeFactor * (bulgeTarget - P) +
            m_RepulsionStrength * collisionOffset;
    }
    m_Cells = cells;
}
