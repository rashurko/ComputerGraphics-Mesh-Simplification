#pragma once

#include <algorithm>
#include <array>
#include <tuple>
#include <queue>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

struct TopologyVertex {
    int id = -1;
    glm::vec3 position = glm::vec3(0.0f);
    glm::mat4 Q{0.0f}; // For quadratic error 
    float K; // Approximation of the Gaussian curvature
    bool active = true;
};

struct TopologyVertexNormal {
    int vertexId = -1;
    glm::vec3 normal = glm::vec3(0.0f);
};

struct TopologyFace {
    int id = -1;
    std::array<int, 3> v = { -1, -1, -1 };
    bool active = true;
};

struct TopologyFaceNormal {
    int faceId = -1;
    glm::vec3 normal = glm::vec3(0.0f);
};

struct TopologyEdge {
    int v0 = -1;
    int v1 = -1;
    std::vector<int> faceIds;
};

struct TopologyRenderVertex {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    glm::vec2 texCoord = glm::vec2(0.0f);
};

struct TopologyRenderData {
    std::vector<TopologyRenderVertex> vertices;
    std::vector<unsigned int> indices;
};

class TopologyMesh {
public:
    TopologyMesh() = default;

    void clear() {
        vertices.clear();
        vertexNormals.clear();
        faces.clear();
        faceNormals.clear();
        edges.clear();
        vertexToFaces.clear();
        edgeToLength =  std::priority_queue<std::pair<float, std::pair<int, int>>>();
        edgeToNeighbors.clear();

        edgeToQuadraticError = std::priority_queue<
        std::pair<float, std::pair<int, int>>, 
        std::vector<std::pair<float, std::pair<int, int>>>, 
        std::greater<std::pair<float, std::pair<int, int>>>
        >();
        edgeToPos.clear();
        edgeToCurrentError.clear();

        gaussianCurvatureEnabled = false;
    }

    int addVertex(const glm::vec3& position) {
        const int id = static_cast<int>(vertices.size());
        vertices.push_back({ id, position, glm::mat4(0.0f), true });
        vertexToFaces.emplace_back();
        return id;
    }

    int addFace(int a, int b, int c) {
        const int id = static_cast<int>(faces.size());
        faces.push_back({ id, { a, b, c }, true });
        return id;
    }

