#pragma once

#include <algorithm>
#include <array>
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

        firstPass = true;
    }

    int addVertex(const glm::vec3& position) {
        const int id = static_cast<int>(vertices.size());
        vertices.push_back({ id, position, true });
        vertexToFaces.emplace_back();
        return id;
    }

    int addFace(int a, int b, int c) {
        const int id = static_cast<int>(faces.size());
        faces.push_back({ id, { a, b, c }, true });
        return id;
    }

    void buildAdjacency() {
        if (firstPass) {
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

                firstPass = false;
                affectedFaces.clear();
            }
        } else {
            for (const int faceId : affectedFaces) {
                TopologyFace &face = faces[faceId];

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
                    auto &vFaces = vertexToFaces[vid];
                    if (std::find(vFaces.begin(), vFaces.end(), face.id) == vFaces.end()) {
                        vFaces.push_back(face.id);
                    }
                }

                addEdgeFace(face.v[0], face.v[1], face.id);
                addEdgeFace(face.v[1], face.v[2], face.id);
                addEdgeFace(face.v[2], face.v[0], face.id);
            }            
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
        int count = 0;
        for (const auto& entry : edges) {
            const TopologyEdge& edge = entry.second;
            if (isVertexActive(edge.v0) && isVertexActive(edge.v1)) {
                count++;
            }
        }
        return count;
    }

    std::vector<TopologyEdge> getActiveEdges() const {
        std::vector<TopologyEdge> result;
        result.reserve(edges.size());
        for (const auto& [key, edge] : edges) {
            if (isVertexActive(edge.v0) && isVertexActive(edge.v1)) {
                result.push_back(edge);
            }
        }
        return result;
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

        for (const auto& faceid : vertexToFaces[removeVid]) {
            TopologyFace& face = faces[faceid];
            if (!face.active) {
                continue;
            }

            bool touched = false;
            // Update face vertices
            for (int& vid : face.v) {
                if (vid == removeVid) {
                    vid = keepVid;
                    touched = true;
                }
            }

            if (!touched) {
                continue;
            }

            if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
                face.active = false;
            }

            affectedFaces.push_back(face.id);
        }

        vertices[removeVid].active = false;

        removeDegenerateFaces();
        buildAdjacency();
        return true;
    }

    TopologyRenderData toRenderData() {
        TopologyRenderData renderData;
        recomputeVertexNormals();

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
        // On first run
        if (faceNormals.size() != faces.size()) {
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
        } else {
            for (const int faceId : affectedFaces) {
            if (!isFaceActive(faceId)) {
                continue;
            }

            const glm::vec3& p0 = vertices[faces[faceId].v[0]].position;
            const glm::vec3& p1 = vertices[faces[faceId].v[1]].position;
            const glm::vec3& p2 = vertices[faces[faceId].v[2]].position;

            glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
            const float length = glm::length(normal);
            if (length > 1e-8f) {
                normal /= length;
            }
            faceNormals[faceId].faceId = faceId;
            faceNormals[faceId].normal = normal;
            }
        }
    }

    void recomputeVertexNormals() {
        recomputeFaceNormals();

        // On first run
        if (vertexNormals.size() != vertices.size()) {
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

        } else {
            std::vector<int> affectedVertices;
            for (const int faceId : affectedFaces) {
                if (!isFaceActive(faceId)) {
                    continue;
                }

                for (int vid : faces[faceId].v) {
                    if (isVertexActive(vid)) {
                        affectedVertices.push_back(vid);
                    }
                }
            }

            // remove duplicate vertex normals
            std::sort(affectedVertices.begin(), affectedVertices.end());
            affectedVertices.erase(std::unique(affectedVertices.begin(), affectedVertices.end()), affectedVertices.end());

            // Clear affected vertex normals
            for (int vid : affectedVertices) {
                vertexNormals[vid].normal = glm::vec3(0.0f);
            }

            // Recompute affected vertex normals
            for (int vid : affectedVertices) {
                for (const int faceId : vertexToFaces[vid]) {
                    if (!isFaceActive(faceId)) {
                        continue;
                    }
                    vertexNormals[vid].normal += faceNormals[faceId].normal;
                }
            }

            // Normalize the affected vertex normals
            for (int vid : affectedVertices) {
                const float length = glm::length(vertexNormals[vid].normal);
                if (length > 1e-8f) {
                    vertexNormals[vid].normal /= length;
                } else {
                    vertexNormals[vid].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
        }

        affectedFaces.clear();
    }

    void precomputeEdgeLengths() {
        for (const TopologyEdge& edge : getActiveEdges()) {
            int v0 = edge.v0;
            int v1 = edge.v1;
            // Length calculation
            const glm::vec3 delta = getVertices()[v0].position - getVertices()[v1].position;
            const float length = glm::dot(delta, delta);
            // Associated vertex neighbors
            std::vector<int> v0Neighbors = getVertexNeighbors(v0);
            std::vector<int> v1Neighbors = getVertexNeighbors(v1);

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

            const glm::vec3 delta = getVertices()[collapseEdge.first].position - getVertices()[n].position;
            const float length = glm::dot(delta, delta);
            edgeToLength.emplace(1/length, std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
        }

        for (const int n : neighbors.second) {
            if (n == collapseEdge.first) {
                edgeToNeighbors.erase(std::make_pair(std::min(collapseEdge.first, n), std::max(collapseEdge.first, n)));
                continue;
            }

            const glm::vec3 delta = getVertices()[collapseEdge.second].position - getVertices()[n].position;
            const float length = glm::dot(delta, delta);
            edgeToLength.emplace(1/length, std::make_pair(std::min(collapseEdge.second, n), std::max(collapseEdge.second, n)));
        }
    }

    void popEdgeLengths() {
        edgeToLength.pop();
    }

    const std::vector<TopologyVertex>& getVertices() const { return vertices; }
    const std::vector<TopologyFace>& getFaces() const { return faces; }
    const std::priority_queue<std::pair<float, std::pair<int, int>>> getEdgeToLength() const { return edgeToLength; }
    const std::map<std::pair<int, int>, std::pair<std::vector<int>, std::vector<int>>> getEdgeToNeighbors() const { return edgeToNeighbors; }

private:
    std::vector<TopologyVertex> vertices;
    std::vector<TopologyVertexNormal> vertexNormals;
    std::vector<TopologyFace> faces;
    std::vector<TopologyFaceNormal> faceNormals;
    std::unordered_map<std::int64_t, TopologyEdge> edges;
    std::vector<std::vector<int>> vertexToFaces;

    // For sorting using length
    std::priority_queue<std::pair<float, std::pair<int, int>>> edgeToLength;
    std::map<std::pair<int, int>, std::pair<std::vector<int>, std::vector<int>>> edgeToNeighbors;

    // Affected faces
    bool firstPass = true;
    std::vector<int> affectedFaces;

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

        auto [it, inserted] = edges.emplace(key, TopologyEdge{ lo, hi, {} });
        it->second.faceIds.push_back(faceId);
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
        for (int fid : affectedFaces) {
            TopologyFace& face = faces[fid];
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
