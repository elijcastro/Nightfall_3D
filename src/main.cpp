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
#include <array>
#include <cctype>
#include <string>

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
const float standingPlayerEyeHeight = 1.45f;
const float standingPlayerHeadClearance = 0.12f;
const float crouchingPlayerEyeHeight = 0.90f;
const float crouchingPlayerHeadClearance = 0.10f;
const float maxStepHeight = 0.65f;
const float jumpVelocity = 5.2f;
const float floorY = -7.15f;

float currentPlayerEyeHeight = standingPlayerEyeHeight;
float currentPlayerHeadClearance = standingPlayerHeadClearance;
bool isCrouching = false;

glm::vec3 cacoPosition = glm::vec3(-20.0f, -5.0f, -4.60f);
glm::vec3 cacoTargetPosition = cacoPosition;
bool cacoIsMoving = false;
float cacoMoveSpeed = 6.0f;
float cacoHitFlashTime = 0.0f;
float cacoHitFlashDuration = 0.25f;

ma_sound cacoHitSound;
bool cacoHitSoundReady = false;

// ---------------------------------------------------------
// IA del Cacodemon
// ---------------------------------------------------------
bool cacoHasSeenPlayer      = false;   // ya detectó al jugador
bool cacoSightSoundPlayed   = false;   // Cacodemon_sight.wav ya sonó
bool cacoDangerMusicPlaying = false;   // DangerCacodemonZone.ogg está en loop

float cacoDetectionRadius   = 14.0f;   // radio de detección en la habitación
float cacoChaseSpeed        = 3.0f;    // velocidad de persecución
float cacoChaseRadius       = 25.0f;   // radio de persecución
// Sonidos específicos del Cacodemon
ma_sound cacoSightSound;    // Cacodemon_sight.wav (solo una vez al verte)
ma_sound cacoIdleLoopSound; // Cacodemon_sonido1.wav (loop mientras te persigue)
ma_sound cacoDangerMusic;   // DangerCacodemonZone.ogg (loop de música de peligro)

int playerHealth = 3;        // 2 golpes antes de morir
bool playerHurt = false;     // para activar pantalla roja
float hurtTimer = 0.0f;      // duración del efecto de daño

ma_sound playerHurtSound;
ma_sound playerDeathSound;
bool playerHurtSoundReady = false;
bool playerDeathSoundReady = false;

int cacoHealth = 5;
bool cacoHurt = false;
float cacoHurtTimer = 0.0f;
bool cacoIsAlive = true;
float cacoSqueeze = 1.0f; // 1.0 = normal, <1.0 = comprimido


float fireCooldown = 0.0f;
float fireRate = 0.25f; // 4 disparos por segundo

ma_sound cacoHurtSound;
ma_sound cacoDeathSound;
bool cacoHurtSoundReady = false;
bool cacoDeathSoundReady = false;


AABB playerBox;

std::vector<AABB> worldColliders;
std::vector<AABB> floorColliders;
std::vector<DoorModel> doorModels;
bool jumpWasPressed = false;


int cacoExplosionFrame = 0;
float cacoExplosionFrameTime = 0.0f;



bool RayIntersectsAABB(const glm::vec3& rayOrigin,
                       const glm::vec3& rayDir,
                       const AABB& box,
                       float& tOut)
{
    float tMin = 0.0f;
    float tMax = 10000.0f;

    // X
    if (rayDir.x != 0.0f)
    {
        float tx1 = (box.min.x - rayOrigin.x) / rayDir.x;
        float tx2 = (box.max.x - rayOrigin.x) / rayDir.x;
        tMin = std::max(tMin, std::min(tx1, tx2));
        tMax = std::min(tMax, std::max(tx1, tx2));
    }

    // Y
    if (rayDir.y != 0.0f)
    {
        float ty1 = (box.min.y - rayOrigin.y) / rayDir.y;
        float ty2 = (box.max.y - rayOrigin.y) / rayDir.y;
        tMin = std::max(tMin, std::min(ty1, ty2));
        tMax = std::min(tMax, std::max(ty1, ty2));
    }

    // Z
    if (rayDir.z != 0.0f)
    {
        float tz1 = (box.min.z - rayOrigin.z) / rayDir.z;
        float tz2 = (box.max.z - rayOrigin.z) / rayDir.z;
        tMin = std::max(tMin, std::min(tz1, tz2));
        tMax = std::min(tMax, std::max(tz1, tz2));
    }

    if (tMax >= tMin)
    {
        tOut = tMin;
        return true;
    }

    return false;
}


bool PointInsideAABB(const glm::vec3& p, const AABB& box)
{
    return (p.x >= box.min.x && p.x <= box.max.x &&
            p.y >= box.min.y && p.y <= box.max.y &&
            p.z >= box.min.z && p.z <= box.max.z);
}

AABB MakeAABB(const glm::vec3& a, const glm::vec3& b)
{
    return { glm::min(a, b), glm::max(a, b) };
}

AABB MakeAABBFromCenter(const glm::vec3& center, const glm::vec3& size)
{
    glm::vec3 halfSize = size * 0.5f;
    return MakeAABB(center - halfSize, center + halfSize);
}

AABB MakePlayerBox(const glm::vec3& position, float eyeHeight, float headClearance)
{
    return {
        glm::vec3(position.x - playerRadius, position.y - eyeHeight, position.z - playerRadius),
        glm::vec3(position.x + playerRadius, position.y + headClearance, position.z + playerRadius)
    };
}

AABB MakePlayerBox(const glm::vec3& position)
{
    return MakePlayerBox(position, currentPlayerEyeHeight, currentPlayerHeadClearance);
}

bool CollidesWithWorld(const glm::vec3& position, float eyeHeight, float headClearance)
{
    AABB candidate = MakePlayerBox(position, eyeHeight, headClearance);
    for (const auto& box : worldColliders)
    {
        if (box.max.y <= candidate.min.y + 0.02f)
            continue;

        if (CheckCollision(candidate, box))
            return true;
    }

    return false;
}

bool CollidesWithWorld(const glm::vec3& position)
{
    return CollidesWithWorld(position, currentPlayerEyeHeight, currentPlayerHeadClearance);
}

