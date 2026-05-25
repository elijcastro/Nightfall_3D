// ======================================================
// NIGHTFALL 3D
// PASILLO + CUARTO NUEVO DESPUES DE LA PUERTA
// ======================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Shader.h"
#include "geometry.h"
#include "stb_image.h"

#include <iostream>
#include <cstdlib>
#include <ctime>

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 1.0f, 5.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// =======================
// PUERTA
// =======================

bool doorOpen = false;
float doorAngle = 0.0f;

// =======================
// FLICKER
// =======================

float flickerTimer = 0.0f;
float lightIntensity = 1.0f;

// =======================
// CALLBACK
// =======================

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// =======================
// COLISIONES
// =======================

bool checkCollision(glm::vec3 pos)
{
    float leftWall = -1.5f;
    float rightWall = 1.5f;

    float frontLimit = -140.0f;
    float backLimit = 78.0f;

    // PASILLO

    if (pos.z > -80.0f)
    {
        if (pos.x < leftWall || pos.x > rightWall)
            return true;
    }

    // CUARTO NUEVO

    if (pos.z < -80.0f)
    {
        if (pos.x < -12.0f || pos.x > 12.0f)
            return true;

        if (pos.z < -122.0f || pos.z > -80.0f)
            return true;
    }

    // OBSTACULO PASILLO

    if (
        pos.x > -1.0f &&
        pos.x < 1.0f &&
        pos.z > -16.0f &&
        pos.z < -14.0f
        )
        return true;

    // PUERTA CERRADA

    if (!doorOpen)
    {
        if (
            pos.x > -1.2f &&
            pos.x < 1.2f &&
            pos.z > -73.0f &&
            pos.z < -71.5f
            )
            return true;
    }

    // OBSTACULOS CUARTO

    if (
        pos.x > -6.0f &&
        pos.x < -2.0f &&
        pos.z < -100.0f &&
        pos.z > -105.0f
        )
        return true;

    if (
        pos.x > 3.0f &&
        pos.x < 7.0f &&
        pos.z < -110.0f &&
        pos.z > -115.0f
        )
        return true;

    return false;
}

// =======================
// INPUT
// =======================

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

    // ABRIR PUERTA

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        if (
            camera.Position.z < -68.0f &&
            camera.Position.z > -75.0f
            )
        {
            doorOpen = true;
        }
    }

    if (checkCollision(camera.Position))
        camera.Position = oldPos;
}

// =======================
// TEXTURAS
// =======================

unsigned int loadTexture(const char* path)
{
    unsigned int textureID;

    glGenTextures(1, &textureID);

    int width, height, nrChannels;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* data =
        stbi_load(path, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = GL_RGB;

        if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            format,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            data
        );

        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            GL_REPEAT
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            GL_REPEAT
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_LINEAR_MIPMAP_LINEAR
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_LINEAR
        );

        stbi_image_free(data);
    }

    return textureID;
}

