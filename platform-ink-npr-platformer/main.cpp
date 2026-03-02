#include "glad/glad.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

//Globals

int screenWidth  = 1200;
int screenHeight = 1000;

int mapWidth = 0;
int mapHeight = 0;
std::vector<std::string> mapData;

// Player
glm::vec3 playerPos(0.0f);
glm::vec3 playerSpawn(2.0f, 2.0f, 1.0f);

float playerRadius = 0.35f;
float playerSpeed  = 4.0f;
float playerYaw    = 0.0f;
float playerScale  = 0.45f;
float playerOutlineScale = 1.06f;

float playerFeetVisualOffset = 0.4f;


static const float playerModelYawOffset = glm::radians(90.0f);
float playerHalfHeight = 0.5f;


float zVelocity = 0.0f;
float gravity = 9.0f;
float jumpStrength = 4.0f;
bool  isGrounded = false;

// Win/Lose
float timeLimitSec = 100.0f;
float timeLeftSec  = 100.0f;
int   collectedCount = 0;
int   collectTarget  = 4;

bool levelWon  = false;
bool levelLost = false;

static std::vector<std::string> levelFiles = {
    "levels/level1.txt",
    "levels/level2.txt",
    "levels/level3.txt"
};
static int currentLevel = 0;


// Camera
glm::vec3 cameraPos(0.0f);
glm::vec3 cameraUp(0.0f, 0.0f, 1.0f);

float camDist   = 5.0f;
float camHeight = 2.0f;

float camYaw   = 0.0f;
float camPitch = -0.35f;

float mouseSens = 0.0025f;
float camSmooth = 14.0f;

glm::vec3 camLookOffset(0.0f, 0.0f, 0.7f);
glm::vec3 assetAxisFixEuler(glm::radians(90.0f), 0.0f, 0.0f);

// Shaders
static const GLchar* vertexSource =
"#version 150 core\n"
"in vec3 position;"
"in vec3 inNormal;"
"in vec2 inTexCoord;"
"in vec3 inColor;"
"out vec3 Normal;"
"out vec3 FragPos;"
"out vec2 TexCoord;"
"out vec3 VColor;"
"uniform mat4 model;"
"uniform mat4 view;"
"uniform mat4 proj;"
"uniform float uvScale;"
"void main() {"
"   TexCoord = inTexCoord * uvScale;"
"   VColor = inColor;"
"   Normal = mat3(transpose(inverse(model))) * inNormal;"
"   FragPos = vec3(model * vec4(position, 1.0));"
"   gl_Position = proj * view * model * vec4(position, 1.0);"
"}";

static const GLchar* fragmentSource =
"#version 150 core\n"
"in vec3 Normal;"
"in vec3 FragPos;"
"in vec2 TexCoord;"
"in vec3 VColor;"
"out vec4 outColor;"
"uniform sampler2D tex0;"
"uniform sampler2D toonRamp;"
"uniform vec3 objectColor;"
"uniform vec3 viewPos;"
"uniform int toonBands;"
"uniform float minIntensity;"
"uniform float rimThreshold;"
"uniform float rimIntensity;"
"uniform int isOutline;"
"void main() {"
"   if (isOutline == 1) { outColor = vec4(0.0,0.0,0.0,1.0); return; }"
"   vec3 lightDir = normalize(vec3(0.3, -0.3, -1.0));"
"   vec3 norm = normalize(Normal);"
"   float amount = clamp(dot(norm, -lightDir), 0.0, 1.0);"
"   float denom = max(float(toonBands - 1), 1.0);"
"   float intensity = floor(amount * denom) / denom;"
"   intensity = clamp(intensity, 0.0, 1.0);"
"   intensity = max(intensity, minIntensity);"
"   vec3 ramp = texture(toonRamp, vec2(intensity, 0.5)).rgb;"
"   vec3 texC = texture(tex0, TexCoord).rgb;"
"   vec3 base = texC * VColor * objectColor;"
"   float mag = dot(base, vec3(0.3, 0.6, 0.1));"
"   float steps = 5.5;"
"   float magQ = floor(mag * steps + 0.5) / steps;"
"   base *= (mag > 1e-4) ? (magQ / mag) : 0.0;"
"   base = clamp(base, 0.0, 1.0);"
"   vec3 ambient = vec3(0.4);"
"   vec3 finalColor = base * (ambient + ramp);"
"   vec3 V = normalize(viewPos - FragPos);"
"   float rimRaw = 1.0 - max(dot(norm, V), 0.0);"
"   float rim = smoothstep(rimThreshold, 1.0, rimRaw);"
"   finalColor += rim * vec3(rimIntensity);"
"   outColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);"
"}";


