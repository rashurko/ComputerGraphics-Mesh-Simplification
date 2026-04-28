#pragma once

#include <queue>
#include <memory>
#include <optional>
#include <random>

#include "topology_mesh.hpp"

enum class SimplificationMode {
    Original,
    Random,
    RandomLegal,
    ShortestLegal,
    LowestLegalQError
};

struct CollapseChoice {
    int keepVid = -1;
    int removeVid = -1;
    glm::vec3 newPosition = glm::vec3(0.0f);
    float cost = 0.0f;
};

class SimplificationStrategy {
public:
    virtual ~SimplificationStrategy() = default;
    virtual std::optional<CollapseChoice> chooseCollapse(TopologyMesh& mesh) = 0;
};

class RandomCollapseStrategy : public SimplificationStrategy {
public:
    std::optional<CollapseChoice> chooseCollapse(TopologyMesh& mesh) override {
        std::vector<TopologyEdge> edges = mesh.getActiveEdges();
        if (edges.empty()) {
            return std::nullopt;
        }

        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<std::size_t> dist(0, edges.size() - 1);
        const TopologyEdge& edge = edges[dist(rng)];

        const glm::vec3 midpoint =
            0.5f * (mesh.getVertices()[edge.v0].position + mesh.getVertices()[edge.v1].position);

        return CollapseChoice{ edge.v0, edge.v1, midpoint, 0.0f };
    }
};

class RandomLegalCollapseStrategy : public SimplificationStrategy {
public:
    std::optional<CollapseChoice> chooseCollapse(TopologyMesh& mesh) override {
        std::vector<TopologyEdge> edges = mesh.getActiveEdges();
        if (edges.empty()) {
            return std::nullopt;
        }

        static std::mt19937 rng(std::random_device{}());
        std::shuffle(edges.begin(), edges.end(), rng);

        for (const TopologyEdge& edge : edges) {
            if (!mesh.isLegalCollapse(edge.v0, edge.v1)) {
                continue;
            }

            const glm::vec3 midpoint =
                0.5f * (mesh.getVertices()[edge.v0].position + mesh.getVertices()[edge.v1].position);

            return CollapseChoice{ edge.v0, edge.v1, midpoint, 0.0f };
        }

        return std::nullopt;
    }
};

class ShortestLegalCollapseStrategy : public SimplificationStrategy {
public:
    std::optional<CollapseChoice> chooseCollapse(TopologyMesh& mesh) override {
        std::optional<CollapseChoice> bestChoice;

        if (mesh.getEdgeToLength().empty()) {
            mesh.precomputeEdgeLengths();
        }

        while (!mesh.getEdgeToLength().empty()) {
            auto edgeToLength = mesh.getEdgeToLength();
            const std::pair<int, int> shortestEdge = edgeToLength.top().second;
            auto edgeToNeighbors = mesh.getEdgeToNeighbors();
            std::pair<std::vector<int>, std::vector<int>> neighbors = edgeToNeighbors[shortestEdge];
            const float length = sqrt(1 / edgeToLength.top().first);
            mesh.popEdgeLengths();

            if (!mesh.isLegalCollapse(shortestEdge.first, shortestEdge.second)) {
                continue;
            }

            const glm::vec3 midpoint = 0.5f * (mesh.getVertices()[shortestEdge.first].position + mesh.getVertices()[shortestEdge.second].position);

            bestChoice = CollapseChoice{shortestEdge.first, shortestEdge.second, midpoint, length};
            mesh.updateEdgeLengths(shortestEdge);
            break;
        }
        return bestChoice;
    }
};

class LowestQuadraticErrorStrategy : public SimplificationStrategy {
    std::optional<CollapseChoice> chooseCollapse(TopologyMesh& mesh) override {
        std::optional<CollapseChoice> bestChoice;

        if (mesh.getEdgeToQuadraticError().empty()) {
            mesh.computeInitialVertexQuadrics();
            mesh.precomputeQuadraticErrors();
        }

        while(!mesh.getEdgeToQuadraticError().empty()) {
            const std::pair<int, int> shortestEdge = mesh.getEdgeToQuadraticError().top().second;
            auto edgeToNeighbors = mesh.getEdgeToNeighbors();
            std::pair<std::vector<int>, std::vector<int>> neighbors = edgeToNeighbors[shortestEdge];
            const float error = mesh.getEdgeToQuadraticError().top().first;
            mesh.popQuadraticError();

    
            const auto& currentErrors = mesh.getEdgeToCurrentError();
            auto it = currentErrors.find(shortestEdge);   
            // If the edge was completely erased, or the error doesn't match
            if (it == currentErrors.end() || error != it->second) {
                continue;
            }

            if (!mesh.isLegalCollapse(shortestEdge.first, shortestEdge.second)) {
                continue;
            }

            const glm::vec3 newPoint = mesh.getEdgeToPos().at(std::make_pair(std::min(shortestEdge.first, shortestEdge.second), std::max(shortestEdge.first, shortestEdge.second)));

            bestChoice = CollapseChoice{shortestEdge.first, shortestEdge.second, newPoint, error};
            mesh.updateQuadraticErrors(shortestEdge);
            break;
        }

        return bestChoice;
    }
};

class SimplificationController {
public:
    void setOriginalMesh(const TopologyMesh& mesh) {
        originalMesh = mesh;
        workingMesh = mesh;
    }

    void setMode(SimplificationMode newMode) {
        currentMode = newMode;
        reset();
    }

    void reset() {
        workingMesh = originalMesh;
    }

    bool applyOneStep() {
        if (currentMode == SimplificationMode::Original) {
            return false;
        }

        std::unique_ptr<SimplificationStrategy> strategy = makeStrategy();
        if (!strategy) {
            return false;
        }

        std::optional<CollapseChoice> choice = strategy->chooseCollapse(workingMesh);
        if (!choice.has_value()) {
            return false;
        }

        return workingMesh.collapseEdge(choice->keepVid, choice->removeVid, choice->newPosition);
    }

    bool simplifyToFaceCount(int targetFaces) {
        bool changed = false;
        while (workingMesh.activeFaceCount() > targetFaces) {
            if (!applyOneStep()) {
                break;
            }
            changed = true;
        }
        return changed;
    }

    TopologyMesh& currentMesh() { return workingMesh; }
    SimplificationMode mode() const { return currentMode; }

private:
    TopologyMesh originalMesh;
    TopologyMesh workingMesh;
    SimplificationMode currentMode = SimplificationMode::Original;

    std::unique_ptr<SimplificationStrategy> makeStrategy() const {
        switch (currentMode) {
        case SimplificationMode::Random:
            return std::make_unique<RandomCollapseStrategy>();
        case SimplificationMode::RandomLegal:
            return std::make_unique<RandomLegalCollapseStrategy>();
        case SimplificationMode::ShortestLegal:
            return std::make_unique<ShortestLegalCollapseStrategy>();
        case SimplificationMode::LowestLegalQError:
            return std::make_unique<LowestQuadraticErrorStrategy>();
        case SimplificationMode::Original:
        default:
            return nullptr;
        }
    }
};