// =======================
// MAIN
// =======================

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

    GLFWwindow* window =
        glfwCreateWindow(
            SCR_WIDTH,
            SCR_HEIGHT,
            "NIGHTFALL 3D",
            NULL,
            NULL
        );

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(
        window,
        framebuffer_size_callback
    );

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    Shader lightingShader(
        "src/lighting.vert",
        "src/lighting.frag"
    );

    lightingShader.use();

    lightingShader.setInt("diffuseMap", 0);

    // =======================
    // TEXTURAS
    // =======================

    unsigned int wallTex =
        loadTexture("textures/Pared_Doom.jpg");

    unsigned int floorTex =
        loadTexture("textures/floor.jpg");

    unsigned int doorTex =
        loadTexture("textures/malla.jpg");

    // NUEVAS TEXTURAS CUARTO

    unsigned int roomWallTex =
        loadTexture("textures/laboratorio.jpg");

    unsigned int roomFloorTex =
        loadTexture("textures/subterraneo.jpg");

    unsigned int roomCeilingTex =
        loadTexture("textures/finalroom.jpg");

    // =======================
    // VAO
    // =======================

    unsigned int cubeVAO, cubeVBO;

    glGenVertexArrays(1, &cubeVAO);

    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        cubeVerticesSize,
        cubeVertices,
        GL_STATIC_DRAW
    );

    // POS

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    // NORMALS

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        (void*)(3 * sizeof(float))
    );

    glEnableVertexAttribArray(1);

    // TEXCOORDS

    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        (void*)(6 * sizeof(float))
    );

    glEnableVertexAttribArray(2);

    srand((unsigned int)time(NULL));

    // =======================
    // LOOP
    // =======================

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();

        deltaTime = currentFrame - lastFrame;

        lastFrame = currentFrame;

        processInput(window);

        // HEAD BOBBING

        camera.Position.y =
            1.0f +
            sin(glfwGetTime() * 8.0f) * 0.03f;

        // =========================
        // ILUMINACION
        // =========================

        glm::vec3 fogColor;
        glm::vec3 lightColor;

        float fogDensity;

        // PASILLO ORIGINAL

        if (camera.Position.z > -75.0f)
        {
            fogColor =
                glm::vec3(0.03f, 0.03f, 0.04f);

            fogDensity = 0.02f;

            lightColor =
                glm::vec3(1.0f, 1.0f, 1.0f);

            lightIntensity = 1.0f;
        }
        else
        {
            // NUEVO CUARTO

            fogColor =
                glm::vec3(0.01f, 0.04f, 0.02f);

            fogDensity = 0.06f;

            lightColor =
                glm::vec3(0.2f, 1.0f, 0.3f);

            lightIntensity = 0.6f;
        }

        // =========================
        // FLICKER
        // =========================

        flickerTimer += deltaTime;

        if (flickerTimer > 0.08f)
        {
            flickerTimer = 0.0f;

            float r =
                (rand() % 100) / 100.0f;

            if (r > 0.85f)
                lightIntensity *= 0.3f;
        }

        // =========================
        // CLEAR
        // =========================

        glClearColor(
            fogColor.r,
            fogColor.g,
            fogColor.b,
            1.0f
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        // =========================
        // MATRICES
        // =========================

        glm::mat4 projection =
            glm::perspective(
                glm::radians(60.0f),
                (float)SCR_WIDTH / SCR_HEIGHT,
                0.1f,
                300.0f
            );

        glm::mat4 view =
            camera.GetViewMatrix();

        // =========================
        // SHADER
        // =========================

        lightingShader.use();

        lightingShader.setMat4(
            "projection",
            projection
        );

        lightingShader.setMat4(
            "view",
            view
        );

        lightingShader.setVec3(
            "viewPos",
            camera.Position
        );

        lightingShader.setVec3(
            "fogColor",
            fogColor
        );

        lightingShader.setFloat(
            "fogDensity",
            fogDensity
        );

        lightingShader.setVec3(
            "dirLight.ambient",
            lightColor *
            (0.2f * lightIntensity)
        );

        lightingShader.setVec3(
            "dirLight.diffuse",
            lightColor *
            lightIntensity
        );

        // =========================
        // DRAW
        // =========================

        glBindVertexArray(cubeVAO);

        glActiveTexture(GL_TEXTURE0);

        glm::mat4 model;

        // =========================
        // PASILLO ORIGINAL
        // =========================

        glBindTexture(GL_TEXTURE_2D, floorTex);

        model = glm::scale(
            glm::mat4(1.0f),
            glm::vec3(20, 0.1, 80)
        );

        lightingShader.setMat4(
            "model",
            model
        );

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // PAREDES

        glBindTexture(GL_TEXTURE_2D, wallTex);

        for (int i = -40; i <= 40; i++)
        {
            // LEFT

            model = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(-2, 1.5f, i * 2)
            );

            model = glm::scale(
                model,
                glm::vec3(0.3f, 3.0f, 2.0f)
            );

            lightingShader.setMat4(
                "model",
                model
            );

            glDrawArrays(GL_TRIANGLES, 0, 36);

            // RIGHT

            model = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(2, 1.5f, i * 2)
            );

            model = glm::scale(
                model,
                glm::vec3(0.3f, 3.0f, 2.0f)
            );

            lightingShader.setMat4(
                "model",
                model
            );

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // =========================
        // OBSTACULO PASILLO
        // =========================

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(0.0f, 0.5f, -15.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(1.5f)
        );

        lightingShader.setMat4(
            "model",
            model
        );

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // =========================
        // PUERTA
        // =========================

        if (doorOpen && doorAngle < 90.0f)
            doorAngle += 1.5f;

        glBindTexture(GL_TEXTURE_2D, doorTex);

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(0.0f, 1.5f, -72.0f)
        );

        model = glm::rotate(
            model,
            glm::radians(doorAngle),
            glm::vec3(0, 1, 0)
        );

        model = glm::scale(
            model,
            glm::vec3(4.0f, 3.0f, 0.2f)
        );

        lightingShader.setMat4(
            "model",
            model
        );

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ======================================================
        // NUEVO CUARTO
        // ======================================================

        // PISO

        glBindTexture(GL_TEXTURE_2D, roomFloorTex);

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(0.0f, -0.5f, -100.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(12.0f, 0.1f, 12.0f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // TECHO

        glBindTexture(GL_TEXTURE_2D, roomCeilingTex);

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(0.0f, 4.0f, -100.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(12.0f, 0.1f, 12.0f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // PAREDES

        glBindTexture(GL_TEXTURE_2D, roomWallTex);

        // IZQUIERDA

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(-12.0f, 1.5f, -100.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(0.2f, 4.0f, 12.0f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // DERECHA

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(12.0f, 1.5f, -100.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(0.2f, 4.0f, 12.0f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // FONDO

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(0.0f, 1.5f, -112.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(12.0f, 4.0f, 0.2f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // =========================
        // OBSTACULOS CUARTO
        // =========================

        glBindTexture(GL_TEXTURE_2D, wallTex);

        // CAJA 1

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(-4.0f, 1.0f, -102.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(2.0f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        // CAJA 2

        model = glm::mat4(1.0f);

        model = glm::translate(
            model,
            glm::vec3(5.0f, 1.0f, -108.0f)
        );

        model = glm::scale(
            model,
            glm::vec3(1.5f)
        );

        lightingShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}