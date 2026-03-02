#include "glad/glad.h"
#if defined(__APPLE__) || defined(__linux__)
 #include <SDL3/SDL.h>
 #include <SDL3/SDL_opengl.h>
#else
 #include <SDL3/SDL.h>
 #include <SDL3/SDL_opengl.h>
#endif

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int screenWidth = 1200;
int screenHeight = 1000;

//map
int mapWidth, mapHeight;
std::vector<std::string> mapData;

int currentLevel = 1;
bool keys[5] = { false, false, false, false, false };
float playerRadius = 0.35f;
// Camera
glm::vec3 cameraPos   = glm::vec3(5.0f, 5.0f, 0.0f); 
glm::vec3 cameraFront = glm::vec3(1.0f, 0.0f, 0.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);
float cameraSpeed     = 2.5f;
float turnSpeed       = 90.0f;


float zVelocity = 0.0f;
float gravity = 9.0f;
float jumpStrength = 3.0f;

//shaders
const GLchar* vertexSource =
    "#version 150 core\n"
    "in vec3 position;"
    "in vec3 inNormal;"
    "in vec2 inTexCoord;"
    "out vec3 Normal;"
    "out vec3 FragPos;"
    "out vec2 TexCoord;"
    "uniform mat4 model;"
    "uniform mat4 view;"
    "uniform mat4 proj;"
    "uniform float uvScale;"
    "void main() {"
    "   TexCoord = inTexCoord * uvScale;"
    "   Normal = mat3(transpose(inverse(model))) * inNormal;"
    "   FragPos = vec3(model * vec4(position, 1.0));"
    "   gl_Position = proj * view * model * vec4(position,1.0);"
    "}";
const GLchar* fragmentSource =
    "#version 150 core\n"
    "in vec3 Normal;"
    "in vec3 FragPos;"
    "in vec2 TexCoord;"
    "out vec4 outColor;"
    "uniform sampler2D tex0;"
    "uniform vec3 objectColor;"
    "uniform vec3 viewPos;"
    "uniform vec3 viewDir;"
    
    "void main() {"
    "   vec4 texColor = texture(tex0, TexCoord);"
    "   vec3 baseColor = texColor.rgb * objectColor;"
    "   float ambientStrength = 0.25;"
    "   vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);"
    
        //flashlight
    "   vec3 lightPos = viewPos;"
    "   vec3 lightDirVector = normalize(lightPos - FragPos);"

    "   float theta = dot(lightDirVector, normalize(-viewDir));"
    "   float cutOff = 0.90;"
    
    "   vec3 diffuse = vec3(0.0);"
    "   if(theta > cutOff) {" 
    "       vec3 norm = normalize(Normal);"
    "       float diff = max(dot(norm, lightDirVector), 0.0);"
    "       diffuse = diff * vec3(0.5, 0.45, 0.4);"
    "   }"
    "   float distance = length(lightPos - FragPos);"
    "   float attenuation = 1.0 / (1.0 + 0.3 * distance + 0.05 * distance * distance);"
    "   vec3 result = (ambient + diffuse * attenuation) * baseColor;"
    "   outColor = vec4(result, 1.0);"
    "}";

struct Model {
    GLuint vao;
    GLuint vbo;
    int numVerts;
};


