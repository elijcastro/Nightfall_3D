#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Shader.h"
#include "geometry.h"
#include "stb_image.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>

// ---------------------------------------------------------
// Configuración de ventana
// ---------------------------------------------------------
const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;

// ---------------------------------------------------------
// Cámara y tiempo
// ---------------------------------------------------------
Camera camera(glm::vec3(0.0f, 1.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool  firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---------------------------------------------------------
// Audio (miniaudio)
// ---------------------------------------------------------
ma_engine gEngine;
float     gLastShotTime   = 0.0f;
float     gShotInterval   = 0.18f; // tiempo entre disparos
bool      gIsFiring       = false;

// ---------------------------------------------------------
// Animación arma
// ---------------------------------------------------------
unsigned int gGunTextures[3]; // 0: idle (Gun.gif), 1: shoot1, 2: shoot2
int   gCurrentGunFrame   = 0;
float gGunAnimTime       = 0.0f;
float gGunFrameDuration  = 0.05f; // tiempo por frame

// ---------------------------------------------------------
// Callbacks
// ---------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Rotación con flechas (como ya lo tenías)
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        camera.ProcessMouseMovement(-2.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camera.ProcessMouseMovement(2.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        camera.ProcessMouseMovement(0.0f, 2.0f);

    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        camera.ProcessMouseMovement(0.0f, -2.0f);

    // Movimiento
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);

    // Disparo continuo con tecla Z
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        gIsFiring = true;
    else
        gIsFiring = false;
}

// ---------------------------------------------------------
// Carga de texturas
// ---------------------------------------------------------
unsigned int loadTexture(const char* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrChannels == 1)      format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else
    {
        std::cout << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// ---------------------------------------------------------
// Función para reproducir disparo (con rate limit)
// ---------------------------------------------------------
void playGunshot()
{
    float currentTime = (float)glfwGetTime();
    if (currentTime - gLastShotTime >= gShotInterval)
    {
        ma_engine_play_sound(&gEngine, "sonidos/Gunshot.wav", NULL);
        gLastShotTime = currentTime;
    }
}

// ---------------------------------------------------------
// main
// ---------------------------------------------------------
int main()
{
    // -----------------------------------------------------
    // GLFW / OpenGL
    // -----------------------------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "NIGHTFALL_3D", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    // Centrar ventana
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    int xpos = (mode->width  - SCR_WIDTH)  / 2;
    int ypos = (mode->height - SCR_HEIGHT) / 2;
    glfwSetWindowPos(window, xpos, ypos);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Si quieres volver al mouse libre, descomenta:
    // glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // -----------------------------------------------------
    // Shaders
    // -----------------------------------------------------
    Shader lightingShader("src/lighting.vert", "src/lighting.frag");
    Shader hudShader("src/hud.vert", "src/hud.frag");

    // -----------------------------------------------------
    // VAO/VBO cubo (mundo 3D)
    // -----------------------------------------------------
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, cubeVerticesSize, cubeVertices, GL_STATIC_DRAW);

    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texcoords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // -----------------------------------------------------
    // VAO HUD
    // -----------------------------------------------------
    unsigned int hudVAO, hudVBO;
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);

    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glBufferData(GL_ARRAY_BUFFER, hudQuadSize, hudQuad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // -----------------------------------------------------
    // VAO arma (quad en pantalla)
    // -----------------------------------------------------
    unsigned int gunVAO, gunVBO;
    glGenVertexArrays(1, &gunVAO);
    glGenBuffers(1, &gunVBO);

    glBindVertexArray(gunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gunVBO);
    glBufferData(GL_ARRAY_BUFFER, gunQuadSize, gunQuad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // -----------------------------------------------------
    // Texturas
    // -----------------------------------------------------
    unsigned int wallTex   = loadTexture("textures/Pared_Doom.jpg");
    unsigned int floorTex  = loadTexture("textures/floor.jpg");
    unsigned int hudTex    = loadTexture("textures/hud.png");

    // Frames del arma
    gGunTextures[0] = loadTexture("textures/Gun.gif");         // idle
    gGunTextures[1] = loadTexture("textures/Gun_shoot2.gif");  // disparo 1
    gGunTextures[2] = loadTexture("textures/Gun_shoot1.gif");  // disparo 2

    // -----------------------------------------------------
    // Configuración de luces
    // -----------------------------------------------------
    lightingShader.use();
    lightingShader.setInt("diffuseMap", 0);
    lightingShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
    lightingShader.setVec3("dirLight.ambient",  glm::vec3(0.2f));
    lightingShader.setVec3("dirLight.diffuse",  glm::vec3(0.7f));
    lightingShader.setVec3("dirLight.specular", glm::vec3(0.5f));

    hudShader.use();
    hudShader.setInt("hudTexture", 0);

    // -----------------------------------------------------
    // Audio: inicializar miniaudio
    // -----------------------------------------------------
    if (ma_engine_init(NULL, &gEngine) != MA_SUCCESS)
    {
        std::cout << "Failed to initialize miniaudio engine\n";
        return -1;
    }

    // Sonido ambiente Doom en loop
    ma_sound bgm;
    if (ma_sound_init_from_file(&gEngine, "sonidos/cdoomtheme.ogg",
                                MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
                                NULL, NULL, &bgm) == MA_SUCCESS)
    {
        ma_sound_set_looping(&bgm, MA_TRUE);
        ma_sound_start(&bgm);
    }
    else
    {
        std::cout << "Failed to load ambient sound: sonidos/cdoomtheme.ogg\n";
    }

    // -----------------------------------------------------
    // Posiciones de pilares
    // Primer pilar: 0.0f, 1.0f, -8.0f (como pediste)
    // Otros dos distribuidos a lo largo del pasillo
    // -----------------------------------------------------
    glm::vec3 pillarPositions[] = {
        glm::vec3(0.0f, 1.0f, -8.0f),
        glm::vec3(0.0f, 1.0f, -16.0f),
        glm::vec3(0.0f, 1.0f,  0.0f)
    };

    // -----------------------------------------------------
    // Bucle principal
    // -----------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Si se está disparando, reproducir sonido con rate limit
        if (gIsFiring)
            playGunshot();

        // Actualizar animación del arma
        if (gIsFiring)
        {
            gGunAnimTime += deltaTime;
            if (gGunAnimTime >= gGunFrameDuration)
            {
                gGunAnimTime = 0.0f;
                // Ciclo entre 1 y 2 mientras dispara
                if (gCurrentGunFrame == 0)
                    gCurrentGunFrame = 1;
                else if (gCurrentGunFrame == 1)
                    gCurrentGunFrame = 2;
                else
                    gCurrentGunFrame = 1;
            }
        }
        else
        {
            // No disparando: frame idle
            gCurrentGunFrame = 0;
            gGunAnimTime = 0.0f;
        }

        // -------------------------------------------------
        // Render
        // -------------------------------------------------
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
                                                (float)SCR_WIDTH / (float)SCR_HEIGHT,
                                                0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("viewPos", camera.Position);

        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);

        // Piso
        glBindTexture(GL_TEXTURE_2D, floorTex);
        lightingShader.setFloat("texScale", 10.0f);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(20.0f, 0.1f, 40.0f));
        lightingShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Techo (misma escala que el piso, misma textura de pared, a altura de paredes)
        glBindTexture(GL_TEXTURE_2D, wallTex);
        lightingShader.setFloat("texScale", 10.0f);
        model = glm::mat4(1.0f);
        // paredes: y ~1, escala y=3 => altura ~2.5, ponemos techo un poco arriba
        model = glm::translate(model, glm::vec3(0.0f, 2.9f, 0.0f));
        model = glm::scale(model, glm::vec3(20.0f, 0.1f, 40.0f));
        // opcional: voltear para que normales apunten hacia abajo
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        lightingShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Paredes pasillo
        glBindTexture(GL_TEXTURE_2D, wallTex);
        lightingShader.setFloat("texScale", 1.0f);
        for (int i = -10; i <= 10; ++i)
        {
            // izquierda
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-3.0f, 1.0f, (float)i * 2.0f));
            model = glm::scale(model, glm::vec3(0.5f, 3.0f, 2.0f));
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // derecha
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(3.0f, 1.0f, (float)i * 2.0f));
            model = glm::scale(model, glm::vec3(0.5f, 3.0f, 2.0f));
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Pilares
        glBindTexture(GL_TEXTURE_2D, wallTex);
        for (int i = 0; i < 3; ++i)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, pillarPositions[i]);
            model = glm::scale(model, glm::vec3(1.0f, 2.0f, 1.0f));
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // -------------------------------------------------
        // HUD + arma
        // -------------------------------------------------
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        hudShader.use();

        // Arma
        glBindVertexArray(gunVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gGunTextures[gCurrentGunFrame]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // HUD
        glBindVertexArray(hudVAO);
        glBindTexture(GL_TEXTURE_2D, hudTex);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // -----------------------------------------------------
    // Limpieza
    // -----------------------------------------------------
    ma_sound_uninit(&bgm);
    ma_engine_uninit(&gEngine);

    glfwTerminate();
    return 0;
}
