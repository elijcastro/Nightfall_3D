#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "shader.h"
#include "geometry.h"
#include "stb_image.h"
#include "Model.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

// ---------------------------------------------------------
// Colisiones
// ---------------------------------------------------------
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

bool CheckCollision(const AABB& a, const AABB& b) {
    return (a.max.x > b.min.x &&
            a.min.x < b.max.x &&
            a.max.y > b.min.y &&
            a.min.y < b.max.y &&
            a.max.z > b.min.z &&
            a.min.z < b.max.z);
}

AABB playerBox;
float playerWidth  = 0.4f;
float playerHeight = 1.8f;

std::vector<AABB> worldColliders;

// ---------------------------------------------------------
// Raycast + Decals
// ---------------------------------------------------------
struct HitInfo {
    bool hit;
    glm::vec3 position;
    glm::vec3 normal;
};

struct Decal {
    glm::vec3 pos;
    glm::vec3 normal;
};

std::vector<Decal> decals;

// ---------------------------------------------------------
// Chispas
// ---------------------------------------------------------
struct Spark {
    glm::vec3 pos;
    glm::vec3 vel;
    float life;
};

std::vector<Spark> sparks;

// ---------------------------------------------------------
// Configuración de ventana
// ---------------------------------------------------------
const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;

// ---------------------------------------------------------
// Cámara y tiempo
// ---------------------------------------------------------
Camera camera(glm::vec3(-19.0f, -5.35f, 12.60f));

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
float     gShotInterval   = 0.18f;
bool      gIsFiring       = false;

// ---------------------------------------------------------
// Animación arma
// ---------------------------------------------------------
unsigned int gGunTextures[3];
int   gCurrentGunFrame   = 0;
float gGunAnimTime       = 0.0f;
float gGunFrameDuration  = 0.05f;

//---------------------------------------------------------
// Gravedad
//---------------------------------------------------------
float playerVelocityY = 0.0f;
float gravity = -9.8f;
bool isGrounded = false;

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

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

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
        GLenum format = (nrChannels == 4 ? GL_RGBA : GL_RGB);

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
// Disparo
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

HitInfo Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist)
{
    HitInfo hit = { false, glm::vec3(0), glm::vec3(0) };
    float closest = maxDist;

    for (const auto& box : worldColliders)
    {
        float tMin = 0.0f;
        float tMax = maxDist;

        for (int i = 0; i < 3; i++)
        {
            if (std::fabs(dir[i]) < 0.0001f)
            {
                if (origin[i] < box.min[i] || origin[i] > box.max[i])
                    goto skip;
            }
            else
            {
                float ood = 1.0f / dir[i];
                float t1 = (box.min[i] - origin[i]) * ood;
                float t2 = (box.max[i] - origin[i]) * ood;

                if (t1 > t2) std::swap(t1, t2);

                tMin = t1 > tMin ? t1 : tMin;
                tMax = t2 < tMax ? t2 : tMax;

                if (tMin > tMax)
                    goto skip;
            }
        }

        if (tMin < closest)
        {
            closest = tMin;
            hit.hit = true;
            hit.position = origin + dir * tMin;

            glm::vec3 p = hit.position;

            if (std::fabs(p.x - box.min.x) < 0.01f) hit.normal = glm::vec3(-1,0,0);
            else if (std::fabs(p.x - box.max.x) < 0.01f) hit.normal = glm::vec3(1,0,0);
            else if (std::fabs(p.y - box.min.y) < 0.01f) hit.normal = glm::vec3(0,-1,0);
            else if (std::fabs(p.y - box.max.y) < 0.01f) hit.normal = glm::vec3(0,1,0);
            else if (std::fabs(p.z - box.min.z) < 0.01f) hit.normal = glm::vec3(0,0,-1);
            else if (std::fabs(p.z - box.max.z) < 0.01f) hit.normal = glm::vec3(0,0,1);
        }

        skip:;
    }

    return hit;
}