//models + world objects

struct Model {
    GLuint vao = 0;
    GLuint vbo = 0;
    int numVerts = 0;
};


enum class ObjType { Deco, Solid, Collectible, Hazard, Goal };

struct GameObject {
    ObjType type = ObjType::Deco;
    bool active = true;

    const Model* model = nullptr;

    glm::vec3 pos   = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    float yaw = 0.0f;

    bool outlined = true;
    float outlineMult = 1.03f;

    bool solid = false;
    glm::vec3 halfExtents = glm::vec3(0.5f);
    float triggerRadius = 0.6f;
};

static std::vector<GameObject> worldObjects;
static std::vector<int> solidIndices;

static GLuint shaderProgram = 0;
static GLint  uniformModel = -1;
static GLint  uniformView  = -1;
static GLint  uniformProj  = -1;
static GLint  uniformUvScale = -1;
static GLint  uniformViewPos = -1;
static GLint  uniformIsOutline = -1;
static GLint  uniformObjectColor = -1;

static GLuint whiteTexture = 0;
static GLuint toonRampTexture = 0;

//helpers
static void rebuildSolidList() {
    solidIndices.clear();
    for (int i = 0; i < (int)worldObjects.size(); i++) {
        if (worldObjects[i].active && worldObjects[i].solid) {
            solidIndices.push_back(i);
        }
    }
}

static std::string getDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return path.substr(0, pos + 1);
}

// Texture helpers
static GLuint createWhiteTexture() {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    unsigned char whitePixel[3] = { 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

static GLuint createToonRampTexture() {
    const int W = 256;
    unsigned char data[W * 4];

    for (int i = 0; i < W; i++) {
        float t = i / float(W - 1);

        float v = 0.10f;
        if      (t < 0.33f) v = 0.10f;
        else if (t < 0.75f) v = 0.55f;
        else                v = 1.00f;

        unsigned char c = (unsigned char)(v * 255.0f);
        data[i * 4 + 0] = c;
        data[i * 4 + 1] = c;
        data[i * 4 + 2] = c;
        data[i * 4 + 3] = 255;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return tex;
}

static GLuint createShaderProgram(const GLchar* vs, const GLchar* fs) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vs, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fs, nullptr);
    glCompileShader(fragmentShader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "inNormal");
    glBindAttribLocation(program, 2, "inTexCoord");
    glBindAttribLocation(program, 3, "inColor");

    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// MTL parsing (Kd only)
static std::unordered_map<std::string, glm::vec3> parseMtlKd(const std::string& mtlPath) {
    std::unordered_map<std::string, glm::vec3> kdColors;

    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        std::printf("Could not open MTL: %s\n", mtlPath.c_str());
        return kdColors;
    }

    std::string line;
    std::string currentMaterial;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "newmtl") {
            ss >> currentMaterial;
        } else if (token == "Kd" && !currentMaterial.empty()) {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            ss >> r >> g >> b;
            kdColors[currentMaterial] = glm::vec3(r, g, b);
        }
    }

    return kdColors;
}

// OBJ loading

static void parseObjVertexTriplet(const std::string& text, int& vIndex, int& vtIndex, int& vnIndex) {
    vIndex = 0; vtIndex = 0; vnIndex = 0;

    size_t slash1 = text.find('/');
    size_t slash2 = (slash1 == std::string::npos) ? std::string::npos : text.find('/', slash1 + 1);

    if (slash1 == std::string::npos) {
        vIndex = std::stoi(text);
        return;
    }

    vIndex = std::stoi(text.substr(0, slash1));

    if (slash2 == std::string::npos) {
        if (slash1 + 1 < text.size()) {
            vtIndex = std::stoi(text.substr(slash1 + 1));
        }
        return;
    }
    if (slash2 > slash1 + 1) {
        vtIndex = std::stoi(text.substr(slash1 + 1, slash2 - slash1 - 1));
    }
    if (slash2 + 1 < text.size()) {
        vnIndex = std::stoi(text.substr(slash2 + 1));
    }
}

