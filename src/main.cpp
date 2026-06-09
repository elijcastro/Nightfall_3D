#define NOMINMAX
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
#include <algorithm>
#include <limits>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

// ---------------------------------------------------------
// Colisiones
// ---------------------------------------------------------
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

bool CheckCollision(const AABB& a, const AABB& b)
{
    return (a.max.x > b.min.x &&
            a.min.x < b.max.x &&
            a.max.y > b.min.y &&
            a.min.y < b.max.y &&
            a.max.z > b.min.z &&
            a.min.z < b.max.z);
}

struct DoorModel {
    glm::vec3 center;
    glm::vec3 size;
    unsigned int texture;
    float texScale;
};

const float playerRadius = 0.40f;
const float playerEyeHeight = 1.75f;
const float playerHeadClearance = 0.20f;
const float floorY = -7.15f;

AABB playerBox;

std::vector<AABB> worldColliders;
std::vector<AABB> floorColliders;
std::vector<DoorModel> doorModels;

AABB MakeAABB(const glm::vec3& a, const glm::vec3& b)
{
    return { glm::min(a, b), glm::max(a, b) };
}

AABB MakeAABBFromCenter(const glm::vec3& center, const glm::vec3& size)
{
    glm::vec3 halfSize = size * 0.5f;
    return MakeAABB(center - halfSize, center + halfSize);
}

AABB MakePlayerBox(const glm::vec3& position)
{
    return {
        glm::vec3(position.x - playerRadius, position.y - playerEyeHeight, position.z - playerRadius),
        glm::vec3(position.x + playerRadius, position.y + playerHeadClearance, position.z + playerRadius)
    };
}

bool CollidesWithWorld(const glm::vec3& position)
{
    AABB candidate = MakePlayerBox(position);
    for (const auto& box : worldColliders)
    {
        if (CheckCollision(candidate, box))
            return true;
    }

    return false;
}

void AddWorldCollider(const glm::vec3& center, const glm::vec3& size)
{
    worldColliders.push_back(MakeAABBFromCenter(center, size));
}

void AddFloorCollider(const glm::vec3& center, const glm::vec3& size)
{
    AABB box = MakeAABBFromCenter(center, size);
    floorColliders.push_back(box);
    worldColliders.push_back(box);
}

void AddDoorModel(const glm::vec3& center, const glm::vec3& size, unsigned int texture, float texScale = 1.0f)
{
    doorModels.push_back({ center, size, texture, texScale });
    worldColliders.push_back(MakeAABBFromCenter(center, size));
}

void ExpandThinWall(AABB& box, float minThickness)
{
    glm::vec3 size = box.max - box.min;

    if (size.x < minThickness)
    {
        float padding = (minThickness - size.x) * 0.5f;
        box.min.x -= padding;
        box.max.x += padding;
    }

    if (size.z < minThickness)
    {
        float padding = (minThickness - size.z) * 0.5f;
        box.min.z -= padding;
        box.max.z += padding;
    }
}

void AddModelWallColliders(const Model& model, const glm::mat4& transform)
{
    const float minWallHeight = 0.75f;
    const float maxWallThickness = 0.45f;
    const float colliderThickness = 0.35f;

    for (const auto& mesh : model.meshes)
    {
        if (mesh.vertices.empty())
            continue;

        glm::vec3 minPoint(std::numeric_limits<float>::max());
        glm::vec3 maxPoint(std::numeric_limits<float>::lowest());

        for (const auto& vertex : mesh.vertices)
        {
            glm::vec3 worldPos = glm::vec3(transform * glm::vec4(vertex.Position, 1.0f));
            minPoint = glm::min(minPoint, worldPos);
            maxPoint = glm::max(maxPoint, worldPos);
        }

        AABB box = MakeAABB(minPoint, maxPoint);
        glm::vec3 size = box.max - box.min;
        bool thinOnX = size.x <= maxWallThickness && size.z >= maxWallThickness;
        bool thinOnZ = size.z <= maxWallThickness && size.x >= maxWallThickness;

        if (size.y >= minWallHeight && (thinOnX || thinOnZ))
        {
            ExpandThinWall(box, colliderThickness);
            worldColliders.push_back(box);
        }
    }
}