Model loadOBJ(std::string fileName) {
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
    std::vector<glm::vec3> temp_normals;
    std::vector<float> finalData;

    std::ifstream file(fileName);
    if (!file.is_open()) {
        printf("ERROR: Could not open OBJ file: %s\n", fileName.c_str());
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;
        if (type == "v") { 
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            temp_vertices.push_back(v);
        } 
        else if (type == "vt") { 
            glm::vec2 vt;
            ss >> vt.x >> vt.y;
            temp_uvs.push_back(vt);
        } 
        else if (type == "vn") { 
            glm::vec3 vn;
            ss >> vn.x >> vn.y >> vn.z;
            temp_normals.push_back(vn);
        } else if (type == "f") { 
            std::vector<std::string> faceVerts;
            std::string val;
            while (ss >> val) {
                faceVerts.push_back(val);
            }

            for (int i = 1; i < faceVerts.size() - 1; i++) {
                std::string triVerts[3] = { faceVerts[0], faceVerts[i], faceVerts[i+1] };

                for (int j = 0; j < 3; j++) {
                    std::string vertexData = triVerts[j];
                    
                    int vIdx = 0, vtIdx = 0, vnIdx = 0;
                    size_t firstSlash = vertexData.find('/');
                    size_t secondSlash = vertexData.find('/', firstSlash + 1);

                    if (firstSlash != std::string::npos) {
                        vIdx = std::stoi(vertexData.substr(0, firstSlash));
                        
                        if (secondSlash != std::string::npos) {
                            if (secondSlash > firstSlash + 1) {
                                vtIdx = std::stoi(vertexData.substr(firstSlash + 1, secondSlash - firstSlash - 1));
                            }
                            vnIdx = std::stoi(vertexData.substr(secondSlash + 1));
                        } 
                        else {
                            vtIdx = std::stoi(vertexData.substr(firstSlash + 1));
                        }
                    } 
                    else {
                        vIdx = std::stoi(vertexData);
                    }
                    
                    if (vIdx > 0) {
                        glm::vec3 v = temp_vertices[vIdx - 1];
                        finalData.push_back(v.x); finalData.push_back(v.y); finalData.push_back(v.z);
                    }
                    
                    if (vtIdx > 0) {
                        glm::vec2 vt = temp_uvs[vtIdx - 1];
                        finalData.push_back(vt.x); finalData.push_back(vt.y);
                    } else {
                        finalData.push_back(0.0f); finalData.push_back(0.0f);
                    }

                    if (vnIdx > 0) {
                        glm::vec3 vn = temp_normals[vnIdx - 1];
                        finalData.push_back(vn.x); finalData.push_back(vn.y); finalData.push_back(vn.z);
                    } 
                    else {
                        finalData.push_back(0.0f); finalData.push_back(0.0f); finalData.push_back(1.0f);
                    }
                }
            }
        }
    }
    file.close();

    Model m;
    m.numVerts = finalData.size() / 8;
    
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, finalData.size() * sizeof(float), finalData.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    printf("OBJ Loaded: %s (%d vertices)\n", fileName.c_str(), m.numVerts);
    return m;
}
Model loadModel(std::string fileName) {
    Model m;
    std::ifstream modelFile(fileName);
    
    if (!modelFile.is_open()) {
        printf("ERROR: Could not open model file: %s\n", fileName.c_str());
        exit(1);
    }

    int numLines = 0;
    modelFile >> numLines;

    float* modelData = new float[numLines];
    for (int i = 0; i < numLines; i++) {
        modelFile >> modelData[i];
    }
    m.numVerts = numLines / 8; 
    modelFile.close();

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, numLines * sizeof(float), modelData, GL_STATIC_DRAW);
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    GLint posAttrib = 0; 
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
    glEnableVertexAttribArray(posAttrib);

    GLint colAttrib = 1;
    glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(colAttrib);

    GLint texAttrib = 2;
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(texAttrib);
    glBindVertexArray(0);
    delete[] modelData;

    return m;
}

void loadMap(std::string fileName) {
    std::ifstream mapFile(fileName);
    if (mapFile.is_open()) {
        mapFile >> mapWidth >> mapHeight; 
        std::string line;
        int row = 0;
        while (std::getline(mapFile, line)) {
            if (line.length() > 0) {
                mapData.push_back(line);
                
                for (int col = 0; col < line.length(); col++) {
                    if (line[col] == 'S') {
                        cameraPos.x = (float)col;
                        cameraPos.y = (float)row;
                        cameraPos.z = 0.0f;
                        printf("Start Position found at: %.1f, %.1f\n", cameraPos.x, cameraPos.y);
                    }
                }
                row++;
            }
        }
        mapFile.close();
        printf("Map Loaded: %dx%d\n", mapWidth, mapHeight);
    } else {
        printf("ERROR: Could not open map file: %s\n", fileName.c_str());
        exit(1);
    }
}

