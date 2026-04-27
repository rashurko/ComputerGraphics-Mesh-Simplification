#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>
#include <cmath>
#include <cstring>
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
        Model(std::string path) {
            loadModel(path);
        }

        void Draw(Shader &shader) {
            for (unsigned int i = 0; i < meshes.size(); i++) {
                meshes[i].Draw(shader);
            }
        }

        void loadFromPath(const std::string& path) {
            loadModel(path);
        }

        void setSimplificationMode(SimplificationMode mode) {
            simplifier.setMode(mode);
            pendingCollapses = 0;
            rebuildRenderMeshesFromCurrentTopology();
        }

        void resetSimplification() {
            simplifier.reset();
            pendingCollapses = 0;
            rebuildRenderMeshesFromCurrentTopology();
        }

        void queueCollapseBatch(int steps) {
            if (steps > 0) {
                pendingCollapses += steps;
            }
        }

        bool processPendingCollapses(int maxStepsPerFrame) {
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

            if (changed) {
                rebuildRenderMeshesFromCurrentTopology();
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
        
    private:
        // model data
        std::vector<Mesh> meshes;
        std::string directory;
        TopologyMesh originalTopology;
        SimplificationController simplifier;
        int pendingCollapses = 0;

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

        void loadModel(std::string path) {
            meshes.clear();
            originalTopology.clear();
            pendingCollapses = 0;
            std::unordered_map<PositionKey, int, PositionKeyHasher> topologyVertexMap;

            // Initialize loader
            objl::Loader loader;

            // Load .obj file
            bool isLoaded = loader.LoadFile(path);

            // Check if the file is properly loaded
            if (isLoaded) {

                // Go through each loaded mesh
                for (int i = 0; i < loader.LoadedMeshes.size(); i++) {
                    // copy the current mesh
                    objl::Mesh curMesh = loader.LoadedMeshes[i];

                    for (int j = 0; j + 2 < curMesh.Indices.size(); j += 3) {
                        int topoFace[3];

                        for (int k = 0; k < 3; ++k) {
                            const objl::Vertex& sourceVertex = curMesh.Vertices[curMesh.Indices[j + k]];
                            const glm::vec3 position(
                                sourceVertex.Position.X,
                                sourceVertex.Position.Y,
                                sourceVertex.Position.Z);

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

                        originalTopology.addFace(
                            topoFace[0],
                            topoFace[1],
                            topoFace[2]);
                    }
                }

            } else {
                std::cout << "Failed to load " << path << std::endl;
            }

            originalTopology.buildAdjacency();
            simplifier.setOriginalMesh(originalTopology);
            simplifier.setMode(SimplificationMode::RandomLegal);
            rebuildRenderMeshesFromCurrentTopology();

            // Declare local axes
            localX = glm::vec3(1.0f, 0.0f, 0.0f);
            localY = glm::vec3(0.0f, 1.0f, 0.0f);
            localZ = glm::vec3(0.0f, 0.0f, 1.0f);
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
                renderVertices.push_back(vertex);
            }

            meshes.emplace_back(renderVertices, renderData.indices, std::vector<Texture>{});
        }

        PositionKey makePositionKey(const glm::vec3& position) const {
            PositionKey key;
            std::memcpy(&key.x, &position.x, sizeof(float));
            std::memcpy(&key.y, &position.y, sizeof(float));
            std::memcpy(&key.z, &position.z, sizeof(float));
            return key;
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