void AddModelTriangleWallColliders(const Model& model, const glm::mat4& transform)
{
    const float minWallHeight = 0.35f;
    const float maxVerticalNormalY = 0.35f;
    const float colliderThickness = 0.32f;

    for (const auto& mesh : model.meshes)
    {
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            const glm::vec3 p0 = glm::vec3(transform * glm::vec4(mesh.vertices[mesh.indices[i]].Position, 1.0f));
            const glm::vec3 p1 = glm::vec3(transform * glm::vec4(mesh.vertices[mesh.indices[i + 1]].Position, 1.0f));
            const glm::vec3 p2 = glm::vec3(transform * glm::vec4(mesh.vertices[mesh.indices[i + 2]].Position, 1.0f));

            glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(normal) < 0.0001f)
                continue;

            normal = glm::normalize(normal);
            if (std::fabs(normal.y) > maxVerticalNormalY)
                continue;

            AABB box = MakeAABB(glm::min(p0, glm::min(p1, p2)), glm::max(p0, glm::max(p1, p2)));
            glm::vec3 size = box.max - box.min;
            if (size.y < minWallHeight)
                continue;

            box.min.y -= 0.05f;
            box.max.y += 0.05f;
            ExpandThinWall(box, colliderThickness);
            worldColliders.push_back(box);
        }
    }
}

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