bool FindStepTarget(const glm::vec3& position, float currentFootY, float& targetFootY)
{
    AABB candidate = MakePlayerBox(position);
    float bestStepY = currentFootY;
    bool foundStep = false;

    for (const auto& box : floorColliders)
    {
        bool overlapsX = candidate.max.x > box.min.x && candidate.min.x < box.max.x;
        bool overlapsZ = candidate.max.z > box.min.z && candidate.min.z < box.max.z;
        if (!overlapsX || !overlapsZ)
            continue;

        float topY = box.max.y;
        float stepDelta = topY - currentFootY;
        if (stepDelta <= 0.02f || stepDelta > maxStepHeight)
            continue;

        if (!foundStep || topY < bestStepY)
        {
            bestStepY = topY;
            foundStep = true;
        }
    }

    targetFootY = bestStepY;
    return foundStep;
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

void AddFloorCollider(const AABB& box)
{
    floorColliders.push_back(box);
    worldColliders.push_back(box);
}

void AddDoorModel(const glm::vec3& center, const glm::vec3& size, unsigned int texture, float texScale = 1.0f)
{
    doorModels.push_back({ center, size, texture, texScale });
    //worldColliders.push_back(MakeAABBFromCenter(center, size));
}

void AddDoorFrameColliders(const glm::vec3& center, const glm::vec3& size)
{
    float halfX = size.x * 0.5f;
    float halfY = size.y * 0.5f;
    float halfZ = size.z * 0.5f;

    float thickness = 0.25f; // grosor de las columnas laterales

    // Lado izquierdo
    worldColliders.push_back({
        glm::vec3(center.x - halfX, center.y - halfY, center.z - halfZ),
        glm::vec3(center.x - halfX + thickness, center.y + halfY, center.z + halfZ)
    });

    // Lado derecho
    worldColliders.push_back({
        glm::vec3(center.x + halfX - thickness, center.y - halfY, center.z - halfZ),
        glm::vec3(center.x + halfX, center.y + halfY, center.z + halfZ)
    });
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

void AddModelFloorColliders(const Model& model, const glm::mat4& transform)
{
    const float minHorizontalNormalY = 0.65f;
    const float floorThickness = 0.25f;
    const float topPadding = 0.04f;
    const float horizontalPadding = 0.03f;

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
            if (std::fabs(normal.y) < minHorizontalNormalY)
                continue;

            AABB box = MakeAABB(glm::min(p0, glm::min(p1, p2)), glm::max(p0, glm::max(p1, p2)));
            glm::vec3 size = box.max - box.min;
            if (size.x < 0.05f && size.z < 0.05f)
                continue;

            float topY = box.max.y;
            box.min.x -= horizontalPadding;
            box.max.x += horizontalPadding;
            box.min.z -= horizontalPadding;
            box.max.z += horizontalPadding;
            box.min.y = topY - floorThickness;
            box.max.y = topY + topPadding;

            AddFloorCollider(box);
        }
    }
}

unsigned int quadVAO = 0;
unsigned int quadVBO = 0;

void drawFullScreenQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions   // texcoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
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

int gWindowWidth = SCR_WIDTH;
int gWindowHeight = SCR_HEIGHT;

enum class GameScreen {
    MainMenu,
    Instructions,
    Map,
    Playing
};

GameScreen gCurrentScreen = GameScreen::MainMenu;

struct UiRect {
    float x;
    float y;
    float w;
    float h;
};

struct UiInput {
    double mouseX;
    double mouseY;
    bool mouseClicked;
};

bool gMouseWasPressed = false;
bool gEnterWasPressed = false;
bool gEscWasPressed = false;
bool gIWasPressed = false;
bool gMWasPressed = false;
bool gCursorIsMenuMode = false;

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

bool KeyPressedOnce(GLFWwindow* window, int key, bool& wasPressed);
void SetCursorForCurrentScreen(GLFWwindow* window);

bool cacoIsExploding = false;
float cacoExplosionTimer = 0.0f;
unsigned int cacoExplosionTexture = 0;


// ---------------------------------------------------------
// Callbacks
// ---------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    gWindowWidth = width;
    gWindowHeight = height;
    glViewport(0, 0, width, height);
}

unsigned int explosionFrames[8];

unsigned int CreateExplosionTexture(int frameIndex)
{
    const int size = 64;
    unsigned char data[size * size * 4];

    float t = (frameIndex + 1) / 8.0f;   // 0.125 → 1.0
    float radius = t * 1.3f;             // expansión DOOM 64
    float fade = 1.0f - t;               // fade-out

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float dx = (x - size / 2) / float(size / 2);
            float dy = (y - size / 2) / float(size / 2);
            float dist = sqrt(dx*dx + dy*dy);

            // -----------------------------
            // Ruido DOOM 64 (simple pero eficaz)
            // -----------------------------
            float noise = 
                (sin(x * 0.35f + frameIndex * 0.8f) +
                 sin(y * 0.45f + frameIndex * 1.1f)) * 0.12f;

            dist -= noise;   // rompe la simetría

            int idx = (y * size + x) * 4;

            if (dist < radius * 0.35f)
            {
                // Núcleo brillante DOOM 64
                data[idx + 0] = 255;
                data[idx + 1] = 240;
                data[idx + 2] = 120;
                data[idx + 3] = 255 * fade;
            }
            else if (dist < radius * 0.7f)
            {
                // Fuego naranja
                data[idx + 0] = 255;
                data[idx + 1] = 140;
                data[idx + 2] = 40;
                data[idx + 3] = 220 * fade;
            }
            else if (dist < radius)
            {
                // Borde irregular + humo
                data[idx + 0] = 180;
                data[idx + 1] = 80;
                data[idx + 2] = 20;
                data[idx + 3] = 150 * fade;
            }
            else
            {
                // Transparente
                data[idx + 0] = 0;
                data[idx + 1] = 0;
                data[idx + 2] = 0;
                data[idx + 3] = 0;
            }
        }
    }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (gCurrentScreen != GameScreen::Playing)
    {
        firstMouse = true;
        lastX = (float)xpos;
        lastY = (float)ypos;
        return;
    }

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
    {
        float speedMultiplier = isCrouching ? 0.55f : 1.0f;
        movement = glm::normalize(movement) * camera.MovementSpeed * speedMultiplier * deltaTime;
    }

    return movement;
}