static void appendFinalVertex(
    std::vector<float>& outData,
    const std::vector<glm::vec3>& vertices,
    const std::vector<glm::vec2>& uvs,
    const std::vector<glm::vec3>& normals,
    int vIndex, int vtIndex, int vnIndex,
    const glm::vec3& color
) {
    glm::vec3 v(0.0f);
    glm::vec2 uv(0.0f);
    glm::vec3 n(0.0f, 0.0f, 1.0f);

    if (vIndex  > 0 && vIndex  <= (int)vertices.size()) v = vertices[vIndex - 1];
    if (vtIndex > 0 && vtIndex <= (int)uvs.size())      uv = uvs[vtIndex - 1];
    if (vnIndex > 0 && vnIndex <= (int)normals.size())  n = normals[vnIndex - 1];

    outData.push_back(v.x);  outData.push_back(v.y);  outData.push_back(v.z);
    outData.push_back(uv.x); outData.push_back(uv.y);
    outData.push_back(n.x);  outData.push_back(n.y);  outData.push_back(n.z);
    outData.push_back(color.r);
    outData.push_back(color.g);
    outData.push_back(color.b);
}

static Model loadObjWithKdVertexColors(const std::string& objPath) {
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec2> tempUVs;
    std::vector<glm::vec3> tempNormals;

    std::vector<float> finalData;
    finalData.reserve(200000);

    std::ifstream file(objPath);
    if (!file.is_open()) {
        std::printf("ERROR: Could not open OBJ file: %s\n", objPath.c_str());
        std::exit(1);
    }

    std::unordered_map<std::string, glm::vec3> kdMap;
    glm::vec3 currentColor(1.0f);

    std::string objDir = getDirectory(objPath);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "mtllib") {
            std::string mtlName;
            ss >> mtlName;
            kdMap = parseMtlKd(objDir + mtlName);
        }
        else if (type == "usemtl") {
            std::string matName;
            ss >> matName;
            auto it = kdMap.find(matName);
            currentColor = (it != kdMap.end()) ? it->second : glm::vec3(1.0f);
        }
        else if (type == "v") {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            tempVertices.push_back(v);
        }
        else if (type == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            tempUVs.push_back(uv);
        }
        else if (type == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            tempNormals.push_back(n);
        }
        else if (type == "f") {
            std::vector<std::string> face;
            std::string token;
            while (ss >> token) face.push_back(token);
            if (face.size() < 3) continue;

            for (int i = 1; i < (int)face.size() - 1; i++) {
                int vIndex=0, vtIndex=0, vnIndex=0;

                parseObjVertexTriplet(face[0], vIndex, vtIndex, vnIndex);
                appendFinalVertex(finalData, tempVertices, tempUVs, tempNormals, vIndex, vtIndex, vnIndex, currentColor);

                parseObjVertexTriplet(face[i], vIndex, vtIndex, vnIndex);
                appendFinalVertex(finalData, tempVertices, tempUVs, tempNormals, vIndex, vtIndex, vnIndex, currentColor);

                parseObjVertexTriplet(face[i + 1], vIndex, vtIndex, vnIndex);
                appendFinalVertex(finalData, tempVertices, tempUVs, tempNormals, vIndex, vtIndex, vnIndex, currentColor);
            }
        }
    }

    Model model;
    model.numVerts = (int)finalData.size() / 11;

    glGenBuffers(1, &model.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(finalData.size() * sizeof(float)), finalData.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &model.vao);
    glBindVertexArray(model.vao);

    const int stride = 11 * (int)sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    std::printf("OBJ Loaded: %s (%d verts)\n", objPath.c_str(), model.numVerts);
    return model;
}

// Level map loading

