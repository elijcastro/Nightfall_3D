#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Shader.h"
#include "geometry.h"
#include "geometry.h"
#include "stb_image.h"

#include <iostream>
#include <cstdlib>

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 1.0f, 5.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// PUERTA
bool doorOpen = false;
float doorAngle = 0.0f;

// LUZ PARPADEO
float flickerTimer = 0.0f;
float lightIntensity = 1.0f;

// CALLBACK
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// COLISION
bool checkCollision(glm::vec3 pos)
{
    float leftWall = -1.5f;
    float rightWall = 1.5f;

    float frontLimit = -78.0f;
    float backLimit = 78.0f;

    if (pos.x < leftWall || pos.x > rightWall)
        return true;

    if (pos.z < frontLimit || pos.z > backLimit)
        return true;

    if (pos.x > -1.0f && pos.x < 1.0f &&
        pos.z > -16.0f && pos.z < -14.0f)
        return true;

    if (!doorOpen)
    {
        if (pos.x > -1.2f && pos.x < 1.2f &&
            pos.z > -73.0f && pos.z < -71.5f)
            return true;
    }

    return false;
}

// INPUT
void processInput(GLFWwindow* window)
{
    glm::vec3 oldPos = camera.Position;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        camera.ProcessMouseMovement(-2.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camera.ProcessMouseMovement(2.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        if (camera.Position.x > -2.0f && camera.Position.x < 2.0f &&
            camera.Position.z > -74.0f && camera.Position.z < -69.0f)
        {
            doorOpen = true;
        }
    }

    if (checkCollision(camera.Position))
        camera.Position = oldPos;
}

// TEXTURA
unsigned int loadTexture(const char* path)
{
    unsigned int tex;
    glGenTextures(1, &tex);

    int w, h, c;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path, &w, &h, &c, 0);

    if (!data)
    {
        std::cout << "ERROR TEXTURA: " << path << std::endl;
        return tex;
    }

    GLenum format = (c == 4) ? GL_RGBA : GL_RGB;

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}

// MAIN
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "HORROR HALLWAY", NULL, NULL);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    Shader lightingShader("src/lighting.vert", "src/lighting.frag");

    lightingShader.use();
    lightingShader.setInt("diffuseMap", 0);

    unsigned int wallTex = loadTexture("textures/Pared_Doom.jpg");
    unsigned int floorTex = loadTexture("textures/floor.jpg");
    unsigned int doorTex = loadTexture("textures/malla.jpg");

    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, cubeVerticesSize, cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    srand((unsigned)time(NULL));

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        camera.Position.y = 1.0f;

        // 🔥 FLICKER LIGHT
        flickerTimer += deltaTime;
        if (flickerTimer > 0.08f)
        {
            flickerTimer = 0.0f;
            float r = (rand() % 100) / 100.0f;
            lightIntensity = (r > 0.85f) ? 0.2f : 1.0f;
        }

        glClearColor(0.03f, 0.03f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
            (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        glm::mat4 view = camera.GetViewMatrix();

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("viewPos", camera.Position);

        // 🌫️ FOG
        lightingShader.setVec3("fogColor", glm::vec3(0.02f, 0.02f, 0.03f));
        lightingShader.setFloat("fogDensity", 0.03f);

        // 💡 LIGHT FLICKER
        lightingShader.setVec3("dirLight.ambient", glm::vec3(0.2f * lightIntensity));
        lightingShader.setVec3("dirLight.diffuse", glm::vec3(lightIntensity));

        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);

        glm::mat4 model;

        glBindTexture(GL_TEXTURE_2D, floorTex);
        model = glm::scale(glm::mat4(1.0f), glm::vec3(20, 0.1, 80));
        lightingShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindTexture(GL_TEXTURE_2D, wallTex);

        for (int i = -40; i <= 40; i++)
        {
            model = glm::translate(glm::mat4(1.0f), glm::vec3(-2, 1.5, i * 2));
            model = glm::scale(model, glm::vec3(0.3, 3, 2));
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            model = glm::translate(glm::mat4(1.0f), glm::vec3(2, 1.5, i * 2));
            model = glm::scale(model, glm::vec3(0.3, 3, 2));
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        if (doorOpen && doorAngle < 90.0f)
            doorAngle += 2.0f;

        glBindTexture(GL_TEXTURE_2D, doorTex);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 1.5f, -72.0f));
        model = glm::rotate(model, glm::radians(doorAngle), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(4.0f, 3.0f, 0.2f));

        lightingShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}