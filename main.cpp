#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stb_image.h"
#include "OBJ_Loader.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "portable-file-dialogs.h"

#include "shader.hpp"
#include "model.hpp"
#include "camera.hpp"

#include <iostream>

// Callbacks
// ---------

// callback to resize a window
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// process the inputs
void processInput(GLFWwindow *window, Camera &cam, Model &model, float deltaTime) {
    // esc key to exit
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    static bool mode1PressedLastFrame = false;
    static bool mode2PressedLastFrame = false;
    static bool mode3PressedLastFrame = false;
    static bool mode4PressedLastFrame = false;
    static bool mode5PressedLastFrame = false;
    static bool resetPressedLastFrame = false;

    const bool mode1PressedNow = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    const bool mode2PressedNow = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    const bool mode3PressedNow = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
    const bool mode4PressedNow = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
    const bool mode5PressedNow = glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS;
    const bool resetPressedNow = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

    if (mode1PressedNow && !mode1PressedLastFrame) {
        model.setSimplificationMode(SimplificationMode::Original);
        std::cout << "Mode: " << model.currentModeName() << std::endl;
    }
    if (mode2PressedNow && !mode2PressedLastFrame) {
        model.setSimplificationMode(SimplificationMode::Random);
        std::cout << "Mode: " << model.currentModeName() << std::endl;
    }
    if (mode3PressedNow && !mode3PressedLastFrame) {
        model.setSimplificationMode(SimplificationMode::RandomLegal);
        std::cout << "Mode: " << model.currentModeName() << std::endl;
    }
    if (mode4PressedNow && !mode4PressedLastFrame) {
        model.setSimplificationMode(SimplificationMode::ShortestLegal);
        std::cout << "Mode: " << model.currentModeName() << std::endl;
    }
    if (mode5PressedNow && !mode5PressedLastFrame) {
        model.setSimplificationMode(SimplificationMode::LowestLegalQError);
        std::cout << "Mode: " << model.currentModeName() << std::endl;
    }
    if (resetPressedNow && !resetPressedLastFrame) {
        model.resetSimplification();
        std::cout << "Reset current mesh in mode: " << model.currentModeName() << std::endl;
    }

    mode1PressedLastFrame = mode1PressedNow;
    mode2PressedLastFrame = mode2PressedNow;
    mode3PressedLastFrame = mode3PressedNow;
    mode4PressedLastFrame = mode4PressedNow;
    mode5PressedLastFrame = mode5PressedNow;
    resetPressedLastFrame = resetPressedNow;

    static bool decimatePressedLastFrame = false;
    const bool decimatePressedNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    if (decimatePressedNow && !decimatePressedLastFrame) {
        // queue 10% collapse
        model.queueCollapseBatch(static_cast<int>(model.activeTriangleCount() * 0.1f));
        std::cout << "Queued 10% collapses. Pending: "
                  << model.pendingCollapseCount()
                  << " | mode: " << model.currentModeName()
                  << " | faces: " << model.activeTriangleCount()
                  << " | edges: " << model.activeEdgeCount()
                  << std::endl;
    }
    decimatePressedLastFrame = decimatePressedNow;

    // camera movement
    const float cameraSpeed = 2.5f * deltaTime;
    cam.pos_update(window, deltaTime);
}

// process the scroll
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// process the mouse position
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// process the mouse button
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// Settings
// --------

// window settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera settings
glm::vec3 camPos = glm::vec3(0.0f, 0.2f, 1.0f);
glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camUp = glm::vec3(0.0, 1.0f, 0.0f);
float camFov = 45.0f;
float camSpeed = 2.5f;

// light position
glm::vec3 lightPos(0.2f, 0.5f, 1.5f);

// Variables
// ---------
// model transformation matrix of the imported model
glm::mat4 model = glm::mat4(1.0f);

// times
float deltaTime = 0.0f; // time between the current frame and the last frame
float lastFrame = 0.0f; // time of the last frame


// Default components
// ------------------