glm::vec3 GetMovementInput(GLFWwindow* window)
{
    glm::vec3 forward(camera.Front.x, 0.0f, camera.Front.z);
    glm::vec3 right(camera.Right.x, 0.0f, camera.Right.z);

    if (glm::length(forward) > 0.0001f)
        forward = glm::normalize(forward);
    else
        forward = glm::vec3(0.0f, 0.0f, -1.0f);

    if (glm::length(right) > 0.0001f)
        right = glm::normalize(right);
    else
        right = glm::vec3(1.0f, 0.0f, 0.0f);

    glm::vec3 movement(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        movement += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        movement -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        movement -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        movement += right;

    if (glm::length(movement) > 0.0001f)
        movement = glm::normalize(movement) * camera.MovementSpeed * deltaTime;

    return movement;
}

void MovePlayerWithCollisions(const glm::vec3& delta)
{
    float distance = glm::length(glm::vec2(delta.x, delta.z));
    int steps = std::max(1, (int)std::ceil(distance / 0.12f));
    glm::vec3 step = delta / (float)steps;

    for (int i = 0; i < steps; ++i)
    {
        glm::vec3 candidate = camera.Position;
        candidate.x += step.x;
        if (!CollidesWithWorld(candidate))
            camera.Position.x = candidate.x;

        candidate = camera.Position;
        candidate.z += step.z;
        if (!CollidesWithWorld(candidate))
            camera.Position.z = candidate.z;
    }
}

void ApplyGravity()
{
    playerVelocityY += gravity * deltaTime;
    camera.Position.y += playerVelocityY * deltaTime;
    isGrounded = false;

    playerBox = MakePlayerBox(camera.Position);

    for (const auto& box : floorColliders)
    {
        if (!CheckCollision(playerBox, box))
            continue;

        if (playerVelocityY <= 0.0f)
        {
            camera.Position.y = box.max.y + playerEyeHeight;
            isGrounded = true;
        }
        else
        {
            camera.Position.y = box.min.y - playerHeadClearance;
        }

        playerVelocityY = 0.0f;
        playerBox = MakePlayerBox(camera.Position);
        return;
    }
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

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
    // VAO para puertas y props simples
    // -----------------------------------------------------
    unsigned int propVAO, propVBO;
    glGenVertexArrays(1, &propVAO);
    glGenBuffers(1, &propVBO);

    glBindVertexArray(propVAO);
    glBindBuffer(GL_ARRAY_BUFFER, propVBO);
    glBufferData(GL_ARRAY_BUFFER, cubeVerticesSize, cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

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
    unsigned int exitDoorTex = loadTexture("assets/models/textures/EXITDOOR_baseColor.png");
    unsigned int bigDoorTex = loadTexture("assets/models/textures/BIGDOOR1_baseColor.png");

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

    glm::mat4 doomWorldModel = glm::mat4(1.0f);
    doomWorldModel = glm::translate(doomWorldModel, glm::vec3(0.0f, -7.15f, -5.0f));
    doomWorldModel = glm::rotate(doomWorldModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    doomWorldModel = glm::scale(doomWorldModel, glm::vec3(0.02f));


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

    // Reconstruir la lista final de colisiones con el sistema nuevo.
    worldColliders.clear();
    floorColliders.clear();
    doorModels.clear();

    AddFloorCollider(glm::vec3(0.0f, floorY - 0.5f, 0.0f), glm::vec3(400.0f, 1.0f, 400.0f));
    AddModelTriangleWallColliders(doomWorld, doomWorldModel);

    AddWorldCollider(glm::vec3(-27.0f, -5.25f, 2.5f), glm::vec3(0.8f, 4.0f, 39.0f));
    AddWorldCollider(glm::vec3(-3.0f, -5.25f, 2.5f), glm::vec3(0.8f, 4.0f, 39.0f));
    AddWorldCollider(glm::vec3(-15.0f, -5.25f, 22.2f), glm::vec3(24.0f, 4.0f, 0.8f));
    AddWorldCollider(glm::vec3(-15.0f, -5.25f, -17.2f), glm::vec3(24.0f, 4.0f, 0.8f));

    AddDoorModel(glm::vec3(-19.0f, -5.55f, 18.8f), glm::vec3(3.2f, 3.2f, 0.35f), bigDoorTex, 1.0f);
    AddDoorModel(glm::vec3(-12.0f, -5.55f, 9.0f), glm::vec3(0.35f, 3.2f, 2.8f), exitDoorTex, 1.0f);
    AddDoorModel(glm::vec3(-23.5f, -5.55f, 6.0f), glm::vec3(0.35f, 3.2f, 2.6f), exitDoorTex, 1.0f);
    AddDoorModel(glm::vec3(-17.0f, -5.55f, -4.0f), glm::vec3(3.0f, 3.2f, 0.35f), bigDoorTex, 1.0f);
    AddWorldCollider(glm::vec3(-19.0f, -5.15f, -6.60f), glm::vec3(1.5f, 2.2f, 1.5f));

    std::cout << "Colliders cargados: " << worldColliders.size() << std::endl;

    // -----------------------------------------------------
    // Audio
    // -----------------------------------------------------
    if (ma_engine_init(NULL, &gEngine) != MA_SUCCESS)
    {
        std::cout << "Failed to initialize miniaudio engine\n";
        return -1;
    }

    ma_sound bgm;
    bool bgmReady = false;
    if (ma_sound_init_from_file(&gEngine, "sonidos/cdoomtheme.ogg",
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
        NULL, NULL, &bgm) == MA_SUCCESS)
    {
        bgmReady = true;
        ma_sound_set_looping(&bgm, MA_TRUE);
        ma_sound_start(&bgm);
    }

    //---------------------------------------------------------
    //Sonido Cacodemon
    //---------------------------------------------------------
ma_sound cacoSound;
bool cacoSoundReady = false;
float nextCacoSoundTime = 10.0f;
if (ma_sound_init_from_file(&gEngine, "sonidos/Cacodemon_sonido1.wav",
    MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
    NULL, NULL, &cacoSound) == MA_SUCCESS)
{
    cacoSoundReady = true;
    ma_sound_set_looping(&cacoSound, MA_FALSE); // no repetir automáticamente
    ma_sound_set_volume(&cacoSound, 0.3f);      // volumen bajo
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

        processInput(window);
        MovePlayerWithCollisions(GetMovementInput(window));
        ApplyGravity();

        if (cacoSoundReady && currentFrame >= nextCacoSoundTime)
        {
            ma_sound_seek_to_pcm_frame(&cacoSound, 0);
            ma_sound_start(&cacoSound);
            nextCacoSoundTime = currentFrame + 10.0f;
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
        lightingShader.setMat4("model", doomWorldModel);
        lightingShader.setFloat("texScale", 1.0f);

        doomWorld.Draw(lightingShader);

        glBindVertexArray(propVAO);
        glActiveTexture(GL_TEXTURE0);

        for (const auto& door : doorModels)
        {
            glm::mat4 doorMatrix = glm::mat4(1.0f);
            doorMatrix = glm::translate(doorMatrix, door.center);
            doorMatrix = glm::scale(doorMatrix, door.size);

            glBindTexture(GL_TEXTURE_2D, door.texture);
            lightingShader.setMat4("model", doorMatrix);
            lightingShader.setFloat("texScale", door.texScale);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindVertexArray(0);

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

    if (cacoSoundReady)
        ma_sound_uninit(&cacoSound);

    if (bgmReady)
        ma_sound_uninit(&bgm);
    ma_engine_uninit(&gEngine);

    glfwTerminate();
    return 0;
}