static void loadMap(const std::string& fileName) {
    std::ifstream mapFile(fileName);
    if (!mapFile.is_open()) {
        std::printf("ERROR: Could not open map file: %s\n", fileName.c_str());
        std::exit(1);
    }

    mapData.clear();
    mapFile >> mapWidth >> mapHeight;

    std::string line;
    std::getline(mapFile, line);

    for (int row = 0; row < mapHeight; row++) {
        std::getline(mapFile, line);
        if ((int)line.size() < mapWidth) {
            std::printf("ERROR: Bad map line %d\n", row);
            std::exit(1);
        }

        line = line.substr(0, (size_t)mapWidth);
        mapData.push_back(line);

        for (int col = 0; col < mapWidth; col++) {
            if (line[col] == 'S') {
                playerSpawn = glm::vec3((float)col, (float)row, 0.0f);
            }
        }
    }
    int gCount = 0;
    for (int r = 0; r < mapHeight; r++)
        for (int c = 0; c < mapWidth; c++)
            if (mapData[r][c] == 'G') gCount++;

    collectTarget = (gCount > 0) ? gCount : 1;
    timeLimitSec = gCount * 8.0f;

    std::printf("Map Loaded: %s (%dx%d)\n", fileName.c_str(), mapWidth, mapHeight);
}

// World building
// Map legend:
//  S start (solid)
//  P platform (solid)
//  B bridge (solid)
//  F fence (solid)
//  X spikes (hazard)
//  G gem (collectible)
//  E exit/flag (goal)
//  T tree (deco)
//  U bush (deco)
static void addObject(
    const Model& model,
    ObjType type,
    const glm::vec3& position,
    const glm::vec3& scale,
    float yawRadians,
    bool outlined,
    float outlineMult,
    bool solid,
    float triggerRadius
) {
    GameObject obj;
    obj.model = &model;
    obj.type = type;
    obj.active = true;

    obj.pos = position;
    obj.scale = scale * 0.8f;
    obj.yaw = yawRadians;

    obj.outlined = outlined;
    obj.outlineMult = outlineMult;

    obj.solid = solid;
    obj.triggerRadius = triggerRadius;
    obj.halfExtents = 0.5f * obj.scale;

    worldObjects.push_back(obj);
}

static void buildWorld(
    const Model& platformModel,
    const Model& platformModel2,
    const Model& bridgeModel,
    const Model& spikesModel,
    const Model& gemModel,
    const Model& flagModel,
    const Model& treeModel,
    const Model& bushModel,
    const Model& fenceModel
) {
    worldObjects.clear();
    auto addGround = [&](const glm::vec3& p) {
    addObject(platformModel, ObjType::Solid, p, glm::vec3(1.0f), 0.0f, true, 1.02f, true, 0.0f);
};
    for (int row = 0; row < mapHeight; row++) {
        for (int col = 0; col < mapWidth; col++) {
            char tile = mapData[row][col];
            glm::vec3 basePos((float)col, (float)row, 0.0f);

            if (tile == 'S') {
                addObject(platformModel, ObjType::Solid, basePos, glm::vec3(1.0f), 0.0f, true, 1.02f, true, 0.0f);
            } else if (tile == 'P') {
                addObject(platformModel, ObjType::Solid, basePos, glm::vec3(1.0f), 0.0f, true, 1.02f, true, 0.0f);
            } 
             else if (tile == 'Y') {
                addObject(platformModel2, ObjType::Solid, basePos, glm::vec3(1.0f), 0.0f, true, 1.02f, true, 0.0f);
            } 
            else if (tile == 'B') {
                addObject(bridgeModel, ObjType::Solid, basePos, glm::vec3(1.0f), 0.0f, true, 1.02f, true, 0.0f);
            } else if (tile == 'F') {
                addObject(fenceModel, ObjType::Solid, basePos, glm::vec3(1.0f), 0.0f, true, 1.02f, true, 0.0f);
            } else if (tile == 'X') {
                addGround(basePos);
                addObject(spikesModel, ObjType::Hazard, basePos, glm::vec3(0.8f), 0.0f, true, 1.02f, false, 0.85f);
            } else if (tile == 'G') {
                addGround(basePos);
                addObject(gemModel, ObjType::Collectible, basePos + glm::vec3(0,0,0.45f), glm::vec3(1.0f), 0.0f, true, 1.06f, false, 0.75f);
            } else if (tile == 'E') {
                addGround(basePos);
                addObject(flagModel, ObjType::Goal, basePos, glm::vec3(1.0f), 0.0f, true, 1.02f, false, 0.95f);
            } else if (tile == 'T') {
                addGround(basePos);
                addObject(treeModel, ObjType::Deco, basePos, glm::vec3(1.0f), 0.0f, true, 1.01f, false, 0.0f);
            } else if (tile == 'U') {
                addGround(basePos);
                addObject(bushModel, ObjType::Deco, basePos, glm::vec3(1.0f), 0.0f, true, 1.01f, false, 0.0f);
                
            } 
        }
    }
    rebuildSolidList();
}