    void buildAdjacency() {
        vertexToFaces.assign(vertices.size(), {});

        for (TopologyFace& face : faces) {
            if (!face.active) {
                continue;
            }

            if (!isValidVertexId(face.v[0]) || !isValidVertexId(face.v[1]) || !isValidVertexId(face.v[2])) {
                face.active = false;
                continue;
            }

            if (!vertices[face.v[0]].active || !vertices[face.v[1]].active || !vertices[face.v[2]].active) {
                face.active = false;
                continue;
            }

            if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
                face.active = false;
                continue;
            }

            for (int vid : face.v) {
                vertexToFaces[vid].push_back(face.id);
            }

            addEdgeFace(face.v[0], face.v[1], face.id);
            addEdgeFace(face.v[1], face.v[2], face.id);
            addEdgeFace(face.v[2], face.v[0], face.id);
        }     
    }

    bool isVertexActive(int vid) const {
        return isValidVertexId(vid) && vertices[vid].active;
    }

    bool isFaceActive(int fid) const {
        return fid >= 0 && fid < static_cast<int>(faces.size()) && faces[fid].active;
    }

    int activeVertexCount() const {
        int count = 0;
        for (const TopologyVertex& vertex : vertices) {
            if (vertex.active) {
                count++;
            }
        }
        return count;
    }

    int activeFaceCount() const {
        int count = 0;
        for (const TopologyFace& face : faces) {
            if (face.active) {
                count++;
            }
        }
        return count;
    }

    int activeEdgeCount() const {
        return edges.size();
    }

    const std::unordered_map<std::int64_t, TopologyEdge>& getActiveEdges() const {
        return edges;
    }

    std::vector<int> getVertexNeighbors(int vid) const {
        
        if (!isVertexActive(vid)) {
            return {};
        }

        std::vector<int> neighbors;
        neighbors.reserve(16);

        for (int fid : getIncidentFaces(vid)) {
            const TopologyFace& face = faces[fid];
            for (int other : face.v) {
                if (other != vid && isVertexActive(other)) {
                    neighbors.push_back(other);
                }
            }
        }

        // Remove duplicates
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());

        return neighbors;
    }

    std::vector<int> getIncidentFaces(int vid) const {
        if (!isVertexActive(vid) || vid >= static_cast<int>(vertexToFaces.size())) {
            return {};
        }

        std::vector<int> result;
        result.reserve(vertexToFaces[vid].size());
        for (int fid : vertexToFaces[vid]) {
            if (isFaceActive(fid)) {
                result.push_back(fid);
            }
        }
        return result;
    }

    bool hasEdge(int a, int b) const {
        if (a == b || !isVertexActive(a) || !isVertexActive(b)) {
            return false;
        }
        return edges.find(edgeKey(a, b)) != edges.end();
    }

    bool isBoundaryEdge(int a, int b) const {
        const auto it = edges.find(edgeKey(a, b));
        if (it == edges.end()) {
            return false;
        }
        return it->second.faceIds.size() == 1;
    }

    bool isLegalCollapse(int keepVid, int removeVid) const {
        if (keepVid == removeVid) {
            return false;
        }
        if (!isVertexActive(keepVid) || !isVertexActive(removeVid)) {
            return false;
        }
        if (!hasEdge(keepVid, removeVid)) {
            return false;
        }

        const auto sharedFaceIds = getSharedFaces(keepVid, removeVid);
        // Valid edge has 2 shared faces
        if (sharedFaceIds.size() != 2) {
            return false;
        }

        std::unordered_set<int> keepNeighbors;
        for (int neighbor : getVertexNeighbors(keepVid)) {
            keepNeighbors.insert(neighbor);
        }

        int sharedNeighborCount = 0;
        for (int neighbor : getVertexNeighbors(removeVid)) {
            if (neighbor != keepVid && keepNeighbors.count(neighbor) > 0) {
                sharedNeighborCount++;
            }
        }

        // Check shared neighbor count, if it's more than the shared face count, it means we will create non-manifold edge after collapse
        if (sharedNeighborCount > static_cast<int>(sharedFaceIds.size())) {
            return false;
        }

        for (int fid : getIncidentFaces(removeVid)) {
            // Skip shared faces that will be removed after collapse
            if (std::find(sharedFaceIds.begin(), sharedFaceIds.end(), fid) != sharedFaceIds.end()) {
                continue;
            }

            const TopologyFace& face = faces[fid];
            int a = face.v[0];
            int b = face.v[1];
            int c = face.v[2];

            if (a == removeVid) {
                a = keepVid;
            }
            if (b == removeVid) {
                b = keepVid;
            }
            if (c == removeVid) {
                c = keepVid;
            }

            if (a == b || b == c || a == c) {
                return false;
            }

            const glm::vec3& p0 = vertices[a].position;
            const glm::vec3& p1 = vertices[b].position;
            const glm::vec3& p2 = vertices[c].position;
            const glm::vec3& oldP0 = vertices[face.v[0]].position;
            const glm::vec3& oldP1 = vertices[face.v[1]].position;
            const glm::vec3& oldP2 = vertices[face.v[2]].position;
            const glm::vec3 cross = glm::cross(p1 - p0, p2 - p0);
            // Check if the triangle is degenerate (can be removed?)
            if (glm::length(cross) < 1e-8f) {
                return false;
            }

            const glm::vec3 oldCross = glm::cross(oldP1 - oldP0, oldP2 - oldP0);
            const float oldLength = glm::length(oldCross);
            const float newLength = glm::length(cross);
            // Check if the old triangle or new triangle is degenerate
            if (oldLength < 1e-8f || newLength < 1e-8f) {
                return false;
            }

            const glm::vec3 oldNormal = oldCross / oldLength;
            const glm::vec3 newNormal = cross / newLength;
            // Check if the triangle will flip after collapse
            if (glm::dot(oldNormal, newNormal) <= 0.0f) {
                return false;
            }
        }

        return true;
    }

    bool collapseEdge(int keepVid, int removeVid, const glm::vec3& newPos) {
        if (!isVertexActive(keepVid) || !isVertexActive(removeVid) || keepVid == removeVid) {
            return false;
        }

        vertices[keepVid].position = newPos;
        vertices[keepVid].Q += vertices[removeVid].Q;

        std::vector<int> incidentFaces = vertexToFaces[removeVid];

        for (const int faceid : incidentFaces) {
            TopologyFace& face = faces[faceid];
            if (!face.active) {
                continue;
            }

            if (face.v[0] == keepVid || face.v[1] == keepVid || face.v[2] == keepVid) {
                face.active = false;
                for (int vid : face.v) {
                    auto &vFaces = vertexToFaces[vid];
                    vFaces.erase(std::remove(vFaces.begin(), vFaces.end(), face.id), vFaces.end());
                    removeEdgeFace(vid, keepVid, face.id);
                }
                continue;
            }

            // Rewire the face indices
            for (int i = 0; i < 3; i++) {
                if (face.v[i] == removeVid) {
                    face.v[i] = keepVid;
                } else {
                    removeEdge(face.v[i], removeVid);
                }
            }

            vertexToFaces[keepVid].push_back(faceid);

            removeEdge(keepVid, removeVid);

            addEdgeFace(face.v[0], face.v[1], face.id);
            addEdgeFace(face.v[1], face.v[2], face.id);
            addEdgeFace(face.v[2], face.v[0], face.id);
        }

        vertices[removeVid].active = false;
        vertexToFaces[removeVid].clear();

        return true;
    }

    TopologyRenderData toRenderData() {
        TopologyRenderData renderData;
        if (vertexNormals.size() != vertices.size()) {
            recomputeVertexNormals();
            std::cout << "first pass" << std::endl;
        }

        for (const TopologyFace& face : faces) {
            if (!face.active) {
                continue;
            }

            for (int localIndex = 0; localIndex < 3; ++localIndex) {
                const int vid = face.v[localIndex];
                TopologyRenderVertex renderVertex;
                renderVertex.position = vertices[vid].position;
                renderVertex.normal = vertexNormals[vid].normal;
                renderVertex.texCoord = glm::vec2(0.0f);

                renderData.vertices.push_back(renderVertex);
                renderData.indices.push_back(static_cast<unsigned int>(renderData.indices.size()));
            }
        }

        return renderData;
    }

    void recomputeFaceNormals() {
        faceNormals.assign(faces.size(), { -1, glm::vec3(0.0f)});
        for (TopologyFace &face : faces) {
            if (!face.active) {
                continue;
            }

            const glm::vec3& p0 = vertices[face.v[0]].position;
            const glm::vec3& p1 = vertices[face.v[1]].position;
            const glm::vec3& p2 = vertices[face.v[2]].position;

            glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
            const float length = glm::length(normal);
            if (length > 1e-8f) {
                normal /= length;
            }
            faceNormals[face.id].faceId = face.id;
            faceNormals[face.id].normal = normal;
        }   
    }

    void recomputeVertexNormals() {
        recomputeFaceNormals();
        vertexNormals.assign(vertices.size(), { -1, glm::vec3(0.0f) });
        for (const TopologyFace& face : faces) {
            if (!face.active) {
                continue;
            }

            for (int vid : face.v) {
                if (isVertexActive(vid)) {
                    vertexNormals[vid].normal += faceNormals[face.id].normal;
                    vertexNormals[vid].vertexId = vid;
                }
            }
        }

        for (std::size_t i = 0; i < vertexNormals.size(); ++i) {
            const float length = glm::length(vertexNormals[i].normal);
            if (length > 1e-8f) {
                vertexNormals[i].normal /= length;
            } else {
                vertexNormals[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    // Shortest edge removal
    // ---------------------
    void precomputeEdgeLengths() {
        for (const auto& [key, edge] : edges) {
            int v0 = edge.v0;
            int v1 = edge.v1;
            // Length calculation
            const glm::vec3 delta = getVertices()[v0].position - getVertices()[v1].position;
            float length = glm::dot(delta, delta);
            // Associated vertex neighbors
            std::vector<int> v0Neighbors = getVertexNeighbors(v0);
            std::vector<int> v1Neighbors = getVertexNeighbors(v1);

            if (gaussianCurvatureEnabled) {
                float Kedge = std::abs(vertices[v0].K) + std::abs(vertices[v1].K);
                length *= (1 - exp(-Kedge)); // costFunc -> costFunc(1 - e^{-aK}) with a = 1
            }

            edgeToLength.emplace(1/length, std::make_pair(std::min(v0, v1), std::max(v0, v1)));
            edgeToNeighbors.emplace(std::make_pair(std::min(v0, v1), std::max(v0, v1)), std::make_pair(v0Neighbors, v1Neighbors));
        }
    }

    void updateEdgeLengths(const std::pair<int, int> &collapseEdge) {
        std::pair<std::vector<int>, std::vector<int>> neighbors = edgeToNeighbors[collapseEdge];

        for (const int n : neighbors.first) {
            if (n == collapseEdge.second) {
                edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                continue;
            }

            TopologyVertex& v0 = vertices[collapseEdge.first];
            TopologyVertex& v1 = vertices[n];
            const glm::vec3 delta = v0.position - v1.position;
            float length = glm::dot(delta, delta);
            if (gaussianCurvatureEnabled) {
                float Kedge = std::abs(v0.K) + std::abs(v1.K);
                length *= (1 - exp(-Kedge)); // costFunc -> costFunc(1 - e^{-aK}) with a = 1
            }
            edgeToLength.emplace(1/length, std::make_pair(std::min(v0.id, v1.id), std::max(v0.id, v1.id)));
        }

        for (const int n : neighbors.second) {
            if (n == collapseEdge.first) {
                edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.second, n), std::max(collapseEdge.second, n)));
                continue;
            }

            TopologyVertex& v0 = vertices[collapseEdge.first];
            TopologyVertex& v1 = vertices[n];

            // Delete all pairs with the second vertex of the edge
            edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.second, n), std::max(collapseEdge.second, n)));

            const glm::vec3 delta = v0.position - v1.position;
            float length = glm::dot(delta, delta);
            if (gaussianCurvatureEnabled) {
                float Kedge = std::abs(v0.K) + std::abs(v1.K);
                length *= (1 - exp(-Kedge)); // costFunc -> costFunc(1 - e^{-aK}) with a = 1
            }
            edgeToLength.emplace(1/length, std::make_pair(std::min(v0.id, v1.id), std::max(v0.id, v1.id)));
        }
    }

    void popEdgeLengths() {
        edgeToLength.pop();
    }

    std::priority_queue<std::pair<float, std::pair<int, int>>>& getEdgeToLength() { return edgeToLength; }
    std::map<std::pair<int, int>, std::pair<std::vector<int>, std::vector<int>>>& getEdgeToNeighbors() { return edgeToNeighbors; }

    // Lowest quadratic error removal
    // -----------------------------
    void precomputeQuadraticErrors() {
        for (const auto& [key, edge] : edges) {
            int v0 = edge.v0;
            int v1 = edge.v1;

            std::tuple<glm::vec3, float> solution = solveQuadratic(edge);
            glm::vec3 vPos(std::get<0>(solution));
            float error = std::get<1>(solution);

            if (gaussianCurvatureEnabled) {
                float Kedge = std::abs(vertices[v0].K) + std::abs(vertices[v1].K);
                error *= (1 - exp(-Kedge)); // costFunc -> costFunc(1 - e^{-aK}) with a = 1
            }

            // Check whether the solution is valid
            if (error < -0.1f) {
                continue;
            }

            std::vector<int> v0Neighbors = getVertexNeighbors(v0);
            std::vector<int> v1Neighbors = getVertexNeighbors(v1);
            
            edgeToPos.emplace(std::make_pair(std::min(v0, v1), std::max(v0, v1)), vPos);
            edgeToQuadraticError.emplace(error, std::make_pair(std::min(v0, v1), std::max(v0, v1)));
            edgeToNeighbors.emplace(std::make_pair(std::min(v0, v1), std::max(v0, v1)), std::make_pair(v0Neighbors, v1Neighbors));
            edgeToCurrentError[std::make_pair(std::min(v0, v1), std::max(v0, v1))] = error;
        }
    }

    void computeInitialVertexQuadrics() {
        // Ensure all vertex quadrics are zeroed out
        for (auto& vertex : vertices) {
            vertex.Q = glm::mat4(0.0f);
        }

        for (const TopologyFace& face : faces) {
            int v0 = face.v[0];
            int v1 = face.v[1];
            int v2 = face.v[2];

            glm::vec3 p0 = vertices[v0].position;
            glm::vec3 p1 = vertices[v1].position;
            glm::vec3 p2 = vertices[v2].position;

            glm::vec3 e1 = p1 - p0;
            glm::vec3 e2 = p2 - p0;
            glm::vec3 n = glm::normalize(glm::cross(e1, e2));

            // Determine a, b, c and d for the plane equation (ax + by + cz + d = 0)
            float a = n.x;
            float b = n.y;
            float c = n.z;
            float d = -glm::dot(n, p0);

            // Determine the fundamental error quadric Kp 
            float coeffs[] = {a, b, c, d};
            glm::mat4 Kp(0.0f);
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    Kp[i][j] = coeffs[i] * coeffs[j];
                }
            }

            vertices[v0].Q += Kp;
            vertices[v1].Q += Kp;
            vertices[v2].Q += Kp;
        }
    }

    void updateQuadraticErrors(const std::pair<int, int> &collapseEdge) {
        std::pair<std::vector<int>, std::vector<int>> neighbors = edgeToNeighbors[collapseEdge];
        
        // Determine the faces of the vertices of the edge
        std::vector<int> faces0 = vertexToFaces[collapseEdge.first];
        std::vector<int> faces1 = vertexToFaces[collapseEdge.second];

        for (const int n : neighbors.first) {
            if (n == collapseEdge.second) {
                edgeToPos.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                edgeToCurrentError.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                continue;
            }

            // Determine common faces
            std::vector<int> nFaces = vertexToFaces[n];
            std::vector<int> commonFaces;
            for (int fid : faces0) {
                if (std::find(nFaces.begin(), nFaces.end(), fid) != nFaces.end()) {
                    commonFaces.push_back(fid);
                }
            }

            TopologyEdge edge = {std::min(collapseEdge.first, n), std::max(collapseEdge.first, n), commonFaces};
            int v0 = edge.v0;
            int v1 = edge.v1;

            std::tuple<glm::vec3, float> solution = solveQuadratic(edge);
            glm::vec3 vPos = std::get<0>(solution);
            float error = std::get<1>(solution);

            if (gaussianCurvatureEnabled) {
                float Kedge = std::abs(vertices[v0].K) + std::abs(vertices[v1].K);
                error *= (1 - exp(-Kedge)); // costFunc -> costFunc(1 - e^{-aK}) with a = 1
            }

            if (error < 0) {
                continue;
            }

            edgeToPos[std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n))] = vPos;
            edgeToQuadraticError.emplace(error, std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
            edgeToCurrentError[std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n))] = error;
        }

        for (const int n : neighbors.second) {
            if (n == collapseEdge.first) {
                edgeToPos.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                edgeToCurrentError.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                continue;
            }

            // Delete all pairs with the second vertex of the edge
            edgeToPos.erase(std::make_pair(std::min(collapseEdge.second, n), std::max(collapseEdge.second, n)));
            edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.second, n), std::max(collapseEdge.second, n)));
            edgeToCurrentError.erase(std::make_pair(std::min(collapseEdge.second, n), std::max(collapseEdge.second, n)));

            // Determine common faces
            std::vector<int> nFaces = vertexToFaces[n];
            std::vector<int> commonFaces;
            for (int fid : faces0) {
                if (std::find(nFaces.begin(), nFaces.end(), fid) != nFaces.end()) {
                    commonFaces.push_back(fid);
                }
            }

            TopologyEdge edge = {std::min(collapseEdge.first, n), std::max(collapseEdge.first, n), commonFaces};
            int v0 = edge.v0;
            int v1 = edge.v1;

            std::tuple<glm::vec3, float> solution = solveQuadratic(edge);
            glm::vec3 vPos = std::get<0>(solution);
            float error = std::get<1>(solution);

            if (gaussianCurvatureEnabled) {
                float Kedge = std::abs(vertices[v0].K) + std::abs(vertices[v1].K);
                error *= (1 - exp(-Kedge)); // costFunc -> costFunc(1 - e^{-aK}) with a = 1
            }

            if (error < 0) {
                continue;
            }

            edgeToPos[std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n))] = vPos;
            edgeToQuadraticError.emplace(error, std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
            edgeToCurrentError[std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n))] = error;
        }
    }

    std::tuple<glm::vec3, float> solveQuadratic(const TopologyEdge& edge) const {
        std::tuple<glm::vec3, float> result(glm::vec3(0.0f), -1.0f);
        int v0 = edge.v0;
        int v1 = edge.v1;

        // Check if the vertices are active
        if (!isVertexActive(v0) || !isVertexActive(v1)) {
            return result;
        }

        // The quadric for the new collapsed vertex
        glm::mat4 Q = vertices[v0].Q + vertices[v1].Q;

        glm::mat4 tildeQ = Q;
        tildeQ[0][3] = 0.0f;
        tildeQ[1][3] = 0.0f;
        tildeQ[2][3] = 0.0f;
        tildeQ[3][3] = 1.0f;

        // Calculate the optimal position
        float det = glm::determinant(tildeQ);
        glm::vec4 vPos4;
        glm::vec3 vPos;
        
        if (std::abs(det) > 1e-6f) {
            vPos4 = glm::inverse(tildeQ) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            vPos = glm::vec3(vPos4);
        } else {
            // If solution is degenerate choose the midpoint
            vPos = (vertices[v0].position + vertices[v1].position) * 0.5f;
            vPos4 = glm::vec4(vPos, 1.0f);
        }

        float error = glm::dot(vPos4, Q * vPos4);
        
        // If error < 0 due to the floating point error
        error = std::max(0.0f, error); 

        result = std::make_tuple(vPos, error);
        return result;
    }

    void popQuadraticError() {
        edgeToQuadraticError.pop();
    }

    std::priority_queue<
    std::pair<float, std::pair<int, int>>, 
    std::vector<std::pair<float, std::pair<int, int>>>, 
    std::greater<std::pair<float, std::pair<int, int>>>
    >& getEdgeToQuadraticError() { return edgeToQuadraticError; }
    std::map<std::pair<int, int>, glm::vec3>& getEdgeToPos() { return edgeToPos; }
    std::map<std::pair<int, int>, float>& getEdgeToCurrentError() { return edgeToCurrentError; }

    // Gaussian curvature
    // ------------------
    void precomputeGaussianCurvatures() {
        for (TopologyVertex &v : vertices) {
            float vid = v.id;
            computeK(vid);
        }
    }

    void computeK(const int vid) {
        float sum_angles = 0; // sum of the adjacent angles to the vertex
        float sum_A = 0; // sum of surface areas of the adjacent faces
        for (const int fid : vertexToFaces[vid]) {
            TopologyFace& face = faces[fid];
            int v0 = face.v[0];
            int v1 = face.v[1];
            int v2 = face.v[2];
            std::vector<int> oppositeVertices; // vids not equal to the seed vertex (vid)
            for (int vidFace : {v0, v1, v2}) {
                if (vidFace != vid) {
                    oppositeVertices.push_back(vidFace);
                }
            }

            if (oppositeVertices.size() != 2) {
                continue;
            }
            
            // Basis vectors of the triangle
            glm::vec3 e1 = vertices[oppositeVertices[0]].position - vertices[vid].position;
            glm::vec3 e2 = vertices[oppositeVertices[1]].position - vertices[vid].position;

            float dot = glm::dot(e1, e2);
            float angle = std::abs(acos(dot / (glm::length(e1) * glm::length(e2))));
            float A = 0.5f * std::abs(glm::length(e1) * glm::length(e2) * sin(angle));

            sum_angles += angle;
            sum_A += A;
        }

        // If the area = 0: the vertex doesn't play a role => huge curvature to remove it first
        if (sum_A <= 0) {
            vertices[vid].K = 1e6;
        }

        vertices[vid].K = 3*(2*M_PI - sum_angles) / sum_A;
    }

    void enableGaussianCurvature() {
        gaussianCurvatureEnabled = true;
    }
    void disableGaussianCurvature() {
        gaussianCurvatureEnabled = false;
    }
    bool isGaussianCurvatureEnabled() {
        return gaussianCurvatureEnabled;
    }

    const std::vector<TopologyVertex>& getVertices() const { return vertices; }
    const std::vector<TopologyFace>& getFaces() const { return faces; }
    

