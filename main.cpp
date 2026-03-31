#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stb_image.h"
#include "OBJ_Loader.h"

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
void processInput(GLFWwindow *window, Camera &cam, float deltaTime) {
    // esc key to exit
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    // camera movement
    const float cameraSpeed = 2.5f * deltaTime;
    cam.pos_update(window, deltaTime);
}

// process the scroll
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// process the mouse position
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

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

// Viriables
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

    // glad: load all OpenGl function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
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

        // Process the inputs
        // ------------------
        processInput(window, cam, deltaTime);

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

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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