static float groundHeightFromSolids(float x, float y) {
    float bestTopZ = -10000.0f;

    for (int idx : solidIndices) {
        const GameObject& obj = worldObjects[idx];
        glm::vec3 he = obj.halfExtents;

        float minX = obj.pos.x - he.x;
        float maxX = obj.pos.x + he.x;
        float minY = obj.pos.y - he.y;
        float maxY = obj.pos.y + he.y;
        float pad = playerRadius * 0.9f;

        if (x >= minX - pad && x <= maxX + pad && y >= minY - pad && y <= maxY + pad) {
            float topZ = obj.pos.z + (he.z * 2.0f);
            if (topZ > bestTopZ) bestTopZ = topZ;
        }

    }

    return bestTopZ; 
}
//Level reset + interactions
static void resetLevel() {
    playerPos = playerSpawn;

    float spawnGround = groundHeightFromSolids(playerSpawn.x, playerSpawn.y);
    if (spawnGround > -9999.0f) {
        playerPos.z = spawnGround;
        isGrounded = true;
    } else {
        isGrounded = false;
    }

    zVelocity = 0.0f;

    timeLeftSec = timeLimitSec;
    collectedCount = 0;

    levelWon = false;
    levelLost = false;

    for (auto& obj : worldObjects) {
        if (obj.type == ObjType::Collectible) obj.active = true;
    }
    rebuildSolidList();
}

static void updateInteractions(float dt) {
    if (!levelWon && !levelLost) {
        timeLeftSec -= dt;
        if (timeLeftSec <= 0.0f) {
            timeLeftSec = 0.0f;
            levelLost = true;
        }
    }

    if (!levelWon && !levelLost && playerPos.z < -3.0f) {
        levelLost = true;
    }

    if (levelWon || levelLost) return;

    for (auto& obj : worldObjects) {
        if (!obj.active) continue;

        float dx = playerPos.x - obj.pos.x;
        float dy = playerPos.y - obj.pos.y;
        float dz = playerPos.z - obj.pos.z;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (obj.type == ObjType::Collectible) {
            if (dist < obj.triggerRadius) {
                obj.active = false;
                collectedCount++;
            }
        }
        else if (obj.type == ObjType::Hazard) {
            if (dist < obj.triggerRadius) {
                levelLost = true;
                return;
            }
        }
        else if (obj.type == ObjType::Goal) {
            if (dist < obj.triggerRadius) {
                if (collectedCount >= collectTarget) levelWon = true;
            }
        }
    }
}
// Matrices + drawing (simple, readable)
static glm::mat4 buildModelMatrix(const glm::vec3& position, float yawRadians, const glm::vec3& scale) {
    glm::mat4 model(1.0f);

    model = glm::translate(model, position);
    model = glm::rotate(model, yawRadians, glm::vec3(0,0,1));
    model = glm::rotate(model, assetAxisFixEuler.z, glm::vec3(0,0,1));
    model = glm::rotate(model, assetAxisFixEuler.x, glm::vec3(1,0,0));
    model = glm::rotate(model, assetAxisFixEuler.y, glm::vec3(0,1,0));

    model = glm::scale(model, scale);
    return model;
}

