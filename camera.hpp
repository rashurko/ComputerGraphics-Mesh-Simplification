#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

class Camera {
    private:
        glm::vec3 camPos;
        glm::vec3 camFront;
        glm::vec3 camUp;

        glm::vec3 zoomVec;
        glm::vec2 cursorPos;

        bool firstMouse; // gives whether it's the first time the mouse movement is recorded
        float yaw;
        float pitch;
        float lastX;
        float lastY;
        float fov;
        float speed;

    public:
        bool leftMousePressed;

        Camera(glm::vec3 camPos, glm::vec3 camFront, glm::vec3 camUp, float fov, float speed, const unsigned int screenWidth, const unsigned int screenHeight) {
            this->camPos = camPos;
            this->camFront = camFront;
            this->camUp = camUp;
            
            this->firstMouse = true;
            this->leftMousePressed = false;
            this->yaw = -90.0f;
            this->pitch = 0.0f;
            this->lastX = screenWidth / 2.0;
            this->lastY = screenHeight / 2.0;
            this->fov = fov;
            this->speed = speed;
        }
        // whenever the mouse moves, the callback is called
        void mouse_callback(GLFWwindow* window, double xposIn, double yposIn, glm::mat4 &model, Model &ourModel) {
            float xpos = static_cast<float>(xposIn);
            float ypos = static_cast<float>(yposIn);

            if (this->firstMouse)
            {
                this->lastX = xpos;
                this->lastY = ypos;
                this->cursorPos = glm::vec2(xpos, ypos);
                this->firstMouse = false;
            }

            // Zooming
            // -------
            float xoffset = xpos - lastX;
            float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
            this->lastX = xpos;
            this->lastY = ypos;
            this->cursorPos = glm::vec2(xpos, ypos);

            // cursor position in the camera coordinates
            float radius = 2.0f;
            float normalizedX = (this->cursorPos.x / 800.0f) * 2.0f - 1.0f;
            float normalizedY = 1.0f - (this->cursorPos.y / 600.0f) * 2.0f;
            float normalizedZ = sqrt(radius * radius - normalizedX * normalizedX - normalizedY * normalizedY);

            this->zoomVec = glm::vec3(normalizedX, normalizedY, -normalizedZ);
            this->zoomVec = glm::normalize(this->zoomVec);


            // Rotation of the model
            // ---------------------
            if (this->leftMousePressed) {
                float sensitivity = 0.3f;
                xoffset *= sensitivity;
                yoffset *= sensitivity;

                model = glm::rotate(model, glm::radians(xoffset), ourModel.getLocalY()); // rotate around the y-axis
                model = glm::rotate(model, -glm::radians(yoffset), ourModel.getLocalX()); // rotate around the x-axis

                glm::mat4 rotLocal = glm::mat4(1.0f);
                rotLocal = glm::rotate(rotLocal, -glm::radians(xoffset), ourModel.getLocalY());
                rotLocal = glm::rotate(rotLocal, glm::radians(yoffset), ourModel.getLocalX());

                // change the local axes
                ourModel.setLocalX(glm::normalize(glm::vec3(rotLocal * glm::vec4(ourModel.getLocalX(), 1.0f))));
                ourModel.setLocalY(glm::normalize(glm::vec3(rotLocal * glm::vec4(ourModel.getLocalY(), 1.0f))));
            }
        }
        // whenever user scrolls the scroll wheel the camera "zooms" in/out
        void scroll_callback(GLFWwindow* window, double xoffset, double yoffset, float distance) {
            float magnitude = 0.0;
            if (yoffset < 0) {
                magnitude = -log10(distance + 1);
                if (distance > 10.0) {
                    magnitude = 0.0;
                }
            } else if (yoffset > 0) {
                magnitude = log10(distance + 0.5);
                if (distance < 0.1) {
                    magnitude = 0.0;
                }
            }
            this->camPos += magnitude * this->zoomVec;
        }
        // wasd movement
        void pos_update(GLFWwindow* window, float deltaTime) {
            float distance = this->speed * deltaTime;

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                this->camPos += distance * this->camUp;
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                this->camPos -= distance * this->camUp;
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                this->camPos -= glm::normalize(glm::cross(this->camFront, this->camUp)) * distance;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                this->camPos += glm::normalize(glm::cross(this->camFront, this->camUp)) * distance;
            }
        }

        glm::vec3 get_cameraPos() const {
            return this->camPos;
        }
        glm::vec3 get_cameraFront() const {
            return this->camFront;
        }
        glm::vec3 get_cameraUp() const {
            return this->camUp;
        }

        float get_fov() const {
            return this->fov;
        }

        float dist_origin() const {
            return glm::length(this->camPos);
        }
};

#endif