#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <unordered_map>

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "OBJ_Loader.h"
#include "stb_image.h"

#include "shader.hpp"
#include "mesh.hpp"
#include "simplification.hpp"

class Model {
    public:
        bool isCollapsing = false;
        bool useLegal = false;
        bool showCurv = false;

        Model(std::string path) {
            loadModel(path);
        }

        void Draw(Shader &shader) {
            const std::vector<Mesh>& drawMeshes = activeMeshes();
            for (unsigned int i = 0; i < drawMeshes.size(); i++) {
                drawMeshes[i].Draw(shader);
            }
        }

        void loadFromPath(const std::string& path) {
            loadModel(path);
        }

        void saveModel(const std::string& path) {
            TopologyMesh workingMesh = simplifier.currentMesh();

            if (!path.empty()) {
                saveMeshToOBJ(workingMesh, path);
            }
        }

        void setSimplificationMode(SimplificationMode mode) {
            simplifier.setMode(mode);
            pendingCollapses = 0;
            progressiveEnabled = false;
            progressiveLevels.clear();
            rebuildRenderMeshesFromCurrentTopology();
        }

        void setUseLegal(bool useLegal) {
            simplifier.useLegal = useLegal;
            simplifier.setWorkingMeshUseLegal(useLegal);
            clearProgressiveMeshes();
        }

        void toggleGaussianCurvature() {
            if (simplifier.isGaussianCurvatureEnabled()) {
                simplifier.disableGaussianCurvature();
            } else {
                simplifier.enableGaussianCurvature();
            }
            gaussianCurvatureEnabled = simplifier.isGaussianCurvatureEnabled();
        }

        void setGaussianCurvature(bool useGaussianCurvature) {
            if (useGaussianCurvature) {
                simplifier.enableGaussianCurvature();
            } else {
                simplifier.disableGaussianCurvature();
            }
            gaussianCurvatureEnabled = simplifier.isGaussianCurvatureEnabled();
            clearProgressiveMeshes();
        }

        void setShowCurv(bool showCurv) {
            simplifier.setShowCurv(showCurv);
            this->showCurv = showCurv;
            rebuildRenderMeshesFromCurrentTopology();
        }

        void setMaxK(float maxK) {
            simplifier.setMaxK(maxK);
            rebuildRenderMeshesFromCurrentTopology();
        }

        bool isGaussianCurvatureEnabled() const {
            return gaussianCurvatureEnabled;
        }

        void setAlpha(float aNew) {
            simplifier.setAlpha(aNew);
            clearProgressiveMeshes();
        }

        void resetSimplification() {
            simplifier.reset();
            pendingCollapses = 0;
            clearProgressiveMeshes();
            rebuildRenderMeshesFromCurrentTopology();
        }

        void queueCollapseBatch(int steps) {
            if (steps > 0) {
                clearProgressiveMeshes();
                pendingCollapses += steps;
            }
        }

        bool processPendingCollapses(int maxStepsPerFrame) {
            if (!isCollapsing && pendingCollapses > 0) {
                isCollapsing = true;
            }

            if (pendingCollapses <= 0 || maxStepsPerFrame <= 0) {
                return false;
            }

            if (simplifier.mode() == SimplificationMode::Original) {
                pendingCollapses = 0;
                return false;
            }

            bool changed = false;
            const int stepsThisFrame = std::min(pendingCollapses, maxStepsPerFrame);
            for (int i = 0; i < stepsThisFrame; ++i) {
                if (!simplifier.applyOneStep()) {
                    pendingCollapses = 0;
                    break;
                }
                pendingCollapses--;
                changed = true;
            }

            return changed;
        }

        int pendingCollapseCount() const {
            return pendingCollapses;
        }

        SimplificationMode currentMode() const {
            return simplifier.mode();
        }

        const char* currentModeName() const {
            switch (simplifier.mode()) {
            case SimplificationMode::Original:
                return "Original";
            case SimplificationMode::Random:
                return "Random";
            case SimplificationMode::RandomLegal:
                return "RandomLegal";
            case SimplificationMode::ShortestLegal:
                return "ShortestLegal";
            case SimplificationMode::LowestLegalQError:
                return "LowestLegalQError";
            default:
                return "Unknown";
            }
        }

        int activeTriangleCount() {
            return simplifier.currentMesh().activeFaceCount();
        }

        int activeEdgeCount() {
            return simplifier.currentMesh().activeEdgeCount();
        }

        glm::vec3 getLocalX() const {
            return localX;
        }