private:
    std::vector<TopologyVertex> vertices;
    std::vector<TopologyVertexNormal> vertexNormals;
    std::vector<TopologyFace> faces;
    std::vector<TopologyFaceNormal> faceNormals;
    std::unordered_map<std::int64_t, TopologyEdge> edges;
    std::vector<std::vector<int>> vertexToFaces;
    bool gaussianCurvatureEnabled = false;

    // For shortest edge removal
    std::priority_queue<std::pair<float, std::pair<int, int>>> edgeToLength;
    std::map<std::pair<int, int>, std::pair<std::vector<int>, std::vector<int>>> edgeToNeighbors;

    // For lowest quadratic error removal
    std::priority_queue<
        std::pair<float, std::pair<int, int>>, 
        std::vector<std::pair<float, std::pair<int, int>>>, 
        std::greater<std::pair<float, std::pair<int, int>>>
    > edgeToQuadraticError;
    std::map<std::pair<int, int>, glm::vec3> edgeToPos;
    std::map<std::pair<int, int>, float> edgeToCurrentError;

    bool isValidVertexId(int vid) const {
        return vid >= 0 && vid < static_cast<int>(vertices.size());
    }

    std::int64_t edgeKey(int a, int b) const {
        const int lo = std::min(a, b);
        const int hi = std::max(a, b);
        return (static_cast<std::int64_t>(lo) << 32) | static_cast<std::uint32_t>(hi);
    }

    void addEdgeFace(int a, int b, int faceId) {
        const int lo = std::min(a, b);
        const int hi = std::max(a, b);
        std::int64_t key = edgeKey(lo, hi);

        if (edges.find(key) != edges.end()) {
            if (std::find(edges[key].faceIds.begin(), edges[key].faceIds.end(), faceId) == edges[key].faceIds.end()) {
                edges[key].faceIds.push_back(faceId);
            }
        } else {
            TopologyEdge edge = {lo, hi, {faceId}};
            auto [it, inserted] = edges.emplace(key, edge);
        }
    }

    void removeEdgeFace(int a, int b, int faceId) {
        const int lo = std::min(a, b);
        const int hi = std::max(a, b);
        std::int64_t key = edgeKey(lo, hi);

        auto it = edges.find(key);
        if (it != edges.end()) {
            auto& faceIds = it->second.faceIds;
            faceIds.erase(std::remove(faceIds.begin(), faceIds.end(), faceId), faceIds.end());
        }
    }

    void removeEdge(int a, int b) {
        const int lo = std::min(a, b);
        const int hi = std::max(a, b);
        std::int64_t key = edgeKey(lo, hi);

        edges.erase(key);
    }


    std::vector<int> getSharedFaces(int a, int b) const {
        std::vector<int> shared;
        // Check if the vertex IDs are valid
        if (a >= static_cast<int>(vertexToFaces.size()) || b >= static_cast<int>(vertexToFaces.size())) {
            return shared;
        }

        for (int fid : vertexToFaces[a]) {
            if (std::find(vertexToFaces[b].begin(), vertexToFaces[b].end(), fid) != vertexToFaces[b].end() && isFaceActive(fid)) {
                shared.push_back(fid);
            }
        }
        return shared;
    }

    void removeDegenerateFaces() {
        for (TopologyFace& face : faces) {
            if (!face.active) {
                continue;
            }

            if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
                face.active = false;
                continue;
            }

            const glm::vec3& p0 = vertices[face.v[0]].position;
            const glm::vec3& p1 = vertices[face.v[1]].position;
            const glm::vec3& p2 = vertices[face.v[2]].position;
            const glm::vec3 cross = glm::cross(p1 - p0, p2 - p0);

            if (glm::length(cross) < 1e-8f) {
                face.active = false;
            }
        }
    }
};