void DrawBillboardQuad()
{
    static unsigned int VAO = 0;
    static unsigned int VBO = 0;

    if (VAO == 0)
    {
        float quadVertices[] = {
            // posiciones      // UV
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,

            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}


void SetPlayerHeight(float eyeHeight, float headClearance)
{
    float footY = camera.Position.y - currentPlayerEyeHeight;
    currentPlayerEyeHeight = eyeHeight;
    currentPlayerHeadClearance = headClearance;
    camera.Position.y = footY + currentPlayerEyeHeight;
    playerBox = MakePlayerBox(camera.Position);
}

void UpdateCrouch(GLFWwindow* window)
{
    bool wantsCrouch = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;

    if (wantsCrouch)
    {
        if (!isCrouching)
        {
            isCrouching = true;
            SetPlayerHeight(crouchingPlayerEyeHeight, crouchingPlayerHeadClearance);
        }

        return;
    }

    if (!isCrouching)
        return;

    float footY = camera.Position.y - currentPlayerEyeHeight;
    glm::vec3 standingPosition = camera.Position;
    standingPosition.y = footY + standingPlayerEyeHeight;

    if (!CollidesWithWorld(standingPosition, standingPlayerEyeHeight, standingPlayerHeadClearance))
    {
        isCrouching = false;
        SetPlayerHeight(standingPlayerEyeHeight, standingPlayerHeadClearance);
    }
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
        {
            camera.Position.x = candidate.x;
        }
        else if (isGrounded)
        {
            float stepFootY = 0.0f;
            float currentFootY = camera.Position.y - currentPlayerEyeHeight;
            if (FindStepTarget(candidate, currentFootY, stepFootY))
            {
                candidate.y = stepFootY + currentPlayerEyeHeight;
                if (!CollidesWithWorld(candidate))
                {
                    camera.Position = candidate;
                    playerVelocityY = 0.0f;
                }
            }
        }

        candidate = camera.Position;
        candidate.z += step.z;
        if (!CollidesWithWorld(candidate))
        {
            camera.Position.z = candidate.z;
        }
        else if (isGrounded)
        {
            float stepFootY = 0.0f;
            float currentFootY = camera.Position.y - currentPlayerEyeHeight;
            if (FindStepTarget(candidate, currentFootY, stepFootY))
            {
                candidate.y = stepFootY + currentPlayerEyeHeight;
                if (!CollidesWithWorld(candidate))
                {
                    camera.Position = candidate;
                    playerVelocityY = 0.0f;
                }
            }
        }
    }
}

void ApplyGravity()
{
    playerVelocityY += gravity * deltaTime;
    camera.Position.y += playerVelocityY * deltaTime;
    isGrounded = false;

    playerBox = MakePlayerBox(camera.Position);

    bool foundCollision = false;
    float landingY = -std::numeric_limits<float>::max();
    float ceilingY = std::numeric_limits<float>::max();

    for (const auto& box : floorColliders)
    {
        if (!CheckCollision(playerBox, box))
            continue;

        if (playerVelocityY <= 0.0f)
        {
            landingY = std::max(landingY, box.max.y);
            foundCollision = true;
        }
        else
        {
            ceilingY = std::min(ceilingY, box.min.y);
            foundCollision = true;
        }
    }

    if (!foundCollision)
        return;

    if (playerVelocityY <= 0.0f)
    {
        camera.Position.y = landingY + currentPlayerEyeHeight;
        isGrounded = true;
    }
    else
    {
        camera.Position.y = ceilingY - currentPlayerHeadClearance;
    }

    playerVelocityY = 0.0f;
    playerBox = MakePlayerBox(camera.Position);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (KeyPressedOnce(window, GLFW_KEY_M, gMWasPressed))
    {
        gCurrentScreen = GameScreen::Map;
        gIsFiring = false;
        SetCursorForCurrentScreen(window);
        return;
    }

    UpdateCrouch(window);

    bool jumpPressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (jumpPressed && !jumpWasPressed && isGrounded)
    {
        playerVelocityY = jumpVelocity;
        isGrounded = false;
    }
    jumpWasPressed = jumpPressed;
     // Detectar click izquierdo del mouse 
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        gIsFiring = true;

    else
        gIsFiring = false;
}

std::array<const char*, 7> GetGlyph(char c)
{
    switch (c)
    {
        case 'A': return { "01110", "10001", "10001", "11111", "10001", "10001", "10001" };
        case 'B': return { "11110", "10001", "10001", "11110", "10001", "10001", "11110" };
        case 'C': return { "01111", "10000", "10000", "10000", "10000", "10000", "01111" };
        case 'D': return { "11110", "10001", "10001", "10001", "10001", "10001", "11110" };
        case 'E': return { "11111", "10000", "10000", "11110", "10000", "10000", "11111" };
        case 'F': return { "11111", "10000", "10000", "11110", "10000", "10000", "10000" };
        case 'G': return { "01111", "10000", "10000", "10111", "10001", "10001", "01111" };
        case 'H': return { "10001", "10001", "10001", "11111", "10001", "10001", "10001" };
        case 'I': return { "11111", "00100", "00100", "00100", "00100", "00100", "11111" };
        case 'J': return { "00111", "00010", "00010", "00010", "10010", "10010", "01100" };
        case 'K': return { "10001", "10010", "10100", "11000", "10100", "10010", "10001" };
        case 'L': return { "10000", "10000", "10000", "10000", "10000", "10000", "11111" };
        case 'M': return { "10001", "11011", "10101", "10101", "10001", "10001", "10001" };
        case 'N': return { "10001", "11001", "10101", "10011", "10001", "10001", "10001" };
        case 'O': return { "01110", "10001", "10001", "10001", "10001", "10001", "01110" };
        case 'P': return { "11110", "10001", "10001", "11110", "10000", "10000", "10000" };
        case 'Q': return { "01110", "10001", "10001", "10001", "10101", "10010", "01101" };
        case 'R': return { "11110", "10001", "10001", "11110", "10100", "10010", "10001" };
        case 'S': return { "01111", "10000", "10000", "01110", "00001", "00001", "11110" };
        case 'T': return { "11111", "00100", "00100", "00100", "00100", "00100", "00100" };
        case 'U': return { "10001", "10001", "10001", "10001", "10001", "10001", "01110" };
        case 'V': return { "10001", "10001", "10001", "10001", "10001", "01010", "00100" };
        case 'W': return { "10001", "10001", "10001", "10101", "10101", "10101", "01010" };
        case 'X': return { "10001", "10001", "01010", "00100", "01010", "10001", "10001" };
        case 'Y': return { "10001", "10001", "01010", "00100", "00100", "00100", "00100" };
        case 'Z': return { "11111", "00001", "00010", "00100", "01000", "10000", "11111" };
        case '0': return { "01110", "10001", "10011", "10101", "11001", "10001", "01110" };
        case '1': return { "00100", "01100", "00100", "00100", "00100", "00100", "01110" };
        case '2': return { "01110", "10001", "00001", "00010", "00100", "01000", "11111" };
        case '3': return { "11110", "00001", "00001", "01110", "00001", "00001", "11110" };
        case '4': return { "00010", "00110", "01010", "10010", "11111", "00010", "00010" };
        case '5': return { "11111", "10000", "10000", "11110", "00001", "00001", "11110" };
        case '6': return { "01110", "10000", "10000", "11110", "10001", "10001", "01110" };
        case '7': return { "11111", "00001", "00010", "00100", "01000", "01000", "01000" };
        case '8': return { "01110", "10001", "10001", "01110", "10001", "10001", "01110" };
        case '9': return { "01110", "10001", "10001", "01111", "00001", "00001", "01110" };
        case '-': return { "00000", "00000", "00000", "11111", "00000", "00000", "00000" };
        case ':': return { "00000", "00100", "00100", "00000", "00100", "00100", "00000" };
        case '.': return { "00000", "00000", "00000", "00000", "00000", "00100", "00100" };
        case '/': return { "00001", "00010", "00010", "00100", "01000", "01000", "10000" };
        default:  return { "00000", "00000", "00000", "00000", "00000", "00000", "00000" };
    }
}

bool IsInsideRect(const UiRect& rect, double x, double y)
{
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

bool KeyPressedOnce(GLFWwindow* window, int key, bool& wasPressed)
{
    bool pressed = glfwGetKey(window, key) == GLFW_PRESS;
    bool clicked = pressed && !wasPressed;
    wasPressed = pressed;
    return clicked;
}

UiInput ReadUiInput(GLFWwindow* window)
{
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool mouseClicked = mousePressed && !gMouseWasPressed;
    gMouseWasPressed = mousePressed;

    return { mouseX, mouseY, mouseClicked };
}

UiRect CenteredRect(float width, float height, float y)
{
    return { ((float)gWindowWidth - width) * 0.5f, y, width, height };
}

UiRect MainMenuButtonRect(int index)
{
    const float width = 360.0f;
    const float height = 58.0f;
    const float gap = 20.0f;
    const float firstY = (float)gWindowHeight * 0.43f;
    return CenteredRect(width, height, firstY + index * (height + gap));
}

UiRect BackButtonRect()
{
    return { 44.0f, (float)gWindowHeight - 86.0f, 190.0f, 50.0f };
}

void SetCursorForCurrentScreen(GLFWwindow* window)
{
    bool menuMode = gCurrentScreen != GameScreen::Playing;
    if (gCursorIsMenuMode == menuMode)
        return;

    glfwSetInputMode(window, GLFW_CURSOR, menuMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    gCursorIsMenuMode = menuMode;
    firstMouse = true;

    if (!menuMode)
    {
        lastX = (float)gWindowWidth * 0.5f;
        lastY = (float)gWindowHeight * 0.5f;
        glfwSetCursorPos(window, lastX, lastY);
    }
}

void StartGame(GLFWwindow* window)
{
    gCurrentScreen = GameScreen::Playing;
    gMouseWasPressed = false;
    gIsFiring = false;
    SetCursorForCurrentScreen(window);
    lastFrame = (float)glfwGetTime();
    deltaTime = 0.0f;
}

void ProcessMenuInput(GLFWwindow* window)
{
    SetCursorForCurrentScreen(window);
    gIsFiring = false;

    UiInput input = ReadUiInput(window);
    bool enterPressed = KeyPressedOnce(window, GLFW_KEY_ENTER, gEnterWasPressed);
    bool escPressed = KeyPressedOnce(window, GLFW_KEY_ESCAPE, gEscWasPressed);
    bool iPressed = KeyPressedOnce(window, GLFW_KEY_I, gIWasPressed);
    bool mPressed = KeyPressedOnce(window, GLFW_KEY_M, gMWasPressed);

    if (gCurrentScreen == GameScreen::MainMenu)
    {
        if ((input.mouseClicked && IsInsideRect(MainMenuButtonRect(0), input.mouseX, input.mouseY)) || enterPressed)
        {
            StartGame(window);
            return;
        }

        if ((input.mouseClicked && IsInsideRect(MainMenuButtonRect(1), input.mouseX, input.mouseY)) || iPressed)
        {
            gCurrentScreen = GameScreen::Instructions;
            return;
        }

        if ((input.mouseClicked && IsInsideRect(MainMenuButtonRect(2), input.mouseX, input.mouseY)) || mPressed)
        {
            gCurrentScreen = GameScreen::Map;
            return;
        }

        if (escPressed)
            glfwSetWindowShouldClose(window, true);

        return;
    }

    if (gCurrentScreen == GameScreen::Map && (mPressed || enterPressed))
    {
        StartGame(window);
        return;
    }

    if ((input.mouseClicked && IsInsideRect(BackButtonRect(), input.mouseX, input.mouseY)) || escPressed)
        gCurrentScreen = GameScreen::MainMenu;
}

void DrawUiRect(Shader& shader, unsigned int uiVAO, unsigned int uiVBO,
                const UiRect& rect, const glm::vec3& color, float alpha)
{
    if (rect.w <= 0.0f || rect.h <= 0.0f || gWindowWidth <= 0 || gWindowHeight <= 0)
        return;

    float left = (rect.x / (float)gWindowWidth) * 2.0f - 1.0f;
    float right = ((rect.x + rect.w) / (float)gWindowWidth) * 2.0f - 1.0f;
    float top = 1.0f - (rect.y / (float)gWindowHeight) * 2.0f;
    float bottom = 1.0f - ((rect.y + rect.h) / (float)gWindowHeight) * 2.0f;

    float vertices[] = {
        left,  bottom, 0.0f, 0.0f,
        right, bottom, 1.0f, 0.0f,
        right, top,    1.0f, 1.0f,
        right, top,    1.0f, 1.0f,
        left,  top,    0.0f, 1.0f,
        left,  bottom, 0.0f, 0.0f
    };

    shader.use();
    shader.setBool("useTexture", false);
    shader.setVec3("tintColor", color);
    shader.setFloat("alpha", alpha);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void DrawUiOutline(Shader& shader, unsigned int uiVAO, unsigned int uiVBO,
                   const UiRect& rect, const glm::vec3& color, float alpha, float thickness)
{
    DrawUiRect(shader, uiVAO, uiVBO, { rect.x, rect.y, rect.w, thickness }, color, alpha);
    DrawUiRect(shader, uiVAO, uiVBO, { rect.x, rect.y + rect.h - thickness, rect.w, thickness }, color, alpha);
    DrawUiRect(shader, uiVAO, uiVBO, { rect.x, rect.y, thickness, rect.h }, color, alpha);
    DrawUiRect(shader, uiVAO, uiVBO, { rect.x + rect.w - thickness, rect.y, thickness, rect.h }, color, alpha);
}

float TextWidth(const std::string& text, float scale)
{
    int lineLength = 0;
    int maxLength = 0;

    for (char c : text)
    {
        if (c == '\n')
        {
            maxLength = std::max(maxLength, lineLength);
            lineLength = 0;
        }
        else
        {
            ++lineLength;
        }
    }

    maxLength = std::max(maxLength, lineLength);
    if (maxLength == 0)
        return 0.0f;

    return maxLength * scale * 6.0f - scale;
}

void DrawText(Shader& shader, unsigned int uiVAO, unsigned int uiVBO,
              const std::string& text, float x, float y, float scale,
              const glm::vec3& color, float alpha = 1.0f)
{
    float startX = x;
    float cursorX = x;
    float cursorY = y;

    for (char raw : text)
    {
        if (raw == '\n')
        {
            cursorX = startX;
            cursorY += scale * 9.0f;
            continue;
        }

        char c = (char)std::toupper((unsigned char)raw);
        std::array<const char*, 7> glyph = GetGlyph(c);

        for (int row = 0; row < 7; ++row)
        {
            for (int col = 0; col < 5; ++col)
            {
                if (glyph[row][col] == '1')
                {
                    DrawUiRect(shader, uiVAO, uiVBO,
                        { cursorX + col * scale, cursorY + row * scale, scale, scale },
                        color, alpha);
                }
            }
        }

        cursorX += scale * 6.0f;
    }
}

void DrawCenteredText(Shader& shader, unsigned int uiVAO, unsigned int uiVBO,
                      const std::string& text, const UiRect& rect, float scale,
                      const glm::vec3& color, float alpha = 1.0f)
{
    float width = TextWidth(text, scale);
    float height = scale * 7.0f;
    DrawText(shader, uiVAO, uiVBO, text,
        rect.x + (rect.w - width) * 0.5f,
        rect.y + (rect.h - height) * 0.5f,
        scale, color, alpha);
}

void DrawMenuButton(Shader& shader, unsigned int uiVAO, unsigned int uiVBO,
                    const UiRect& rect, const std::string& label, double mouseX, double mouseY)
{
    bool hovered = IsInsideRect(rect, mouseX, mouseY);
    glm::vec3 fill = hovered ? glm::vec3(0.48f, 0.10f, 0.08f) : glm::vec3(0.18f, 0.04f, 0.05f);
    glm::vec3 border = hovered ? glm::vec3(0.98f, 0.78f, 0.28f) : glm::vec3(0.55f, 0.16f, 0.14f);

    DrawUiRect(shader, uiVAO, uiVBO, rect, fill, 0.92f);
    DrawUiOutline(shader, uiVAO, uiVBO, rect, border, 1.0f, 3.0f);
    DrawCenteredText(shader, uiVAO, uiVBO, label, rect, 4.0f, glm::vec3(0.96f, 0.91f, 0.80f), 1.0f);
}

void DrawMapWorldRect(Shader& shader, unsigned int uiVAO, unsigned int uiVBO,
                      const UiRect& mapRect, float minX, float minZ, float maxX, float maxZ,
                      const glm::vec3& color, float alpha)
{
    const float worldMinX = -28.0f;
    const float worldMaxX = -2.0f;
    const float worldMinZ = -18.0f;
    const float worldMaxZ = 23.0f;

    float x0 = mapRect.x + ((minX - worldMinX) / (worldMaxX - worldMinX)) * mapRect.w;
    float x1 = mapRect.x + ((maxX - worldMinX) / (worldMaxX - worldMinX)) * mapRect.w;
    float y0 = mapRect.y + mapRect.h - ((maxZ - worldMinZ) / (worldMaxZ - worldMinZ)) * mapRect.h;
    float y1 = mapRect.y + mapRect.h - ((minZ - worldMinZ) / (worldMaxZ - worldMinZ)) * mapRect.h;

    DrawUiRect(shader, uiVAO, uiVBO,
        { std::min(x0, x1), std::min(y0, y1), std::fabs(x1 - x0), std::fabs(y1 - y0) },
        color, alpha);
}

glm::vec2 WorldToMapPoint(const UiRect& mapRect, const glm::vec3& worldPos)
{
    const float worldMinX = -28.0f;
    const float worldMaxX = -2.0f;
    const float worldMinZ = -18.0f;
    const float worldMaxZ = 23.0f;

    float x = mapRect.x + ((worldPos.x - worldMinX) / (worldMaxX - worldMinX)) * mapRect.w;
    float y = mapRect.y + mapRect.h - ((worldPos.z - worldMinZ) / (worldMaxZ - worldMinZ)) * mapRect.h;
    return glm::vec2(x, y);
}

void DrawMainMenu(Shader& shader, unsigned int uiVAO, unsigned int uiVBO, double mouseX, double mouseY)
{
    DrawUiRect(shader, uiVAO, uiVBO, { 0.0f, 0.0f, (float)gWindowWidth, (float)gWindowHeight },
        glm::vec3(0.02f, 0.02f, 0.025f), 0.78f);

    UiRect titleArea = CenteredRect(760.0f, 90.0f, 88.0f);
    DrawCenteredText(shader, uiVAO, uiVBO, "NIGHTFALL 3D",
        { titleArea.x + 5.0f, titleArea.y + 5.0f, titleArea.w, titleArea.h },
        8.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.55f);
    DrawCenteredText(shader, uiVAO, uiVBO, "NIGHTFALL 3D",
        titleArea, 8.0f, glm::vec3(0.96f, 0.18f, 0.10f), 1.0f);

    UiRect subtitleArea = CenteredRect(360.0f, 34.0f, 192.0f);
    DrawCenteredText(shader, uiVAO, uiVBO, "MENU DE INICIO",
        subtitleArea, 3.0f, glm::vec3(0.78f, 0.82f, 0.74f), 1.0f);

    DrawMenuButton(shader, uiVAO, uiVBO, MainMenuButtonRect(0), "ENTRAR", mouseX, mouseY);
    DrawMenuButton(shader, uiVAO, uiVBO, MainMenuButtonRect(1), "INSTRUCCIONES", mouseX, mouseY);
    DrawMenuButton(shader, uiVAO, uiVBO, MainMenuButtonRect(2), "MAPA", mouseX, mouseY);
}

void DrawInstructions(Shader& shader, unsigned int uiVAO, unsigned int uiVBO, double mouseX, double mouseY)
{
    DrawUiRect(shader, uiVAO, uiVBO, { 0.0f, 0.0f, (float)gWindowWidth, (float)gWindowHeight },
        glm::vec3(0.015f, 0.018f, 0.022f), 0.86f);

    DrawCenteredText(shader, uiVAO, uiVBO, "INSTRUCCIONES",
        CenteredRect(620.0f, 58.0f, 70.0f), 6.0f, glm::vec3(0.93f, 0.20f, 0.12f), 1.0f);

    UiRect panel = CenteredRect(850.0f, 390.0f, 160.0f);
    DrawUiRect(shader, uiVAO, uiVBO, panel, glm::vec3(0.06f, 0.07f, 0.075f), 0.88f);
    DrawUiOutline(shader, uiVAO, uiVBO, panel, glm::vec3(0.44f, 0.12f, 0.11f), 1.0f, 3.0f);

    DrawText(shader, uiVAO, uiVBO,
        "CONTROLES\n\n"
        "W A S D   MOVERSE\n"
        "MOUSE     MIRAR\n"
        "Z         DISPARAR\n"
        "ESPACIO   SALTAR\n"
        "C         AGACHARSE\n"
        "M         MAPA PAUSA\n"
        "ESC       SALIR",
        panel.x + 70.0f, panel.y + 50.0f, 3.5f,
        glm::vec3(0.88f, 0.86f, 0.76f), 1.0f);

    DrawMenuButton(shader, uiVAO, uiVBO, BackButtonRect(), "VOLVER", mouseX, mouseY);
}

void DrawMapScreen(Shader& shader, unsigned int uiVAO, unsigned int uiVBO, double mouseX, double mouseY)
{
    DrawUiRect(shader, uiVAO, uiVBO, { 0.0f, 0.0f, (float)gWindowWidth, (float)gWindowHeight },
        glm::vec3(0.012f, 0.017f, 0.019f), 0.88f);

    DrawCenteredText(shader, uiVAO, uiVBO, "MAPA",
        CenteredRect(320.0f, 58.0f, 60.0f), 7.0f, glm::vec3(0.93f, 0.20f, 0.12f), 1.0f);

    UiRect mapRect = CenteredRect(760.0f, 390.0f, 145.0f);
    DrawUiRect(shader, uiVAO, uiVBO, mapRect, glm::vec3(0.03f, 0.05f, 0.05f), 0.95f);
    DrawUiOutline(shader, uiVAO, uiVBO, mapRect, glm::vec3(0.18f, 0.56f, 0.52f), 1.0f, 3.0f);

    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -27.0f, -17.2f, -3.0f, 22.2f, glm::vec3(0.10f, 0.22f, 0.20f), 0.95f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -27.0f, -17.2f, -26.2f, 22.2f, glm::vec3(0.47f, 0.09f, 0.08f), 1.0f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -3.8f, -17.2f, -3.0f, 22.2f, glm::vec3(0.47f, 0.09f, 0.08f), 1.0f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -27.0f, 21.4f, -3.0f, 22.2f, glm::vec3(0.47f, 0.09f, 0.08f), 1.0f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -27.0f, -17.2f, -3.0f, -16.4f, glm::vec3(0.47f, 0.09f, 0.08f), 1.0f);

    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -20.6f, 18.5f, -17.4f, 19.1f, glm::vec3(0.88f, 0.66f, 0.18f), 1.0f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -12.2f, 7.6f, -11.8f, 10.4f, glm::vec3(0.88f, 0.66f, 0.18f), 1.0f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -23.7f, 4.7f, -23.3f, 7.3f, glm::vec3(0.88f, 0.66f, 0.18f), 1.0f);
    DrawMapWorldRect(shader, uiVAO, uiVBO, mapRect, -18.5f, -4.3f, -15.5f, -3.7f, glm::vec3(0.88f, 0.66f, 0.18f), 1.0f);

    glm::vec2 startPoint = WorldToMapPoint(mapRect, glm::vec3(-19.0f, 0.0f, 12.6f));
    DrawUiRect(shader, uiVAO, uiVBO, { startPoint.x - 5.0f, startPoint.y - 5.0f, 10.0f, 10.0f },
        glm::vec3(0.20f, 0.80f, 0.40f), 1.0f);
    DrawText(shader, uiVAO, uiVBO, "INICIO", startPoint.x + 10.0f, startPoint.y - 8.0f, 2.0f,
        glm::vec3(0.75f, 0.90f, 0.70f), 1.0f);

    glm::vec2 enemyPoint = WorldToMapPoint(mapRect, glm::vec3(-19.0f, 0.0f, -6.6f));
    DrawUiRect(shader, uiVAO, uiVBO, { enemyPoint.x - 6.0f, enemyPoint.y - 6.0f, 12.0f, 12.0f },
        glm::vec3(0.60f, 0.25f, 0.86f), 1.0f);
    DrawText(shader, uiVAO, uiVBO, "ENEMIGO", enemyPoint.x + 12.0f, enemyPoint.y - 8.0f, 2.0f,
        glm::vec3(0.82f, 0.70f, 0.95f), 1.0f);

    glm::vec2 playerPoint = WorldToMapPoint(mapRect, camera.Position);
    DrawUiRect(shader, uiVAO, uiVBO, { playerPoint.x - 6.0f, playerPoint.y - 6.0f, 12.0f, 12.0f },
        glm::vec3(0.95f, 0.10f, 0.08f), 1.0f);
    DrawText(shader, uiVAO, uiVBO, "TU", playerPoint.x + 12.0f, playerPoint.y - 8.0f, 2.0f,
        glm::vec3(0.95f, 0.82f, 0.72f), 1.0f);

    DrawText(shader, uiVAO, uiVBO,
        "ROJO TU POSICION   VERDE INICIO   AMARILLO PUERTAS",
        mapRect.x + 24.0f, mapRect.y + mapRect.h + 22.0f, 2.0f,
        glm::vec3(0.82f, 0.84f, 0.76f), 1.0f);

    DrawText(shader, uiVAO, uiVBO,
        "M O ENTER PARA VOLVER AL JUEGO",
        mapRect.x + 24.0f, mapRect.y + mapRect.h + 48.0f, 2.0f,
        glm::vec3(0.82f, 0.84f, 0.76f), 1.0f);

    DrawMenuButton(shader, uiVAO, uiVBO, BackButtonRect(), "VOLVER", mouseX, mouseY);
}

void DrawMenuOverlay(Shader& shader, unsigned int uiVAO, unsigned int uiVBO, GLFWwindow* window)
{
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (gCurrentScreen == GameScreen::MainMenu)
        DrawMainMenu(shader, uiVAO, uiVBO, mouseX, mouseY);
    else if (gCurrentScreen == GameScreen::Instructions)
        DrawInstructions(shader, uiVAO, uiVBO, mouseX, mouseY);
    else if (gCurrentScreen == GameScreen::Map)
        DrawMapScreen(shader, uiVAO, uiVBO, mouseX, mouseY);
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

float GetFloorHeightAt(const glm::vec3& pos)
{
    float bestY = -9999.0f;

    for (const auto& box : worldColliders)
    {
        // Detectar colisionadores de piso (minY == maxY)
        if (fabs(box.min.y - box.max.y) < 0.01f)
        {
            if (pos.x >= box.min.x && pos.x <= box.max.x &&
                pos.z >= box.min.z && pos.z <= box.max.z)
            {
                if (box.min.y > bestY)
                    bestY = box.min.y;
            }
        }
    }

    return bestY;
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
    Shader decalShader("src/decal.vert", "src/decal.frag");
    Shader spriteShader("src/sprite.vert", "src/sprite.frag");

   for (int i = 0; i < 8; i++)
   {
      explosionFrames[i] = CreateExplosionTexture(i);
   }


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

    // -----------------------------------------------------
    // VAO UI dinamica para menu, texto y mapa
    // -----------------------------------------------------
    unsigned int uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

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
    hudShader.setBool("useTexture", true);
    hudShader.setVec3("tintColor", glm::vec3(1.0f));
    hudShader.setFloat("alpha", 1.0f);

    // -----------------------------------------------------
    // Cargar GLTF
    // -----------------------------------------------------
    Model doomWorld("assets/models/Mapa1.gltf");
    Model cacodemon("assets/models/Cacodemon3.gltf");
    std::cout << "Cacodemon meshes: " << cacodemon.meshes.size() << std::endl;
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
    AddModelFloorColliders(doomWorld, doomWorldModel);
    AddModelTriangleWallColliders(doomWorld, doomWorldModel);

    AddWorldCollider(glm::vec3(-27.0f, -5.25f, 2.5f), glm::vec3(0.8f, 4.0f, 39.0f));
    AddWorldCollider(glm::vec3(-3.0f, -5.25f, 2.5f), glm::vec3(0.8f, 4.0f, 39.0f));
    AddWorldCollider(glm::vec3(-15.0f, -5.25f, 22.2f), glm::vec3(24.0f, 4.0f, 0.8f));
    AddWorldCollider(glm::vec3(-15.0f, -5.25f, -17.2f), glm::vec3(24.0f, 4.0f, 0.8f));

    AddDoorModel(glm::vec3(-19.0f, -5.55f, 18.8f), glm::vec3(3.2f, 3.2f, 0.35f), bigDoorTex, 1.0f);
    AddDoorFrameColliders(glm::vec3(-19.0f, -5.55f, 18.8f), glm::vec3(3.2f, 3.2f, 0.35f));
    AddDoorModel(glm::vec3(-12.0f, -5.55f, 9.0f), glm::vec3(0.35f, 3.2f, 2.8f), exitDoorTex, 1.0f);
    AddDoorFrameColliders(glm::vec3(-12.0f, -5.55f, 9.0f), glm::vec3(0.35f, 3.2f, 2.8f));
    AddDoorModel(glm::vec3(-23.5f, -5.55f, 6.0f), glm::vec3(0.35f, 3.2f, 2.6f), exitDoorTex, 1.0f);
    AddDoorFrameColliders(glm::vec3(-23.5f, -5.55f, 6.0f), glm::vec3(0.35f, 3.2f, 2.6f));
    AddDoorModel(glm::vec3(-17.0f, -5.55f, -4.0f), glm::vec3(3.0f, 3.2f, 0.35f), bigDoorTex, 1.0f);
    AddDoorFrameColliders(glm::vec3(-17.0f, -5.55f, -4.0f), glm::vec3(3.0f, 3.2f, 0.35f));
   // AddWorldCollider(glm::vec3(-19.0f, -5.15f, -6.60f), glm::vec3(1.5f, 2.2f, 1.5f));

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
// Sonidos del Cacodemon (NUEVO SISTEMA)
//---------------------------------------------------------
if (ma_sound_init_from_file(&gEngine, "sonidos/Cacodemon_sight.wav",
    MA_SOUND_FLAG_DECODE, NULL, NULL, &cacoSightSound) != MA_SUCCESS)
{
    std::cout << "Error cargando Cacodemon_sight.wav\n";
}
if (ma_sound_init_from_file(&gEngine, "sonidos/player_hurt.wav",
    MA_SOUND_FLAG_DECODE, NULL, NULL, &playerHurtSound) == MA_SUCCESS)
{
    playerHurtSoundReady = true;
}

if (ma_sound_init_from_file(&gEngine, "sonidos/player_death.wav",
    MA_SOUND_FLAG_DECODE, NULL, NULL, &playerDeathSound) == MA_SUCCESS)
{
    playerDeathSoundReady = true;
}



if (ma_sound_init_from_file(&gEngine, "sonidos/Cacodemon_sonido1.wav",
    MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &cacoIdleLoopSound) == MA_SUCCESS)
{
    ma_sound_set_looping(&cacoIdleLoopSound, MA_TRUE);
    ma_sound_set_volume(&cacoIdleLoopSound, 0.35f);
}
else
{
    std::cout << "Error cargando Cacodemon_sonido1.wav\n";
}

if (ma_sound_init_from_file(&gEngine, "sonidos/Overtaken.wav",   
    MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &cacoDangerMusic) == MA_SUCCESS)
{
    ma_sound_set_looping(&cacoDangerMusic, MA_TRUE);
    ma_sound_set_volume(&cacoDangerMusic, 0.45f);
}
else
{
    std::cout << "Error cargando Overtaken.wav\n";
}

if (ma_sound_init_from_file(&gEngine, "sonidos/caco_hurt.wav",
    MA_SOUND_FLAG_DECODE, NULL, NULL, &cacoHurtSound) == MA_SUCCESS)
{
    cacoHurtSoundReady = true;
}

if (ma_sound_init_from_file(&gEngine, "sonidos/caco_death.wav",
    MA_SOUND_FLAG_DECODE, NULL, NULL, &cacoDeathSound) == MA_SUCCESS)
{
    cacoDeathSoundReady = true;
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

        if (gCurrentScreen == GameScreen::Playing)
        {
        processInput(window);
        MovePlayerWithCollisions(GetMovementInput(window));
        ApplyGravity();
        
            // ---------------------------------------------------------
           // IA del Cacodemon: detección del jugador
           // ---------------------------------------------------------
           float distToPlayer = glm::distance(camera.Position, cacoPosition);

           if (!cacoHasSeenPlayer && distToPlayer < cacoDetectionRadius)
           {
              cacoHasSeenPlayer = true;

              // Sonido de avistamiento (solo una vez)
              if (!cacoSightSoundPlayed)
              {
                ma_sound_start(&cacoSightSound);
                cacoSightSoundPlayed = true;
              }

              // Música de peligro
              if (!cacoDangerMusicPlaying)
              {
                     if (bgmReady)
                          ma_sound_stop(&bgm);

                 ma_sound_start(&cacoDangerMusic);
                 cacoDangerMusicPlaying = true;
              }

               // Iniciar sonido de persecución
               ma_sound_start(&cacoIdleLoopSound);  
            }

            // ---------------------------------------------------------
            // IA del Cacodemon: persecución del jugador
           // ---------------------------------------------------------
if (cacoIsAlive && cacoHasSeenPlayer && distToPlayer < cacoChaseRadius)
{
    // Dirección hacia el jugador
    glm::vec3 dir = glm::normalize(camera.Position - cacoPosition);

    // Tamaño base del Cacodemon
    glm::vec3 baseSize = glm::vec3(0.625f, 0.6375f, 0.575f);

    // Aplicar compresión
    glm::vec3 cacoHalfSize = glm::vec3(
        baseSize.x * cacoSqueeze,
        baseSize.y * cacoSqueeze,
        baseSize.z * cacoSqueeze
    );

    // Intento de movimiento completo
    glm::vec3 nextPos = cacoPosition + dir * cacoChaseSpeed * deltaTime;

    AABB fullMove = {
        nextPos - cacoHalfSize,
        nextPos + cacoHalfSize
    };

    bool blocked = false;
    for (const auto& box : worldColliders)
    {
        if (CheckCollision(fullMove, box))
        {
            blocked = true;
            break;
        }
    }

    // Si está atorado → comprimir
    if (blocked)
    {
      float stepHeight = 0.35f; // altura del escalón
    glm::vec3 stepPos = cacoPosition;

    // 1. Subir primero
    stepPos.y += stepHeight;

    // 2. Intentar avanzar desde arriba
    glm::vec3 stepForward = stepPos + dir * cacoChaseSpeed * deltaTime;

    AABB stepBox = {
        stepForward - cacoHalfSize,
        stepForward + cacoHalfSize
    };

    bool stepBlocked = false;
    for (const auto& box : worldColliders)
        if (CheckCollision(stepBox, box)) stepBlocked = true;

    if (!stepBlocked)
    {
        // Subió el escalón y avanzó
        cacoPosition = stepForward;
        blocked = false;
    }
    }
    else
    {
        // volver a tamaño normal suavemente
        cacoSqueeze = glm::mix(cacoSqueeze, 1.0f, 5.0f * deltaTime);
    }

    // =====================================================
    // MOVIMIENTO PRINCIPAL + SLIDING
    // =====================================================

    if (!blocked)
    {
        // Movimiento normal
        cacoPosition = nextPos;
    }
    else
    {
        // -----------------------------------------
        // SLIDING: intentar mover solo en X
        // -----------------------------------------
        glm::vec3 slideX = cacoPosition + glm::vec3(dir.x, 0, 0) * cacoChaseSpeed * deltaTime;
        AABB boxX = { slideX - cacoHalfSize, slideX + cacoHalfSize };

        bool blockedX = false;
        for (const auto& box : worldColliders)
            if (CheckCollision(boxX, box)) blockedX = true;

        if (!blockedX)
        {
            cacoPosition = slideX;
        }
        else
        {
            // -----------------------------------------
            // SLIDING: intentar mover solo en Z
            // -----------------------------------------
            glm::vec3 slideZ = cacoPosition + glm::vec3(0, 0, dir.z) * cacoChaseSpeed * deltaTime;
            AABB boxZ = { slideZ - cacoHalfSize, slideZ + cacoHalfSize };

            bool blockedZ = false;
            for (const auto& box : worldColliders)
                if (CheckCollision(boxZ, box)) blockedZ = true;

            if (!blockedZ)
            {
                cacoPosition = slideZ;
            }
            // Si X y Z están bloqueados → esquina real
        }
    }
     
    // ===============================================
// LEVITACIÓN AUTOMÁTICA DEL CACODEMON
// ===============================================

// Altura deseada sobre el piso
float hoverHeight = 0.55f;

// Encontrar piso debajo del Cacodemon
float floorY = GetFloorHeightAt(cacoPosition);

if (floorY > -9000.0f)
{
    float targetY = floorY + hoverHeight;

    // Suavizar movimiento vertical
    cacoPosition.y = glm::mix(cacoPosition.y, targetY, 10.0f * deltaTime);
}
    
    // Asegurar sonido de persecución
    if (!ma_sound_is_playing(&cacoIdleLoopSound))
        ma_sound_start(&cacoIdleLoopSound);
}
           // ---------------------------------------------------------
// PASO 3 — Daño al jugador cuando el Cacodemon lo toca
// ---------------------------------------------------------
float touchDistance = 1.2f; // distancia para que el Cacodemon te toque

if (distToPlayer < touchDistance)
{
    if (!playerHurt) // evitar daño múltiple por frame
    {
        playerHealth--;
        playerHurt = true;
        hurtTimer = 0.9f; // pantalla roja por 0.9 segundos

        if (playerHurtSoundReady)
            ma_sound_start(&playerHurtSound);

        // Si la vida llega a 0 → muerte
        if (playerHealth <= 0)
        {
            if (playerDeathSoundReady)
                ma_sound_start(&playerDeathSound);

            // Reiniciar juego
            gCurrentScreen = GameScreen::MainMenu;

            // Resetear posición del jugador
            camera.Position = glm::vec3(-19.0f, -5.35f, 12.60f);
            cacoPosition = glm::vec3(-20.0f, -5.35f, -4.60f);

            // Resetear vida
            playerHealth = 3;

            // Resetear estado del Cacodemon
            cacoHasSeenPlayer = false;
            cacoSightSoundPlayed = false;
            cacoDangerMusicPlaying = false;
            

            ma_sound_stop(&cacoIdleLoopSound);
            ma_sound_stop(&cacoDangerMusic);
            ma_sound_set_looping(&bgm, MA_TRUE);
            ma_sound_start(&bgm);
        }
    }
}

        /*if (cacoSoundReady && currentFrame >= nextCacoSoundTime)
        {
            ma_sound_seek_to_pcm_frame(&cacoSound, 0);
            ma_sound_start(&cacoSound);
            nextCacoSoundTime = currentFrame + 10.0f;
        }*/

        // =============================================================
// DISPARO DEL JUGADOR
// =============================================================
fireCooldown -= deltaTime;
if (fireCooldown < 0.0f) fireCooldown = 0.0f;

if (gIsFiring && fireCooldown == 0.0f)
{
    fireCooldown = fireRate; 
    playGunshot();

    glm::vec3 origin = camera.Position;
    glm::vec3 dir = camera.Front;

    HitInfo hit = Raycast(origin, dir, 100.0f);

    // =============================================================
    // MATRIZ REAL DEL MODELO DEL CACODEMON (posición visual real)
    // =============================================================
    float time = (float)glfwGetTime();

    glm::vec3 lookDir = glm::normalize(camera.Position - cacoPosition);
    float angleY = atan2(lookDir.x, lookDir.z);
    angleY = glm::mix(0.0f, angleY, 0.85f);

    glm::mat4 modelCaco = glm::mat4(1.0f);
    modelCaco = glm::translate(modelCaco, cacoPosition);
    modelCaco = glm::rotate(modelCaco, angleY, glm::vec3(0,1,0));
    modelCaco = glm::translate(modelCaco, glm::vec3(0.0f, sin(time) * 1.0f, 0.0f));
    modelCaco = glm::scale(modelCaco, glm::vec3(0.5f));
    modelCaco = glm::rotate(modelCaco, glm::radians(-90.0f), glm::vec3(1,0,0));

    // POSICIÓN REAL DEL MODELO EN EL MUNDO
    glm::vec3 cacoWorldPos = glm::vec3(modelCaco[3]);

    // =============================================================
    // COLLIDER REAL DEL CACODEMON (ajustado por escala 0.5)
    // =============================================================
    glm::vec3 cacoHalfSize = glm::vec3(0.3f, 0.3f, 0.3f);

    AABB cacoBox;
    cacoBox.min = cacoWorldPos - cacoHalfSize;
    cacoBox.max = cacoWorldPos + cacoHalfSize;

    // =============================================================
    // RAYCAST CONTRA EL CACODEMON
    // =============================================================
    float tCaco;
    bool hitCaco = RayIntersectsAABB(origin, dir, cacoBox, tCaco);

    // Distancia al impacto con el mundo
    float tWorld = 1e9f;
    if (hit.hit)
    {
        tWorld = glm::distance(origin, hit.position);
    }

    // =============================================================
    // ¿LE PEGAMOS AL CACODEMON?
    // =============================================================
    if (hitCaco && tCaco < tWorld)
    {
        // DAÑO
        cacoHealth--;
        cacoHurt = true;
        cacoHurtTimer = 0.2f;

        if (cacoHurtSoundReady)
            ma_sound_start(&cacoHurtSound);

        // EMPUJÓN HACIA ATRÁS (usando posición visual real)
        glm::vec3 back = -glm::normalize(camera.Position - cacoWorldPos);
        cacoPosition += back * 0.5f;

        // ===============================================
        // BLOQUE DE MUERTE DEL CACODEMON (NUEVO)
        // ===============================================
        if (cacoHealth <= 0 && !cacoIsExploding)
        {
           cacoIsAlive = false;
           cacoIsExploding = true;
           cacoExplosionTimer = 0.0f;

           // detener sonidos del cacodemon
           ma_sound_stop(&cacoIdleLoopSound);
           ma_sound_stop(&cacoDangerMusic);

           // reproducir sonido de explosión
           if (cacoDeathSoundReady)
           ma_sound_start(&cacoDeathSound);

          // restaurar música normal
          ma_sound_start(&bgm);
       }
    }
    else
    {
        // =============================================================
        // DECAL Y CHISPAS (impacto en el mundo)
        // =============================================================
        decals.push_back({ hit.position, hit.normal });

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
        
       // ===============================================
    // ACTUALIZACIÓN DE EXPLOSIÓN DEL CACODEMON 
   // ===============================================
if (cacoIsExploding)
{
    cacoExplosionTimer += deltaTime;
    cacoExplosionFrameTime += deltaTime;
    
        // avanzar frame cada 0.07s
    if (cacoExplosionFrameTime >= 0.07f)
    {
        cacoExplosionFrame++;
        cacoExplosionFrameTime = 0.0f;

        if (cacoExplosionFrame > 7)
            cacoExplosionFrame = 7;
    }

    if (cacoExplosionTimer >= 0.6f)
    {
        cacoIsExploding = false;
    }
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

        }
        else
        {
            ProcessMenuInput(window);
            gCurrentGunFrame = 0;
            gGunAnimTime = 0.0f;
        }

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspectRatio = gWindowHeight > 0 ? (float)gWindowWidth / (float)gWindowHeight : 16.0f / 9.0f;
        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
            aspectRatio, 0.1f, 500.0f);

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
        if (cacoIsAlive)
      {
        float time = (float)glfwGetTime();
        glm::mat4 animCaco = glm::mat4(1.0f); 
        animCaco = glm::translate(animCaco, cacoPosition); // posición dentro del mapa
        // Rotar hacia el jugador
        glm::vec3 lookDir = glm::normalize(camera.Position - cacoPosition);
        float angleY = atan2(lookDir.x, lookDir.z);
        angleY = glm::mix(0.0f, angleY, 0.85f); // suavizado
        animCaco = glm::rotate(animCaco, angleY, glm::vec3(0, 1, 0));

        animCaco = glm::translate(animCaco, glm::vec3(0.0f, sin(time) * 1.2f, 0.0f)); // flotar
        animCaco = glm::scale(animCaco, glm::vec3(0.5f));
        animCaco = glm::rotate(animCaco, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

       
        lightingShader.setMat4("model", animCaco);
        lightingShader.setFloat("texScale", 1.0f);
        
        // ---------------------------------------------------------
        // Efecto visual de daño del Cacodemon
        // ---------------------------------------------------------
        if (cacoHurt)
        {
            cacoHurtTimer -= deltaTime;
            if (cacoHurtTimer <= 0.0f)
            cacoHurt = false;

            // Tint rojo cuando recibe daño
            lightingShader.setVec3("tintColor", glm::vec3(1.0f, 0.3f, 0.3f));
        }
        else
        {
            // Color normal
            lightingShader.setVec3("tintColor", glm::vec3(1.0f, 1.0f, 1.0f));
        }
        
        
        
        
        cacodemon.Draw(lightingShader);
    }
    
     // =========================
     // RENDER EXPLOSIÓN
     // =========================
    if (cacoIsExploding)
    {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, cacoPosition);
      model = glm::scale(model, glm::vec3(2.0f));

      spriteShader.use();
      spriteShader.setMat4("model", model);
      spriteShader.setMat4("view", view);
      spriteShader.setMat4("projection", projection);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, explosionFrames[cacoExplosionFrame]);
      spriteShader.setInt("explosionTex", 0);

       DrawBillboardQuad();
       glDisable(GL_BLEND);
    }
        
        
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

        if (gCurrentScreen == GameScreen::Playing)
        {
            hudShader.setBool("useTexture", true);
            hudShader.setVec3("tintColor", glm::vec3(1.0f));
            hudShader.setFloat("alpha", 1.0f);

            glBindVertexArray(gunVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gGunTextures[gCurrentGunFrame]);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(hudVAO);
            glBindTexture(GL_TEXTURE_2D, hudTex);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        else
        {
            DrawMenuOverlay(hudShader, uiVAO, uiVBO, window);
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        // Pantalla roja de daño
       if (playerHurt)
       {
           hurtTimer -= deltaTime;
           if (hurtTimer <= 0.0f)
           playerHurt = false;

           glDisable(GL_DEPTH_TEST);
           glEnable(GL_BLEND);
           glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

           hudShader.use();
           hudShader.setVec3("tintColor", glm::vec3(1.0f, 0.0f, 0.0f)); // rojo
           hudShader.setFloat("alpha", 0.35f); // transparencia


           drawFullScreenQuad();

           glDisable(GL_BLEND);
           glEnable(GL_DEPTH_TEST);
        }


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

   /* if (cacoSoundReady)
        ma_sound_uninit(&cacoSound);*/

    if (bgmReady)
        ma_sound_uninit(&bgm);
    ma_engine_uninit(&gEngine);

    glfwTerminate();
    return 0;
}