        glm::vec3 getLocalY() const {
            return localY;
        }

        glm::vec3 getLocalZ() const {
            return localZ;
        }

        glm::vec3 setLocalX(glm::vec3 newX) {
            localX = newX;
            return localX;
        }

        glm::vec3 setLocalY(glm::vec3 newY) {
            localY = newY;
            return localY;
        }

        glm::vec3 setLocalZ(glm::vec3 newZ) {
            localZ = newZ;
            return localZ;
        }

        void recomputeNormals() {
            simplifier.currentMesh().recomputeVertexNormals();
        }

        void rebuildRenderMeshesFromCurrentTopology() {
            meshes.clear();

            const TopologyRenderData renderData = simplifier.currentMesh().toRenderData();
            if (renderData.vertices.empty() || renderData.indices.empty()) {
                return;
            }

            std::vector<Vertex> renderVertices;
            renderVertices.reserve(renderData.vertices.size());

            for (const TopologyRenderVertex& sourceVertex : renderData.vertices) {
                Vertex vertex;
                vertex.Position = sourceVertex.position;
                vertex.Normal = sourceVertex.normal;
                vertex.TexCoords = sourceVertex.texCoord;
                vertex.Color = sourceVertex.color;
                renderVertices.push_back(vertex);
            }

            meshes.emplace_back(renderVertices, renderData.indices, std::vector<Texture>{});
        }

        const std::string getOriginalPath() const {
            return originalPath;
        }
        const std::string getSimplifiedPath() const {
            return simplifiedPath;
        }

        const bool isSaved() const {
            return saved;
        }

        float boundingRadius() const {
            return boundsRadius;
        }

        bool buildProgressiveMeshes(int requestedLevelCount, float minFaceRatio) {
            const int originalFaces = originalTopology.activeFaceCount();
            if (originalFaces <= 0) {
                return false;
            }

            const int levelCount = std::clamp(requestedLevelCount, 2, 8);
            const float minRatio = std::clamp(minFaceRatio, 0.02f, 0.95f);
            std::vector<ProgressiveMeshLevel> newLevels(levelCount);

            SimplificationController levelBuilder;
            levelBuilder.setOriginalMesh(originalTopology);
            levelBuilder.setMode(progressiveBuildMode());
            levelBuilder.setWorkingMeshUseLegal(true);
            if (gaussianCurvatureEnabled) {
                levelBuilder.enableGaussianCurvature();
            }

            for (int level = levelCount - 1; level >= 0; --level) {
                const float t = levelCount == 1 ? 1.0f : static_cast<float>(level) / static_cast<float>(levelCount - 1);
                const float ratio = minRatio + (1.0f - minRatio) * t;
                const int targetFaces = std::max(4, static_cast<int>(std::round(originalFaces * ratio)));

                if (levelBuilder.currentMesh().activeFaceCount() > targetFaces) {
                    levelBuilder.simplifyToFaceCount(targetFaces);
                }

                levelBuilder.currentMesh().recomputeVertexNormals();
                newLevels[level].meshes = buildMeshesFromTopology(levelBuilder.currentMesh());
                newLevels[level].faceCount = levelBuilder.currentMesh().activeFaceCount();
                newLevels[level].detailRatio = static_cast<float>(newLevels[level].faceCount) / static_cast<float>(originalFaces);
            }

            if (newLevels.front().meshes.empty() || newLevels.back().meshes.empty()) {
                return false;
            }

            progressiveLevels = std::move(newLevels);
            currentProgressiveLevel = 0;
            progressiveEnabled = true;
            return true;
        }

        void setProgressiveMeshesEnabled(bool enabled) {
            progressiveEnabled = enabled && !progressiveLevels.empty();
        }

        void clearProgressiveMeshes() {
            progressiveEnabled = false;
            progressiveLevels.clear();
            currentProgressiveLevel = 0;
        }

        bool isProgressiveMeshesEnabled() const {
            return progressiveEnabled;
        }

        bool hasProgressiveMeshes() const {
            return !progressiveLevels.empty();
        }

        int progressiveLevelCount() const {
            return static_cast<int>(progressiveLevels.size());
        }

        int currentProgressiveLevelIndex() const {
            return currentProgressiveLevel;
        }

        int currentProgressiveFaceCount() const {
            if (progressiveLevels.empty()) {
                return simplifier.currentMesh().activeFaceCount();
            }
            return progressiveLevels[currentProgressiveLevel].faceCount;
        }