// ---------------------------------------------------------
// main
// ---------------------------------------------------------
int main()
{
    std::srand((unsigned int)std::time(nullptr));

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "NIGHTFALL_3D", NULL, NULL);
    if (!window)
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
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
    Shader decalShader("src/decal.vert", "src/decal.frag");

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
    // VAO arma
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
    // VAO para impactos (decals)
    // -----------------------------------------------------
    unsigned int decalVAO, decalVBO;
    glGenVertexArrays(1, &decalVAO);
    glGenBuffers(1, &decalVBO);

    float decalQuad[] = {
        // x, y, z,   u, v
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
    };

    glBindVertexArray(decalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, decalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(decalQuad), decalQuad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // -----------------------------------------------------
    // Texturas HUD, arma y decal
    // -----------------------------------------------------
    unsigned int hudTex = loadTexture("textures/hud.png");
    gGunTextures[0] = loadTexture("textures/Gun.gif");
    gGunTextures[1] = loadTexture("textures/Gun_shoot2.gif");
    gGunTextures[2] = loadTexture("textures/Gun_shoot1.gif");
    unsigned int bulletHoleTex = loadTexture("textures/bullet-hole.png");

    decalShader.use();
    decalShader.setInt("decalTex", 0);

    // -----------------------------------------------------
    // Luz direccional
    // -----------------------------------------------------
    lightingShader.use();
    lightingShader.setInt("texture_diffuse1", 0);
    lightingShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
    lightingShader.setVec3("dirLight.ambient",  glm::vec3(0.2f));
    lightingShader.setVec3("dirLight.diffuse",  glm::vec3(0.7f));
    lightingShader.setVec3("dirLight.specular", glm::vec3(0.5f));

    hudShader.use();
    hudShader.setInt("hudTexture", 0);

    // -----------------------------------------------------
    // Cargar GLTF
    // -----------------------------------------------------
    Model doomWorld("assets/models/Mapa1.gltf");
    Model cacodemon("assets/models/Cacodemon3.gltf");


    // ---------------------------------------------------------
    // Colliders manuales del mapa
    // ---------------------------------------------------------
    worldColliders.clear();

    // Piso general
    worldColliders.push_back({
        glm::vec3(-200.0f, -7.5f, -200.0f),
        glm::vec3( 200.0f, -5.5f,  200.0f)
    });

    // Pared incial
    worldColliders.push_back({
        glm::vec3(-25.0f, 0.0f, 18.0f),
        glm::vec3(-20.40f, 4.49f, 20.0f)
    });

    // Pared lateral del pasillo derecho
    worldColliders.push_back({
        glm::vec3(-9.59f, 0.0f, 17.0f),
        glm::vec3(-19.07f, 6.0f, 18.51f)
    });

    // Pared lateral del pasillo izquierdo
    worldColliders.push_back({
        glm::vec3(-9.59f, 0.0f, 17.0f),
        glm::vec3(-19.07f, 6.0f, 18.51f)
    });

    // Escalón/borde alto pasillo derecho
    worldColliders.push_back({
        glm::vec3(-6.45f, 0.0f, 16.98f),
        glm::vec3(-9.80f,  1.30f, 16.98f)
    });

    // Puerta de entrada cerrada
    worldColliders.push_back({
        glm::vec3(-22.0f, 0.0f, 21.0f),
        glm::vec3(-18.0f, 3.0f, 22.0f)
    });

    // -----------------------------------------------------
    // Audio
    // -----------------------------------------------------
    if (ma_engine_init(NULL, &gEngine) != MA_SUCCESS)
    {
        std::cout << "Failed to initialize miniaudio engine\n";
        return -1;
    }

    ma_sound bgm;
    if (ma_sound_init_from_file(&gEngine, "sonidos/cdoomtheme.ogg",
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
        NULL, NULL, &bgm) == MA_SUCCESS)
    {
        ma_sound_set_looping(&bgm, MA_TRUE);
        ma_sound_start(&bgm);
    }

    //---------------------------------------------------------
    //Sonido Cacodemon
    //---------------------------------------------------------
ma_sound cacoSound;
if (ma_sound_init_from_file(&gEngine, "sonidos/Cacodemon_sonido1.wav",
    MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
    NULL, NULL, &cacoSound) == MA_SUCCESS)
{
    ma_sound_set_looping(&cacoSound, MA_FALSE); // no repetir automáticamente
    ma_sound_set_volume(&cacoSound, 0.3f);      // volumen bajo
}

// Dentro del bucle principal:
if (fmod(glfwGetTime(), 10.0f) < 0.1f) { // cada ~10 segundos
    ma_sound_start(&cacoSound);
}



    lastFrame = (float)glfwGetTime();

    // -----------------------------------------------------
    // Bucle principal
    // -----------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glm::vec3 oldPos = camera.Position;

        // Aplicar gravedad
        playerVelocityY += gravity * deltaTime;
        camera.Position.y += playerVelocityY * deltaTime;

        processInput(window);

        // AABB del jugador
        playerBox.min = camera.Position - glm::vec3(playerWidth, 0.0f, playerWidth);
        playerBox.max = camera.Position + glm::vec3(playerWidth, playerHeight, playerWidth);

        // Colisión jugador vs colliders
        for (const auto& box : worldColliders)
        {
            if (CheckCollision(playerBox, box))
            {
                if (playerVelocityY < 0.0f)
                {
                    isGrounded = true;
                    playerVelocityY = 0.0f;
                    camera.Position.y = oldPos.y;
                }
                else
                {
                    camera.Position = oldPos;
                }
            }
        }

        if (!isGrounded)
        {
            // sigue cayendo naturalmente
        }
        else
        {
            isGrounded = false;
        }

        // Disparo + decals + chispas
        if (gIsFiring)
        {
            playGunshot();

            glm::vec3 origin = camera.Position;
            glm::vec3 dir = camera.Front;

            HitInfo hit = Raycast(origin, dir, 100.0f);

            if (hit.hit)
            {
                // Decal
                decals.push_back({ hit.position, hit.normal });

                // Chispas
                for (int i = 0; i < 15; ++i)
                {
                    glm::vec3 randDir(
                        ((std::rand() / (float)RAND_MAX) - 0.5f),
                        ((std::rand() / (float)RAND_MAX)),
                        ((std::rand() / (float)RAND_MAX) - 0.5f)
                    );
                    randDir = glm::normalize(randDir + hit.normal * 0.5f);

                    float speed = 5.0f + (std::rand() / (float)RAND_MAX) * 5.0f;
                    float life  = 0.2f + (std::rand() / (float)RAND_MAX) * 0.3f;

                    Spark s;
                    s.pos  = hit.position + hit.normal * 0.02f;
                    s.vel  = randDir * speed;
                    s.life = life;

                    sparks.push_back(s);
                }
            }
        }

        // Animación arma
        if (gIsFiring)
        {
            gGunAnimTime += deltaTime;
            if (gGunAnimTime >= gGunFrameDuration)
            {
                gGunAnimTime = 0.0f;
                gCurrentGunFrame = (gCurrentGunFrame == 1 ? 2 : 1);
            }
        }
        else
        {
            gCurrentGunFrame = 0;
            gGunAnimTime = 0.0f;
        }

        // Actualizar chispas
        for (size_t i = 0; i < sparks.size();)
        {
            Spark& s = sparks[i];
            s.life -= deltaTime;
            s.vel += glm::vec3(0.0f, gravity * 0.5f, 0.0f) * deltaTime;
            s.pos += s.vel * deltaTime;

            if (s.life <= 0.0f)
                sparks.erase(sparks.begin() + i);
            else
                ++i;
        }

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);

        glm::mat4 view = camera.GetViewMatrix();

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("viewPos", camera.Position);

        // -------------------------------------------------
        // DIBUJAR MUNDO GLTF
        // -------------------------------------------------
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -7.15f, -5.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
        model = glm::scale(model, glm::vec3(0.02f));

        lightingShader.setMat4("model", model);
        lightingShader.setFloat("texScale", 1.0f);

        doomWorld.Draw(lightingShader);
        
        // -------------------------------------------------
        // DIBUJAR Cacodemon GLTF
        //---------------------------------------------
        float time = glfwGetTime();
        glm::mat4 animCaco = glm::mat4(1.0f); 
        animCaco = glm::translate(animCaco, glm::vec3(-19.0f, -5.35f, -6.60f)); // posición dentro del mapa
        animCaco = glm::scale(animCaco, glm::vec3(0.5f));
        animCaco = glm::translate(animCaco, glm::vec3(0.0f, sin(time) * 1.0f, 0.0f)); // flotar
        animCaco = glm::rotate(animCaco, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // rotar
       
        lightingShader.setMat4("model", animCaco);
        lightingShader.setFloat("texScale", 1.0f);

        cacodemon.Draw(lightingShader);



        // -------------------------------------------------
        // DIBUJAR DECALS (agujeros de bala)
        // -------------------------------------------------
        decalShader.use();
        decalShader.setMat4("view", view);
        decalShader.setMat4("projection", projection);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(decalVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bulletHoleTex);

        for (auto& d : decals)
        {
            glm::mat4 m = glm::mat4(1.0f);
            m = glm::translate(m, d.pos + d.normal * 0.01f);

            if (d.normal.x != 0) m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0,1,0));
            if (d.normal.y != 0) m = glm::rotate(m, glm::radians(90.0f), glm::vec3(1,0,0));

            m = glm::scale(m, glm::vec3(0.25f));

            decalShader.setMat4("model", m);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // -------------------------------------------------
        // DIBUJAR CHISPAS (reutilizando el mismo quad/texture, muy pequeñas)
        // -------------------------------------------------
        for (auto& s : sparks)
        {
            glm::mat4 m = glm::mat4(1.0f);
            m = glm::translate(m, s.pos);
            m = glm::scale(m, glm::vec3(0.05f));

            decalShader.setMat4("model", m);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glDisable(GL_BLEND);

        // -------------------------------------------------
        // HUD + arma
        // -------------------------------------------------
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        hudShader.use();

        glBindVertexArray(gunVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gGunTextures[gCurrentGunFrame]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(hudVAO);
        glBindTexture(GL_TEXTURE_2D, hudTex);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ma_sound_uninit(&bgm);
    ma_engine_uninit(&gEngine);

    glfwTerminate();
    return 0;
}