static void drawOutlinedModel(const Model& model, const glm::mat4& modelMatrix, float outlineScale) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);
    glUniform1f(uniformUvScale, 1.0f);

    glCullFace(GL_FRONT);
    glUniform1i(uniformIsOutline, 1);

    glm::mat4 outlineMatrix = glm::scale(modelMatrix, glm::vec3(outlineScale));
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(outlineMatrix));

    glBindVertexArray(model.vao);
    glDrawArrays(GL_TRIANGLES, 0, model.numVerts);

    glCullFace(GL_BACK);
    glUniform1i(uniformIsOutline, 0);

    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glDrawArrays(GL_TRIANGLES, 0, model.numVerts);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Window* window = SDL_CreateWindow("NPR Platformer", screenWidth, screenHeight, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_SetWindowRelativeMouseMode(window, true);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::printf("Failed to initialize GLAD\n");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);


    Model platformModel = loadObjWithKdVertexColors("models/platform.obj");
    Model platformModel2 = loadObjWithKdVertexColors("models/platform2.obj");
    Model bridgeModel   = loadObjWithKdVertexColors("models/bridge.obj");
    Model spikesModel   = loadObjWithKdVertexColors("models/spikes.obj");
    Model gemModel      = loadObjWithKdVertexColors("models/gem.obj");
    Model flagModel     = loadObjWithKdVertexColors("models/flag.obj");
    Model treeModel     = loadObjWithKdVertexColors("models/tree.obj");
    Model bushModel     = loadObjWithKdVertexColors("models/bush.obj");
    Model fenceModel    = loadObjWithKdVertexColors("models/fence.obj");
    Model playerModel   = loadObjWithKdVertexColors("models/player.obj");

    auto loadCurrentLevel = [&]() {
        loadMap(levelFiles[currentLevel]);
        buildWorld(platformModel, platformModel2, bridgeModel, spikesModel,
                gemModel, flagModel, treeModel, bushModel, fenceModel);
        resetLevel();
    };

    loadCurrentLevel();


    shaderProgram = createShaderProgram(vertexSource, fragmentSource);
    glUseProgram(shaderProgram);

    uniformModel = glGetUniformLocation(shaderProgram, "model");
    uniformView  = glGetUniformLocation(shaderProgram, "view");
    uniformProj  = glGetUniformLocation(shaderProgram, "proj");
    uniformUvScale = glGetUniformLocation(shaderProgram, "uvScale");
    uniformViewPos = glGetUniformLocation(shaderProgram, "viewPos");
    uniformIsOutline = glGetUniformLocation(shaderProgram, "isOutline");
    uniformObjectColor = glGetUniformLocation(shaderProgram, "objectColor");

    GLint uniformTex0 = glGetUniformLocation(shaderProgram, "tex0");
    GLint uniformRamp = glGetUniformLocation(shaderProgram, "toonRamp");
    glUniform1i(uniformTex0, 0);
    glUniform1i(uniformRamp, 1);

    GLint uniformToonBands = glGetUniformLocation(shaderProgram, "toonBands");
    GLint uniformMinIntensity = glGetUniformLocation(shaderProgram, "minIntensity");
    GLint uniformRimThreshold = glGetUniformLocation(shaderProgram, "rimThreshold");
    GLint uniformRimIntensity = glGetUniformLocation(shaderProgram, "rimIntensity");

    glUniform1i(uniformToonBands, 3);
    glUniform1f(uniformMinIntensity, 0.20f);
    glUniform1f(uniformRimThreshold, 0.90f);
    glUniform1f(uniformRimIntensity, 0.06f);

    glm::mat4 projMatrix = glm::perspective(glm::radians(45.0f),
                                           (float)screenWidth / (float)screenHeight,
                                           0.1f, 150.0f);
    glUniformMatrix4fv(uniformProj, 1, GL_FALSE, glm::value_ptr(projMatrix));

    //Textures
    whiteTexture = createWhiteTexture();
    toonRampTexture = createToonRampTexture();

    glUniform3f(uniformObjectColor, 1.0f, 1.0f, 1.0f);


    SDL_Event event;
    bool quit = false;

    float lastTimeMs = (float)SDL_GetTicks();

    while (!quit) {
        float nowMs = (float)SDL_GetTicks();
        float dt = (nowMs - lastTimeMs) / 1000.0f;
        lastTimeMs = nowMs;
        if (dt > 0.1f) dt = 0.1f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) quit = true;
            if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE) quit = true;

            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                camYaw   -= event.motion.xrel * mouseSens;
                camPitch -= event.motion.yrel * mouseSens;

                const float pitchMin = -1.2f;
                const float pitchMax = -0.15f;
                camPitch = glm::clamp(camPitch, pitchMin, pitchMax);
            }
        }

        const bool* keyState = SDL_GetKeyboardState(NULL);
        if ((levelWon || levelLost) && keyState[SDL_SCANCODE_R]) {
            resetLevel();
        }

        //movement + physics
        if (!levelWon && !levelLost) {
            glm::vec3 forward(std::cos(camYaw), std::sin(camYaw), 0.0f);
            forward = glm::normalize(forward);

            glm::vec3 right(forward.y, -forward.x, 0.0f);
            right = glm::normalize(right);

            glm::vec3 moveDir(0.0f);
            if (keyState[SDL_SCANCODE_W]) moveDir += forward;
            if (keyState[SDL_SCANCODE_S]) moveDir -= forward;
            if (keyState[SDL_SCANCODE_D]) moveDir += right;
            if (keyState[SDL_SCANCODE_A]) moveDir -= right;

            if (glm::length(moveDir) > 0.0f) {
                moveDir = glm::normalize(moveDir);
                playerPos += moveDir * playerSpeed * dt;

                playerYaw = std::atan2(moveDir.y, moveDir.x);
            }

            if (keyState[SDL_SCANCODE_SPACE] && isGrounded) {
                zVelocity = jumpStrength;
                isGrounded = false;
            }

            zVelocity -= gravity * dt;
            playerPos.z += zVelocity * dt;

            float groundH = groundHeightFromSolids(playerPos.x, playerPos.y);
            if (groundH > -9999.0f && playerPos.z <= groundH) {
                playerPos.z = groundH;
                zVelocity = 0.0f;
                isGrounded = true;
            }

            updateInteractions(dt);
            if (levelWon) {
            currentLevel++;
            if (currentLevel >= (int)levelFiles.size()) currentLevel = 0;
            loadCurrentLevel();
        }
        }

        //window title
        {
            char title[256];
            const char* stateStr = levelWon ? "WIN! (R to restart)" : (levelLost ? "LOSE! (R to restart)" : "RUN!");
            std::snprintf(title, 256, "NPR Platformer | %s | Gems %d/%d | Time %.1fs",
                          stateStr, collectedCount, collectTarget, timeLeftSec);
            SDL_SetWindowTitle(window, title);
        }

        // Camera update
        glm::vec3 orbitDir(
            std::cos(camPitch) * std::cos(camYaw),
            std::cos(camPitch) * std::sin(camYaw),
            std::sin(camPitch)
        );
        orbitDir = glm::normalize(orbitDir);

        glm::vec3 desiredCameraPos = playerPos - orbitDir * camDist;
        desiredCameraPos.z += camHeight;

        float alpha = 1.0f - std::exp(-camSmooth * dt);
        cameraPos = glm::mix(cameraPos, desiredCameraPos, alpha);

        glm::vec3 lookTarget = playerPos + camLookOffset;
        glm::mat4 viewMatrix = glm::lookAt(cameraPos, lookTarget, cameraUp);

        // Render
        glClearColor(0.55f, 0.75f, 0.95f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniform3f(uniformViewPos, cameraPos.x, cameraPos.y, cameraPos.z);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, toonRampTexture);

        // Draw world
        float timeSeconds = nowMs * 0.001f;

        for (const GameObject& obj : worldObjects) {
            if (!obj.active) continue;

            glm::vec3 renderPos = obj.pos;
            float extraYaw = 0.0f;
            if (obj.type == ObjType::Collectible) {
                renderPos.z += 0.15f * std::sin(timeSeconds * 2.5f);
                extraYaw = timeSeconds * 1.5f;
            }

            glm::mat4 modelMatrix = buildModelMatrix(renderPos, obj.yaw + extraYaw, obj.scale);
            drawOutlinedModel(*obj.model, modelMatrix, obj.outlineMult);
        }

        // Draw player
        glm::vec3 playerRenderPos = playerPos + glm::vec3(0,0, playerFeetVisualOffset * playerScale);
        glm::mat4 playerMatrix = buildModelMatrix(
            playerRenderPos,
            playerYaw + playerModelYawOffset,
            glm::vec3(playerScale)
        );
        drawOutlinedModel(playerModel, playerMatrix, playerOutlineScale);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DestroyContext(context);
    SDL_Quit();
    return 0;
}