        float currentProgressiveDetailRatio() const {
            if (progressiveLevels.empty()) {
                return 1.0f;
            }
            return progressiveLevels[currentProgressiveLevel].detailRatio;
        }

        void updateProgressiveLevel(
            const glm::vec3& cameraPos,
            const glm::vec3& cameraFront,
            const glm::vec3& cameraUp,
            float verticalFovDegrees,
            float aspectRatio,
            const glm::mat4& modelMatrix) {

            if (!progressiveEnabled || progressiveLevels.empty()) {
                return;
            }

            const glm::vec3 worldCenter = glm::vec3(modelMatrix * glm::vec4(boundsCenter, 1.0f));
            const glm::mat3 modelLinear(modelMatrix);
            const float maxScale = std::max({
                glm::length(modelLinear[0]),
                glm::length(modelLinear[1]),
                glm::length(modelLinear[2])
            });
            const float worldRadius = std::max(boundsRadius * maxScale, 1e-4f);

            const glm::vec3 toCenter = worldCenter - cameraPos;
            const float distance = glm::length(toCenter);
            if (distance < 1e-5f) {
                currentProgressiveLevel = static_cast<int>(progressiveLevels.size()) - 1;
                return;
            }

            const float nearDistance = std::max(worldRadius * 2.5f, 1e-4f);
            const float farDistance = std::max(worldRadius * 8.0f, nearDistance + 1.0f);
            if (distance <= nearDistance) {
                currentProgressiveLevel = static_cast<int>(progressiveLevels.size()) - 1;
                return;
            }

            const glm::vec3 front = glm::normalize(cameraFront);
            const glm::vec3 dir = toCenter / distance;
            const float forward = glm::dot(dir, front);
            const float angularRadius = glm::degrees(std::asin(std::min(worldRadius / distance, 1.0f)));
            if (forward <= 0.0f && angularRadius < 45.0f) {
                currentProgressiveLevel = 0;
                return;
            }

            const glm::vec3 right = glm::normalize(glm::cross(front, cameraUp));
            const glm::vec3 up = glm::normalize(glm::cross(right, front));
            const float horizontalAngle = glm::degrees(std::atan2(std::abs(glm::dot(dir, right)), forward));
            const float verticalAngle = glm::degrees(std::atan2(std::abs(glm::dot(dir, up)), forward));
            const float verticalHalfFov = std::max(verticalFovDegrees * 0.5f, 1.0f);
            const float horizontalHalfFov = glm::degrees(std::atan(std::tan(glm::radians(verticalHalfFov)) * aspectRatio));

            const float horizontalVisibility = 1.0f - std::clamp((horizontalAngle - angularRadius) / horizontalHalfFov, 0.0f, 1.0f);
            const float verticalVisibility = 1.0f - std::clamp((verticalAngle - angularRadius) / verticalHalfFov, 0.0f, 1.0f);
            const float fovVisibility = std::min(horizontalVisibility, verticalVisibility);
            if (fovVisibility <= 0.0f) {
                currentProgressiveLevel = 0;
                return;
            }

            const float projectedDetail = std::clamp(angularRadius / verticalHalfFov, 0.0f, 1.0f);
            const float distanceT = std::clamp((distance - nearDistance) / (farDistance - nearDistance), 0.0f, 1.0f);
            const float smoothDistanceT = distanceT * distanceT * (3.0f - 2.0f * distanceT);
            const float distanceDetail = 1.0f - smoothDistanceT;
            const float centerWeight = 0.35f + 0.65f * fovVisibility;
            const float detail = std::clamp(std::max(projectedDetail * 0.5f, distanceDetail) * centerWeight, 0.0f, 1.0f);

            currentProgressiveLevel = static_cast<int>(std::round(detail * static_cast<float>(progressiveLevels.size() - 1)));
        }
        
    private:
        struct ProgressiveMeshLevel {
            std::vector<Mesh> meshes;
            int faceCount = 0;
            float detailRatio = 1.0f;
        };

        struct TextureImage {
            int width = 0;
            int height = 0;
            int components = 0;
            std::vector<unsigned char> pixels;
        };

        // model data
        std::vector<Mesh> meshes;
        std::vector<ProgressiveMeshLevel> progressiveLevels;
        bool progressiveEnabled = false;
        int currentProgressiveLevel = 0;

        std::string directory;
        std::string originalPath;
        std::string simplifiedPath;
        bool saved = false;
        std::unordered_map<std::string, TextureImage> textureCache;