GLuint loadTexture(const char* filename) {
    int width, height, nrChannels;
    
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 3); 

    if (!data) {
        printf("Failed to load texture: %s\n", filename);
        unsigned char whitePixel[] = { 255, 255, 255 };
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
        return texture; 
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    printf("Texture Loaded: %s (Size: %dx%d)\n", filename, width, height);
    return texture;
}
GLuint createShaderProgram(const GLchar* vertexSrc, const GLchar* fragmentSrc) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glBindAttribLocation(shaderProgram, 0, "position");
    glBindAttribLocation(shaderProgram, 1, "inNormal");
    glBindAttribLocation(shaderProgram, 2, "inTexCoord");
    glLinkProgram(shaderProgram);
    return shaderProgram;
}
void loadLevel(int levelNum) {
    mapData.clear();
    for(int i=0; i<5; i++) keys[i] = false;
    
    char filename[50];
    snprintf(filename, 50, "models/level%d.txt", levelNum);
    loadMap(filename);
}
bool isWalkable(float x, float y) {
    int gridX = (int)(x + 0.5f); 
    int gridY = (int)(y + 0.5f);

    if (gridX < 0 || gridX >= mapWidth || gridY < 0 || gridY >= mapHeight) return false;
    char tile = mapData[gridY][gridX];
    if (tile == 'W') return false;

    if (tile >= 'A' && tile <= 'E') {
        int doorIndex = tile - 'A'; 
        if (keys[doorIndex] == false) {
            return false;
        }
    }
    return true;
}
void checkInteraction() {
    int gridX = (int)(cameraPos.x + 0.5f);
    int gridY = (int)(cameraPos.y + 0.5f);
    char tile = mapData[gridY][gridX];

    if (tile >= 'a' && tile <= 'e') {
        int keyIndex = tile - 'a';
        keys[keyIndex] = true;
        mapData[gridY][gridX] = '0';
        printf("Picked up Key %c!\n", tile);
    }
    
    else if (tile == 'G') {
        printf("\n*** LEVEL %d COMPLETE! ***\n", currentLevel);
        currentLevel++;
        if (currentLevel > 3) {
            printf("YOU WIN THE GAME!\n");
            exit(0);
        } else {
            loadLevel(currentLevel);
        }
    }
}
int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Window* window = SDL_CreateWindow("Project 4", screenWidth, screenHeight, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(window);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)){
        printf("Failed to initialize GLAD\n");
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    loadLevel(currentLevel);
    Model cubeModel = loadModel("models/cube.txt");
    Model keyModel = loadOBJ("models/key.obj");
    Model gateModel = loadOBJ("models/gate.obj");
    Model gemModel = loadOBJ("models/gem.obj");

    GLuint wallTex = loadTexture("models/wall.bmp");
    GLuint woodTex  = loadTexture("models/floor.bmp");
    GLuint gateTex  = loadTexture("models/gatetex.bmp");
    GLuint goldtex  = loadTexture("models/gold.bmp");

    GLuint shaderProgram = createShaderProgram(vertexSource, fragmentSource);
    glUseProgram(shaderProgram);
    GLint texLoc = glGetUniformLocation(shaderProgram, "tex0");
    glUniform1i(texLoc, 0);

    GLint uniModel = glGetUniformLocation(shaderProgram, "model");
    GLint uniView  = glGetUniformLocation(shaderProgram, "view");
    GLint uniProj  = glGetUniformLocation(shaderProgram, "proj");
    GLint uniColor = glGetUniformLocation(shaderProgram, "objectColor");
    GLint uniUVScale = glGetUniformLocation(shaderProgram, "uvScale");
    GLint uniViewPos = glGetUniformLocation(shaderProgram, "viewPos");
    GLint uniViewDir = glGetUniformLocation(shaderProgram, "viewDir");

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)screenWidth / screenHeight, 0.1f, 100.0f);
    glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

    SDL_Event windowEvent;
    bool quit = false;
    float lastTime = SDL_GetTicks();

    auto state = SDL_GetKeyboardState(NULL);

    while (!quit) {
        float currentTime = SDL_GetTicks();
        float dt = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (dt > 0.1f) dt = 0.1f;

        while (SDL_PollEvent(&windowEvent)) {
            if (windowEvent.type == SDL_EVENT_QUIT) quit = true;
            if (windowEvent.type == SDL_EVENT_KEY_UP && windowEvent.key.key == SDLK_ESCAPE) quit = true;
        }

        // Movement
        glm::vec3 velocity(0.0f);
        if (state[SDL_SCANCODE_W]) velocity += cameraFront;
        if (state[SDL_SCANCODE_S]) velocity -= cameraFront;
        if (state[SDL_SCANCODE_A]) velocity -= glm::normalize(glm::cross(cameraFront, cameraUp));
        if (state[SDL_SCANCODE_D]) velocity += glm::normalize(glm::cross(cameraFront, cameraUp));
        velocity.z = 0;

        if (glm::length(velocity) > 0) {
            velocity = glm::normalize(velocity) * cameraSpeed * dt;
            float collisionRadius = 0.3f; 

            float newX = cameraPos.x + velocity.x;
            if (isWalkable(newX, cameraPos.y) && 
                isWalkable(newX + collisionRadius, cameraPos.y) && 
                isWalkable(newX - collisionRadius, cameraPos.y)) {
                cameraPos.x = newX;
            }

            float newY = cameraPos.y + velocity.y;
            if (isWalkable(cameraPos.x, newY) && 
                isWalkable(cameraPos.x, newY + collisionRadius) && 
                isWalkable(cameraPos.x, newY - collisionRadius)) {
                cameraPos.y = newY;
            }
        }
        checkInteraction();


        int gridX = (int)(cameraPos.x + 0.5f); 
        int gridY = (int)(cameraPos.y + 0.5f);
        char tile = mapData[gridY][gridX];

        if (tile >= 'A' && tile <= 'E') {
            mapData[gridY][gridX] = '0'; 
            int keyID = tile - 'A';
            keys[keyID] = false; 
            printf("Used Key to open Door %c!\n", tile);
        }

        // Turning
        if (state[SDL_SCANCODE_LEFT]) {
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(turnSpeed * dt), cameraUp);
            cameraFront = glm::vec3(rotation * glm::vec4(cameraFront, 1.0f));
        }
        if (state[SDL_SCANCODE_RIGHT]) {
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-turnSpeed * dt), cameraUp);
            cameraFront = glm::vec3(rotation * glm::vec4(cameraFront, 1.0f));
        }

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
        //jumping
        if (state[SDL_SCANCODE_SPACE] && cameraPos.z <= 0.05f) {
            zVelocity = jumpStrength; // Launch up
        }
        zVelocity -= gravity * dt;
        cameraPos.z += zVelocity * dt;
        if (cameraPos.z < 0.0f) {
            cameraPos.z = 0.0f;
            zVelocity = 0.0f;
        }
        if (cameraPos.z > 0.8f) {
            cameraPos.z = 0.8f;
            zVelocity = -0.5f;
        }

        checkInteraction();

        glClearColor(0.1f, 0.1f, 0.2f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUniform3f(uniViewPos, cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(uniViewDir, cameraFront.x, cameraFront.y, cameraFront.z);

        //floor
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTex);
        glUniform1f(uniUVScale, 20.0f);
        glUniform3f(uniColor, 1.0f, 1.0f, 1.0f);

        glm::mat4 floorModel = glm::mat4(1.0f);
        floorModel = glm::translate(floorModel, glm::vec3(mapWidth/2.0f - 0.5f, mapHeight/2.0f - 0.5f, -0.5f));
        floorModel = glm::scale(floorModel, glm::vec3(mapWidth, mapHeight, 0.1f));
        glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(floorModel));
        glBindVertexArray(cubeModel.vao);
        glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVerts);
        glUniform1f(uniUVScale, 1.0f);

        //draw map
        for (int row = 0; row < mapHeight; row++) {
            for (int col = 0; col < mapWidth; col++) {
                char tile = mapData[row][col];
                if (tile == '0') continue; 

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3((float)col, (float)row, 0.0f));
                glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));
                
                glUniform3f(uniColor, 1.0f, 1.0f, 1.0f); 

                if (tile == 'W') {
                    glBindTexture(GL_TEXTURE_2D, wallTex);
                    glBindVertexArray(cubeModel.vao);
                    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVerts);
                } 
                
                // doors
                else if (tile >= 'A' && tile <= 'E') {
                    glBindTexture(GL_TEXTURE_2D, gateTex);
                    
                    if (tile == 'A') glUniform3f(uniColor, 1.0f, 0.0f, 0.0f); // Red
                    if (tile == 'B') glUniform3f(uniColor, 0.0f, 0.0f, 1.0f); // Blue
                    if (tile == 'C') glUniform3f(uniColor, 0.0f, 1.0f, 0.0f); // Green
                    if (tile == 'D') glUniform3f(uniColor, 1.0f, 1.0f, 0.0f); // Yellow
                    if (tile == 'E') glUniform3f(uniColor, 1.0f, 0.0f, 1.0f); // Purple
                    bool isHorizontal = false;
                    if (col > 0 && col < mapWidth-1) {
                        if (mapData[row][col-1] == 'W' || mapData[row][col+1] == 'W') {
                            isHorizontal = true;
                        }
                    }
                    
                    glm::mat4 gateMat = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.4f));
                    gateMat = glm::rotate(gateMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    if (!isHorizontal) {
                        gateMat = glm::rotate(gateMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    
                    gateMat = glm::scale(gateMat, glm::vec3(0.4f));
                    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(gateMat));
                    glBindVertexArray(gateModel.vao); 
                    glDrawArrays(GL_TRIANGLES, 0, gateModel.numVerts);
                }

                //keys
                else if (tile >= 'a' && tile <= 'e') {
                    glBindTexture(GL_TEXTURE_2D, goldtex);
                    if (tile == 'a') glUniform3f(uniColor, 1.0f, 0.0f, 0.0f);
                    if (tile == 'b') glUniform3f(uniColor, 0.0f, 0.0f, 1.0f);
                    if (tile == 'c') glUniform3f(uniColor, 0.0f, 1.0f, 0.0f);
                    if (tile == 'd') glUniform3f(uniColor, 1.0f, 1.0f, 0.0f);
                    if (tile == 'e') glUniform3f(uniColor, 1.0f, 0.0f, 1.0f);

                    float angle = SDL_GetTicks() / 1000.0f;
                    glm::mat4 keyMat = glm::rotate(model, angle, glm::vec3(0,0,1));
                    keyMat = glm::scale(keyMat, glm::vec3(0.005f));
                    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(keyMat));

                    glBindVertexArray(keyModel.vao);
                    glDrawArrays(GL_TRIANGLES, 0, keyModel.numVerts);
                }

                //goal
                else if (tile == 'G') {
                    glUniform3f(uniColor, 1.0f, 0.8f, 0.0f);
                    glBindTexture(GL_TEXTURE_2D, goldtex);
                    
                    float angle = SDL_GetTicks() / 1000.0f;
                    glm::mat4 gemMat = glm::rotate(model, angle, glm::vec3(0,1,0));
                    gemMat = glm::scale(gemMat, glm::vec3(0.5f));


                    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(gemMat));
                    
                    glBindVertexArray(gemModel.vao);
                    glDrawArrays(GL_TRIANGLES, 0, gemModel.numVerts);
                }
            }
        }
        glClear(GL_DEPTH_BUFFER_BIT);

        glm::mat4 hudView = glm::mat4(1.0f); 
        glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(hudView));

        int keysDrawn = 0;
        for (int i = 0; i < 5; i++) {
            if (keys[i]) { 
                glBindTexture(GL_TEXTURE_2D, woodTex); 
                float xPos = -0.5f + (keysDrawn * 0.25f); 
                glm::mat4 heldKey = glm::mat4(1.0f);
                heldKey = glm::translate(heldKey, glm::vec3(xPos, -0.5f, -2.0f)); 
                heldKey = glm::rotate(heldKey, glm::radians(90.0f), glm::vec3(1,0,0));
                heldKey = glm::rotate(heldKey, glm::radians(45.0f), glm::vec3(0,1,0));
                heldKey = glm::scale(heldKey, glm::vec3(0.005f)); 

                // Colors
                if (i==0) glUniform3f(uniColor, 1.0f, 0.0f, 0.0f); 
                if (i==1) glUniform3f(uniColor, 0.0f, 0.0f, 1.0f); 
                if (i==2) glUniform3f(uniColor, 0.0f, 1.0f, 0.0f); 
                if (i==3) glUniform3f(uniColor, 1.0f, 1.0f, 0.0f); 
                if (i==4) glUniform3f(uniColor, 1.0f, 0.0f, 1.0f); 

                glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(heldKey));
                glBindVertexArray(keyModel.vao);
                glDrawArrays(GL_TRIANGLES, 0, keyModel.numVerts);
                keysDrawn++;
            }
        }
        
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DestroyContext(context);
    SDL_Quit();
    return 0;
}