// model pointer
Model* ourModelPtr = nullptr;

// camera
Camera cam(camPos, camFront, camUp, camFov, camSpeed, SCR_WIDTH, SCR_HEIGHT);


int main () {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw: window creation
    // ---------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "MeshSimplification", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Poll events
    // -----------
    // display GLFW window with current resolution
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450 core");

    // glad: load all OpenGl function pointers
    // ---------------------------------------
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // Build and compile shaders
    Shader modelShader("model_vs.txt", "model_fs.txt");
    Shader lightShader("model_vs.txt", "light_fs.txt");

    // Load the model
    // --------------
    Model ourModel("models/bunny_40k.obj");
    ourModelPtr = &ourModel;

    // Render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // Calculate the times
        // -------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ImGui: new frame
        // ----------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Process the inputs
        // ------------------
        processInput(window, cam, ourModel, deltaTime);
        ourModel.processPendingCollapses(5);

        // Render
        // ------
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // lookAt matrix
        glm::mat4 view = glm::lookAt(cam.get_cameraPos(), cam.get_cameraPos() + cam.get_cameraFront(), cam.get_cameraUp());
        glm::mat4 projection = glm::perspective(glm::radians(cam.get_fov()), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // set the model shader uniforms
        modelShader.use();
        // view
        unsigned int viewLoc = glGetUniformLocation(modelShader.ID, "view");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // projection
        unsigned int projectionLoc = glGetUniformLocation(modelShader.ID, "projection");
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        // light position
        modelShader.setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
        // camera position
        glm::vec3 camPos = cam.get_cameraPos();
        modelShader.setVec3("viewPos", camPos.x, camPos.y, camPos.z);
        //modelShader.setVec3("lightPos", camPos.x, camPos.y, camPos.z);
        // object color
        modelShader.setVec3("objectColor", 1.0f, 1.0f, 1.0f);
        // light color
        modelShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);

        // render the loaded model
        unsigned int modelLoc = glGetUniformLocation(modelShader.ID, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        ourModel.Draw(modelShader);


        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("context_menu");
        }

        if (ImGui::BeginPopup("context_menu")) {
            if (ImGui::BeginMenu("Load")) {
                const char* bundledModels[] = {
                    "models/bunny_200.obj",
                    "models/bunny_1k.obj",
                    "models/bunny_40k.obj",
                    "models/complex.obj",
                    "models/creased_cube.obj",
                    "models/cube.obj",
                    "models/l.obj",
                    "models/open_box.obj",
                    "models/torus.obj"
                };

                for (const char* modelPath : bundledModels) {
                    if (ImGui::MenuItem(modelPath)) {
                        ourModel.loadFromPath(modelPath);
                        ourModelPtr = &ourModel;
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Open file dialog")) {
                    auto dialog = pfd::open_file("Select a 3D Model", "./models", {"OBJ Files", "*.obj"});
                    if (!dialog.result().empty()) {
                        std::string newFilePath = dialog.result()[0];
                        ourModel.loadFromPath(newFilePath);
                        ourModelPtr = &ourModel;
                    }
                }

                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        // ImGui window with progress bar
        ImGui::Begin("Simplification Progress");
        ImGui::SetWindowSize(ImVec2(200, 85));
        ImGui::Text("Pending collapses: %d", ourModel.pendingCollapseCount());
        ImGui::Text("Active faces: %d", ourModel.activeTriangleCount());
        ImGui::Text("Active edges: %d", ourModel.activeEdgeCount());
        ImGui::End();


        ImGui::Render();
        // ImGui: render ImGui
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ImGui: cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // glfw: terminate, clearing all previously allocated GLFW resources
    // -----------------------------------------------------------------
    glfwTerminate();
    return 0;

};

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cam.scroll_callback(window, xoffset, yoffset, cam.dist_origin());
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    cam.mouse_callback(window, xposIn, yposIn, model, *ourModelPtr);
};

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{   
    // handle left mouse button
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        cam.leftMousePressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        cam.leftMousePressed = false;
    }
}