        TopologyMesh originalTopology;
        SimplificationController simplifier;
        int pendingCollapses = 0;
        bool gaussianCurvatureEnabled = false;

        struct PositionKey {
            std::uint32_t x = 0;
            std::uint32_t y = 0;
            std::uint32_t z = 0;

            bool operator==(const PositionKey& other) const {
                return x == other.x && y == other.y && z == other.z;
            }
        };

        struct PositionKeyHasher {
            std::size_t operator()(const PositionKey& key) const {
                std::size_t seed = 0;
                seed ^= std::hash<std::uint32_t>{}(key.x) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::uint32_t>{}(key.y) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::uint32_t>{}(key.z) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        // local axis
        glm::vec3 localX;
        glm::vec3 localY;
        glm::vec3 localZ;
        glm::vec3 boundsCenter = glm::vec3(0.0f);
        float boundsRadius = 1.0f;

        void loadModel(std::string path) {
            meshes.clear();
            clearProgressiveMeshes();
            originalTopology.clear();
            pendingCollapses = 0;
            textureCache.clear();
            std::unordered_map<PositionKey, int, PositionKeyHasher> topologyVertexMap;
            originalPath = path;
            const std::filesystem::path modelPath(path);
            directory = modelPath.has_parent_path() ? modelPath.parent_path().generic_string() : ".";

            // Initialize loader
            objl::Loader loader;

            // Load .obj file
            bool isLoaded = false;
            try {
                isLoaded = loader.LoadFile(path);
            } catch (const std::exception& error) {
                std::cout << "Failed to load " << path << ": " << error.what() << std::endl;
            } catch (...) {
                std::cout << "Failed to load " << path << ": unknown OBJ loader error" << std::endl;
            }

            // Check if the file is properly loaded
            if (isLoaded) {

                // Go through each loaded mesh
                for (int i = 0; i < loader.LoadedMeshes.size(); i++) {
                    // copy the current mesh
                    objl::Mesh curMesh = loader.LoadedMeshes[i];
                    const glm::vec3 materialColor = diffuseColorFromMaterial(curMesh.MeshMaterial);

                    for (int j = 0; j + 2 < curMesh.Indices.size(); j += 3) {
                        int topoFace[3];
                        glm::vec2 texCoords[3];
                        bool validTriangle = true;

                        for (int k = 0; k < 3; ++k) {
                            const unsigned int meshIndex = curMesh.Indices[j + k];
                            if (meshIndex >= curMesh.Vertices.size()) {
                                validTriangle = false;
                                break;
                            }

                            const objl::Vertex& sourceVertex = curMesh.Vertices[meshIndex];
                            const glm::vec3 position(
                                sourceVertex.Position.X,
                                sourceVertex.Position.Y,
                                sourceVertex.Position.Z);
                            texCoords[k] = glm::vec2(
                                sourceVertex.TextureCoordinate.X,
                                sourceVertex.TextureCoordinate.Y);

                            const PositionKey key = makePositionKey(position);
                            const auto existing = topologyVertexMap.find(key);
                            if (existing == topologyVertexMap.end()) {
                                const int newVertexId = originalTopology.addVertex(position);
                                topologyVertexMap.emplace(key, newVertexId);
                                topoFace[k] = newVertexId;
                            } else {
                                topoFace[k] = existing->second;
                            }
                        }

                        if (!validTriangle) {
                            continue;
                        }

                        originalTopology.addFace(
                            topoFace[0],
                            topoFace[1],
                            topoFace[2],
                            faceColorFromMaterial(curMesh.MeshMaterial, materialColor, texCoords));
                    }
                }

            } else {
                std::cout << "Failed to load " << path << std::endl;
            }

            originalTopology.buildAdjacency();
            updateBoundsFromTopology();
            simplifier.setOriginalMesh(originalTopology);
            simplifier.setMode(SimplificationMode::RandomLegal);
            rebuildRenderMeshesFromCurrentTopology();

            // Declare local axes
            localX = glm::vec3(1.0f, 0.0f, 0.0f);
            localY = glm::vec3(0.0f, 1.0f, 0.0f);
            localZ = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        void saveMeshToOBJ(const TopologyMesh& mesh, const std::string& filename) {
            std::ofstream outFile(filename);
            if (!outFile.is_open()) {
                std::cerr << "Could not open file for saving: " << filename << std::endl;
                return;
            }

            outFile << std::fixed << std::setprecision(6);

            // Map from original vertex ID to the new index in the .obj file
            std::unordered_map<int, int> oldToNewIndex;
            int nextIdx = 1;

            const auto& vertices = mesh.getVertices();
            const auto& faces = mesh.getFaces();

            // Write active vertices
            for (const auto& v : vertices) {
                if (v.active) {
                    outFile << "v " << v.position.x << " "
                                    << v.position.y << " "
                                    << v.position.z << "\n";
                    oldToNewIndex[v.id] = nextIdx;
                    nextIdx += 1;
                }
            }

            // Write active faces
            for (const auto& f : faces) {
                if (f.active) {
                    // Check whether all vertices of this face are valid
                    if (oldToNewIndex.count(f.v[0]) && oldToNewIndex.count(f.v[1]) && oldToNewIndex.count(f.v[2])) {
                        outFile << "f " << oldToNewIndex[f.v[0]] << " "
                                        << oldToNewIndex[f.v[1]] << " "
                                        << oldToNewIndex[f.v[2]] << "\n";
                    }
                }
            }

            outFile.close();
            std::cout << "Saved model to " << filename << std::endl;
            simplifiedPath = filename;
            saved = true;
        }

        PositionKey makePositionKey(const glm::vec3& position) const {
            PositionKey key;
            std::memcpy(&key.x, &position.x, sizeof(float));
            std::memcpy(&key.y, &position.y, sizeof(float));
            std::memcpy(&key.z, &position.z, sizeof(float));
            return key;
        }

        const std::vector<Mesh>& activeMeshes() const {
            if (progressiveEnabled && !progressiveLevels.empty()) {
                return progressiveLevels[currentProgressiveLevel].meshes;
            }
            return meshes;
        }

        SimplificationMode progressiveBuildMode() const {
            if (simplifier.mode() == SimplificationMode::LowestLegalQError ||
                simplifier.mode() == SimplificationMode::ShortestLegal) {
                return simplifier.mode();
            }
            return SimplificationMode::ShortestLegal;
        }

        std::vector<Mesh> buildMeshesFromTopology(TopologyMesh& topology) {
            std::vector<Mesh> result;
            const TopologyRenderData renderData = topology.toRenderData();
            if (renderData.vertices.empty() || renderData.indices.empty()) {
                return result;
            }

            std::vector<Vertex> renderVertices;
            renderVertices.reserve(renderData.vertices.size());
            for (const TopologyRenderVertex& sourceVertex : renderData.vertices) {
                Vertex vertex;
                vertex.Position = sourceVertex.position;
                vertex.Normal = sourceVertex.normal;
                vertex.TexCoords = sourceVertex.texCoord;
                vertex.Color = sourceVertex.color;
                renderVertices.push_back(vertex);
            }

            result.emplace_back(renderVertices, renderData.indices, std::vector<Texture>{});
            return result;
        }

        void updateBoundsFromTopology() {
            const auto& vertices = originalTopology.getVertices();
            if (vertices.empty()) {
                boundsCenter = glm::vec3(0.0f);
                boundsRadius = 1.0f;
                return;
            }

            glm::vec3 minBounds(std::numeric_limits<float>::max());
            glm::vec3 maxBounds(std::numeric_limits<float>::lowest());
            for (const TopologyVertex& vertex : vertices) {
                if (!vertex.active) {
                    continue;
                }
                minBounds = glm::min(minBounds, vertex.position);
                maxBounds = glm::max(maxBounds, vertex.position);
            }

            boundsCenter = 0.5f * (minBounds + maxBounds);
            boundsRadius = 0.0f;
            for (const TopologyVertex& vertex : vertices) {
                if (vertex.active) {
                    boundsRadius = std::max(boundsRadius, glm::length(vertex.position - boundsCenter));
                }
            }
            boundsRadius = std::max(boundsRadius, 1e-4f);
        }

        Mesh processMesh(objl::Mesh mesh) {
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;
            std::vector<Texture> textures;

            // Go through each vertex and push it into the vertices vector
            for (int i = 0; i < mesh.Vertices.size(); i++) {
                Vertex vertex;
                vertex.Position = glm::vec3(mesh.Vertices[i].Position.X, mesh.Vertices[i].Position.Y, mesh.Vertices[i].Position.Z);
                vertex.Normal = glm::vec3(mesh.Vertices[i].Normal.X, mesh.Vertices[i].Normal.Y, mesh.Vertices[i].Normal.Z);
                vertex.TexCoords = glm::vec2(mesh.Vertices[i].TextureCoordinate.X, mesh.Vertices[i].TextureCoordinate.Y);
                vertex.Color = diffuseColorFromMaterial(mesh.MeshMaterial);
                vertices.push_back(vertex);
            }

            // Go through each index and push it into the indices vector
            for (int i = 0; i < mesh.Indices.size(); i++) {
                indices.push_back(mesh.Indices[i]);
            } 

            // Process the material
            Texture texture;
            texture.id = TextureFromFile(mesh.MeshMaterial, directory);
            texture.type = "texture_diffuse";
            textures.push_back(texture);

            // Return the processed mesh
            return Mesh(vertices, indices, textures);   
        }

        glm::vec3 diffuseColorFromMaterial(const objl::Material& material) const {
            glm::vec3 color(material.Kd.X, material.Kd.Y, material.Kd.Z);
            if (color.x <= 0.0f && color.y <= 0.0f && color.z <= 0.0f) {
                return glm::vec3(1.0f);
            }
            return glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
        }

        glm::vec3 faceColorFromMaterial(
            const objl::Material& material,
            const glm::vec3& materialColor,
            const glm::vec2 texCoords[3]) {

            if (material.map_Kd.empty()) {
                return materialColor;
            }

            const TextureImage* texture = loadDiffuseTexture(material.map_Kd);
            if (texture == nullptr || texture->pixels.empty()) {
                return materialColor;
            }

            glm::vec3 textureColor(0.0f);
            for (int i = 0; i < 3; ++i) {
                textureColor += sampleTexture(*texture, texCoords[i]);
            }
            textureColor /= 3.0f;
            return glm::clamp(textureColor * materialColor, glm::vec3(0.0f), glm::vec3(1.0f));
        }

        const TextureImage* loadDiffuseTexture(const std::string& textureName) {
            const std::string path = resolveTexturePath(textureName);
            if (path.empty()) {
                return nullptr;
            }

            auto cached = textureCache.find(path);
            if (cached != textureCache.end()) {
                return &cached->second;
            }

            int width = 0;
            int height = 0;
            int components = 0;
            unsigned char* data = stbi_load(path.c_str(), &width, &height, &components, 3);
            if (data == nullptr || width <= 0 || height <= 0) {
                stbi_image_free(data);
                return nullptr;
            }

            TextureImage image;
            image.width = width;
            image.height = height;
            image.components = 3;
            image.pixels.assign(data, data + width * height * 3);
            stbi_image_free(data);

            auto [it, inserted] = textureCache.emplace(path, std::move(image));
            return &it->second;
        }

        std::string resolveTexturePath(const std::string& textureName) const {
            const std::filesystem::path texturePath(textureName);
            std::vector<std::filesystem::path> candidates;

            if (texturePath.is_absolute()) {
                candidates.push_back(texturePath);
            } else {
                candidates.push_back(std::filesystem::path(directory) / texturePath);
                candidates.push_back(std::filesystem::path(directory) / "textures" / texturePath.filename());
            }

            for (const std::filesystem::path& candidate : candidates) {
                if (std::filesystem::exists(candidate)) {
                    return candidate.generic_string();
                }
            }
            return "";
        }

        glm::vec3 sampleTexture(const TextureImage& texture, const glm::vec2& uv) const {
            const float u = uv.x - std::floor(uv.x);
            const float v = uv.y - std::floor(uv.y);
            const int x = std::clamp(static_cast<int>(u * texture.width), 0, texture.width - 1);
            const int y = std::clamp(static_cast<int>((1.0f - v) * texture.height), 0, texture.height - 1);
            const int offset = (y * texture.width + x) * texture.components;

            return glm::vec3(
                texture.pixels[offset + 0] / 255.0f,
                texture.pixels[offset + 1] / 255.0f,
                texture.pixels[offset + 2] / 255.0f);
        }

        unsigned int TextureFromFile(objl::Material mat, std::string directory) {
            std::string filename = std::string(mat.name);
            filename = directory + '/' + filename;

            unsigned int textureID;
            glGenTextures(1, &textureID);

            int width, height, nrComponents;
            unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
            if (data)
            {
                GLenum format;
                if (nrComponents == 1)
                    format = GL_RED;
                else if (nrComponents == 3)
                    format = GL_RGB;
                else if (nrComponents == 4)
                    format = GL_RGBA;

                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                stbi_image_free(data);
            }
            else
            {
                std::cout << "Texture failed to load at path: " << mat.name << std::endl;
                stbi_image_free(data);
            }

            return textureID;
        }
};

#endif
