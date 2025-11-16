// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

// Include GLEW and GLFW
#include "dependente\glew\glew.h"
#include "dependente\glfw\glfw3.h"

// Include GLM
#include "dependente\glm\glm.hpp"
#include "dependente\glm\gtc\matrix_transform.hpp"
#include "dependente\glm\gtc\type_ptr.hpp"

// Include our helper function for loading shaders
#include "shader.hpp"

// --- NEW INCLUDES FOR OUR GAME ---
#include <vector>
#include <map>
#include <cmath>
#include <random>       // For randomization
#include <algorithm>    // For std::shuffle
#include <chrono>       // For seeding the random engine

// --- NEW STRUCT FOR OUR SHAPES ---
struct Shape {
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    int indexCount;
};

// --- NEW GLOBAL VARIABLES FOR OUR GAME ---
GLFWwindow* window;
const int width = 1024, height = 1024;
float scaleX = 1.5f, scaleY = 0.5f, scaleZ = 0;

// --- Puzzle Shapes ---
Shape shapeSquare;
Shape shapeTriangle;
Shape shapeCircle;
Shape shapeHexagon;
Shape shapeRectangle; // A generic rectangle
Shape shapeLine;      // A generic line for knight limbs

// --- Knight Shapes ---
Shape shapeKnightTorso;
Shape shapeKnightHead;
Shape shapeKnightArm;
Shape shapeKnightLeg;
Shape shapeKnightCape;

// --- Scenery ---
Shape shapeMountain;

// --- GAME STATE ---
enum GameState {
    STATE_INTRO_READY, // "Ready?"
    STATE_INTRO_3,
    STATE_INTRO_2,
    STATE_INTRO_1,
    STATE_PROMPT,      // Show the sequence
    STATE_PUZZLE,      // Player solves
    STATE_WIN,         // Player submitted correctly
    STATE_LOSE,       // Player submitted incorrectly
    STATE_FINAL        // soby1
};
GameState currentState = STATE_INTRO_READY;
double stateStartTime = 0.0; // A timer for our states


//soby1
double pKeyDownStart = -1.0;
bool pWasDown = false;
float indicatorSquareHalfSize = 0.12f;   // tweak this to make the target bigger or smaller

bool wizardIsDying = false;
float wizardFallRotation = 0.0f;   // degrees

float barSpeed = 0.8f;    // adjust to whatever you used
int barDirection = 1;     // +1 = moving right, -1 = moving left




glm::vec3 knightBasePos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 wizardBasePos = glm::vec3(1.6f, 0.0f, 0.0f); // initial off-screen
bool finalBarActive = false;
float finalBarStartX = -1.2f;
float finalBarX = finalBarStartX;
float finalBarSpeed = 0.9f;     // units/sec (tweak)
float finalBarHalfWidth = 0.06f; // half width for overlap check
float indicatorX = 0.0f;        // center of screen
float indicatorHalfWidth = 0.03f;
bool prevSpaceDown = false;
bool finalResultShown = false;
std::string finalResultText;
double finalResultStartTime = 0.0;

// --- PUZZLE LOGIC ---
// This vector will be shuffled
std::vector<int> correctSequence = { 0, 1, 2, 3 };
int playerContainers[4] = { -1, -1, -1, -1 };
// Random number generator
std::mt19937 randomGenerator;

// --- Bounding box for click detection ---
struct Rect {
    float x, y, w, h;
};
// --- MOVED BUTTON UP ---
Rect submitButtonRect = { 0.6f, -0.8f, 0.3f, 0.1f }; // x, y, width, height for submit button

// --- PUZZLE POSITIONS ---
glm::vec3 containerPositions[4] = {
    glm::vec3(-0.45f, 0.3f, 0.0f),
    glm::vec3(-0.15f, 0.3f, 0.0f),
    glm::vec3(0.15f, 0.3f, 0.0f),
    glm::vec3(0.45f, 0.3f, 0.0f)
};

glm::vec3 puzzleShapePositions[4] = {
    glm::vec3(-0.45f, -0.6f, 0.0f), // Moved shapes up slightly
    glm::vec3(-0.15f, -0.6f, 0.0f),
    glm::vec3(0.15f, -0.6f, 0.0f),
    glm::vec3(0.45f, -0.6f, 0.0f)
};


// --- NEW HELPER FUNCTIONS FOR CREATING SHAPES ---
Shape createShape(GLfloat vertices[], int vertSize, GLuint indices[], int indSize) {
    Shape newShape;
    newShape.indexCount = indSize / sizeof(GLuint);

    glGenVertexArrays(1, &newShape.vao);
    glGenBuffers(1, &newShape.vbo);
    glGenBuffers(1, &newShape.ibo);

    glBindVertexArray(newShape.vao);

    glBindBuffer(GL_ARRAY_BUFFER, newShape.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertSize, vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newShape.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indSize, indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return newShape;
}

void createAllShapes() {
    // --- Generic Square/Rectangle ---
    GLfloat rectVertices[] = {
        0.5f,  0.5f, 0.0f,  // top right
        0.5f, -0.5f, 0.0f,  // bottom right
       -0.5f, -0.5f, 0.0f,  // bottom left
       -0.5f,  0.5f, 0.0f   // top left 
    };
    GLuint rectIndices[] = { 0, 3, 1, 1, 3, 2 };
    shapeRectangle = createShape(rectVertices, sizeof(rectVertices), rectIndices, sizeof(rectIndices));
    // Use the same shape for the puzzle square, just scaled
    shapeSquare = shapeRectangle;

    // --- Generic Triangle ---
    GLfloat triangleVertices[] = {
         0.0f,  0.5f, 0.0f, // top
        -0.5f, -0.5f, 0.0f, // bottom left
         0.5f, -0.5f, 0.0f  // bottom right
    };
    GLuint triangleIndices[] = { 0, 1, 2 };
    shapeTriangle = createShape(triangleVertices, sizeof(triangleVertices), triangleIndices, sizeof(triangleIndices));

    // --- Circle ---
    const int numSegments = 36;
    std::vector<GLfloat> circleVertices;
    std::vector<GLuint> circleIndices;
    circleVertices.push_back(0.0f); circleVertices.push_back(0.0f); circleVertices.push_back(0.0f); // Center
    for (int i = 0; i <= numSegments; ++i) {
        float angle = 2.0f * 3.14159f * float(i) / float(numSegments);
        circleVertices.push_back(cos(angle) * 0.5f); // x
        circleVertices.push_back(sin(angle) * 0.5f); // y
        circleVertices.push_back(0.0f);              // z
    }
    for (int i = 1; i <= numSegments; ++i) {
        circleIndices.push_back(0); circleIndices.push_back(i); circleIndices.push_back(i + 1);
    }
    shapeCircle = createShape(circleVertices.data(), (GLint)(circleVertices.size() * sizeof(GLfloat)), circleIndices.data(), (GLint)(circleIndices.size() * sizeof(GLuint)));

    // --- Hexagon ---
    std::vector<GLfloat> hexVertices;
    std::vector<GLuint> hexIndices;
    hexVertices.push_back(0.0f); hexVertices.push_back(0.0f); hexVertices.push_back(0.0f); // Center
    for (int i = 0; i < 6; ++i) {
        float angle = 2.0f * 3.14159f / 6.0f * (float(i) + 0.5f);
        hexVertices.push_back(cos(angle) * 0.5f); hexVertices.push_back(sin(angle) * 0.5f); hexVertices.push_back(0.0f);
    }
    hexIndices = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6, 0, 6, 1 };
    shapeHexagon = createShape(hexVertices.data(), (GLint)(hexVertices.size() * sizeof(GLfloat)), hexIndices.data(), (GLint)(hexIndices.size() * sizeof(GLuint)));

    // --- Knight Shapes ---
    shapeKnightTorso = shapeRectangle; // Re-use
    shapeKnightHead = shapeCircle;     // Re-use
    shapeKnightCape = shapeTriangle;   // Re-use
    shapeKnightArm = shapeRectangle;
    shapeKnightLeg = shapeRectangle;

    // --- Scenery ---
    shapeMountain = shapeTriangle; // Re-use
}

// --- NEW DRAWING FUNCTION ---
void drawShape(Shape& shape, GLuint programID, glm::vec3 position, float rotation, glm::vec3 scale, glm::vec4 color) {
    glUseProgram(programID);

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::translate(trans, position);
    trans = glm::rotate(trans, glm::radians(rotation), glm::vec3(0.0, 0.0, 1.0));
    trans = glm::scale(trans, scale);

    glUniformMatrix4fv(glGetUniformLocation(programID, "transform"), 1, GL_FALSE, glm::value_ptr(trans));

    // --- THIS IS THE FIX ---
    // Removed the extra 'GL_FALSE' argument from this function call
    glUniform4fv(glGetUniformLocation(programID, "color"), 1, glm::value_ptr(color));

    glBindVertexArray(shape.vao);
    glDrawElements(GL_TRIANGLES, shape.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

//soby1
// --- NEW --- Helper function to draw our knight at a base position
void drawKnight(GLuint programID, float armRotation, float headTilt, const glm::vec3& basePos) {
    // Cape (behind everything)
    drawShape(shapeKnightCape, programID, basePos + glm::vec3(0.0f, -0.3f, 0.0f), 0.0f, glm::vec3(0.3f, 0.5f, 1.0f), glm::vec4(0.5f, 0.1f, 0.1f, 1.0f));
    // Torso
    drawShape(shapeKnightTorso, programID, basePos + glm::vec3(0.0f, -0.2f, 0.0f), 0.0f, glm::vec3(0.2f, 0.4f, 1.0f), glm::vec4(0.3f, 0.3f, 0.4f, 1.0f));
    // Head
    drawShape(shapeKnightHead, programID, basePos + glm::vec3(0.0f, 0.1f, 0.0f), headTilt, glm::vec3(0.14f, 0.14f, 1.0f), glm::vec4(0.8f, 0.7f, 0.6f, 1.0f));
    // Arms (thin rectangles)
    drawShape(shapeKnightArm, programID, basePos + glm::vec3(-0.12f, -0.1f, 0.0f), armRotation, glm::vec3(0.04f, 0.3f, 1.0f), glm::vec4(0.2f, 0.2f, 0.3f, 1.0f));
    drawShape(shapeKnightArm, programID, basePos + glm::vec3(0.12f, -0.1f, 0.0f), -armRotation, glm::vec3(0.04f, 0.3f, 1.0f), glm::vec4(0.2f, 0.2f, 0.3f, 1.0f));
    // Legs
    drawShape(shapeKnightLeg, programID, basePos + glm::vec3(-0.05f, -0.5f, 0.0f), 0.0f, glm::vec3(0.06f, 0.2f, 1.0f), glm::vec4(0.2f, 0.2f, 0.3f, 1.0f));
    drawShape(shapeKnightLeg, programID, basePos + glm::vec3(0.05f, -0.5f, 0.0f), 0.0f, glm::vec3(0.06f, 0.2f, 1.0f), glm::vec4(0.2f, 0.2f, 0.3f, 1.0f));
}

//soby1
// Replace your old drawWizard with this version.
// Note: this uses your existing drawShape(...) which expects (shape, programID, position, rotation, scale, color).
// We build a parent matrix, then compute a full transform per part and set it via glUniformMatrix4fv directly
// (so we duplicate a little of drawShape's work but keep it simple and correct).

void drawWizard(GLuint programID,
    float armRotation,
    float headTilt,
    const glm::vec3& basePos,
    float fallRotDeg)   // NEW parameter: whole-wizard rotation in degrees
{
    // Precompute parent transform: translate to basePos, then rotate by fallRotDeg around Z.
    glm::mat4 parent = glm::mat4(1.0f);
    parent = glm::translate(parent, basePos);
    parent = glm::rotate(parent, glm::radians(fallRotDeg), glm::vec3(0.0f, 0.0f, 1.0f));

    // Colors (same as before)
    glm::vec4 robeColor = glm::vec4(0.45f, 0.0f, 0.75f, 1.0f);
    glm::vec4 trimColor = glm::vec4(0.30f, 0.0f, 0.55f, 1.0f);
    glm::vec4 headColor = glm::vec4(0.95f, 0.85f, 0.75f, 1.0f);

    // Cache uniform locations once (a little faster)
    GLint uTransform = glGetUniformLocation(programID, "transform");
    GLint uColor = glGetUniformLocation(programID, "color");
    glUseProgram(programID);

    // Helper lambda: draw a local part under the parent matrix.
    auto drawLocal = [&](Shape& shape, glm::vec3 localPos, float localRotDeg, glm::vec3 localScale, glm::vec4 color)
        {
            glm::mat4 t = parent;
            t = glm::translate(t, localPos);
            t = glm::rotate(t, glm::radians(localRotDeg), glm::vec3(0.0f, 0.0f, 1.0f));
            t = glm::scale(t, localScale);

            glUniformMatrix4fv(uTransform, 1, GL_FALSE, glm::value_ptr(t));
            glUniform4fv(uColor, 1, glm::value_ptr(color));
            glBindVertexArray(shape.vao);
            glDrawElements(GL_TRIANGLES, shape.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        };

    // NOW draw parts using local coordinates relative to basePos:
    // Body (local origin at wizard feet)
    drawLocal(shapeRectangle, glm::vec3(0.0f, -0.12f, 0.0f), 0.0f, glm::vec3(0.22f, 0.42f, 1.0f), robeColor);

    // Head
    drawLocal(shapeRectangle, glm::vec3(0.0f, 0.18f, 0.0f), headTilt, glm::vec3(0.15f, 0.15f, 1.0f), headColor);

    // Arms (use same X offsets but local now)
    drawLocal(shapeRectangle, glm::vec3(0.15f, -0.02f, 0.0f), armRotation, glm::vec3(0.10f, 0.30f, 1.0f), robeColor);
    drawLocal(shapeRectangle, glm::vec3(-0.15f, -0.02f, 0.0f), -armRotation, glm::vec3(0.10f, 0.30f, 1.0f), robeColor);

    // Feet
    drawLocal(shapeRectangle, glm::vec3(0.08f, -0.43f, 0.0f), 0.0f, glm::vec3(0.07f, 0.22f, 1.0f), trimColor);
    drawLocal(shapeRectangle, glm::vec3(-0.08f, -0.43f, 0.0f), 0.0f, glm::vec3(0.07f, 0.22f, 1.0f), trimColor);

    // Hat brim and tall blocky top (local positions)
    drawLocal(shapeRectangle, glm::vec3(0.0f, 0.28f, 0.0f), 0.0f, glm::vec3(0.28f, 0.06f, 1.0f), trimColor);
    drawLocal(shapeRectangle, glm::vec3(0.0f, 0.42f, 0.0f), 0.0f, glm::vec3(0.20f, 0.25f, 1.0f), robeColor);

    // leave GL program bound state as-is
}




// --- NEW --- Helper function to draw the background
void drawBackground(GLuint programID) {
    // Sky
    glClearColor(0.3f, 0.4f, 0.6f, 1.0f); // Blue sky
    // Mountains
    drawShape(shapeMountain, programID, glm::vec3(-0.8f, -0.8f, 0.0f), 0.0f, glm::vec3(1.0f, 0.5f, 1.0f), glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
    drawShape(shapeMountain, programID, glm::vec3(0.2f, -0.8f, 0.0f), 0.0f, glm::vec3(1.5f, 0.8f, 1.0f), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
    drawShape(shapeMountain, programID, glm::vec3(1.0f, -0.8f, 0.0f), 0.0f, glm::vec3(1.0f, 0.4f, 1.0f), glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
}


// --- CALLBACK FUNCTIONS ---

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    // std::cout << "The mouse cursor is: " << xpos << " " << ypos << std::endl;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && currentState == STATE_PUZZLE)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        float ndc_x = (float)(xpos / width) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (float)(ypos / height) * 2.0f; // Y is inverted

        // --- Check if we clicked a shape at the BOTTOM ---
        for (int i = 0; i < 4; i++) {
            glm::vec3 pos = puzzleShapePositions[i];
            Rect shapeRect = { pos.x - 0.08f, pos.y - 0.08f, 0.16f, 0.16f };
            if (ndc_x >= shapeRect.x && ndc_x <= shapeRect.x + shapeRect.w &&
                ndc_y >= shapeRect.y && ndc_y <= shapeRect.y + shapeRect.h)
            {
                int clickedShapeID = i;
                bool alreadyPlaced = false;
                for (int j = 0; j < 4; j++) {
                    if (playerContainers[j] == clickedShapeID) alreadyPlaced = true;
                }
                if (!alreadyPlaced) {
                    for (int j = 0; j < 4; j++) {
                        if (playerContainers[j] == -1) {
                            playerContainers[j] = clickedShapeID;
                            break;
                        }
                    }
                }
                return;
            }
        }

        // --- Check if we clicked a shape in a CONTAINER ---
        for (int i = 0; i < 4; i++) {
            glm::vec3 pos = containerPositions[i];
            Rect containerRect = { pos.x - 0.08f, pos.y - 0.08f, 0.16f, 0.16f };
            if (ndc_x >= containerRect.x && ndc_x <= containerRect.x + containerRect.w &&
                ndc_y >= containerRect.y && ndc_y <= containerRect.y + containerRect.h)
            {
                if (playerContainers[i] != -1) {
                    playerContainers[i] = -1;
                }
                return;
            }
        }

        // --- Check if we clicked the SUBMIT BUTTON ---
        Rect btn = submitButtonRect;
        // Convert from (center, w, h) to (x1, y1, x2, y2)
        btn.x = submitButtonRect.x - submitButtonRect.w / 2.0f;
        btn.y = submitButtonRect.y - submitButtonRect.h / 2.0f;
        if (ndc_x >= btn.x && ndc_x <= btn.x + btn.w &&
            ndc_y >= btn.y && ndc_y <= btn.y + btn.h)
        {
            // Check the answer!
            bool isCorrect = true;
            for (int i = 0; i < 4; i++) {
                if (playerContainers[i] != correctSequence[i]) {
                    isCorrect = false;
                    break;
                }
            }

            if (isCorrect) {
                currentState = STATE_WIN;
            }
            else {
                currentState = STATE_LOSE;
            }
            stateStartTime = glfwGetTime();
            return;
        }
    }
}


void window_callback(GLFWwindow* window, int new_width, int new_height)
{
    glViewport(0, 0, new_width, new_height);
}

int main(void)
{
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        (void)getchar();
        return -1;
    }

    window = glfwCreateWindow(width, height, "Eloria - The Memory Seal", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to open GLFW window.");
        (void)getchar();
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        (void)getchar();
        glfwTerminate();
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, width, height);

    GLuint programID = LoadShaders("SimpleVertexShader.vertexshader", "SimpleFragmentShader.fragmentshader");

    createAllShapes();

    // --- Store our shape objects in a map for easy access by ID
    std::map<int, Shape> shapeMap;
    shapeMap[0] = shapeSquare;
    shapeMap[1] = shapeTriangle;
    shapeMap[2] = shapeCircle;
    shapeMap[3] = shapeHexagon;

    // --- Define the colors for our shapes
    std::map<int, glm::vec4> shapeColors;
    shapeColors[0] = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f); // Square = Red
    shapeColors[1] = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f); // Triangle = Green
    shapeColors[2] = glm::vec4(0.2f, 0.2f, 0.8f, 1.0f); // Circle = Blue
    shapeColors[3] = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f); // Hexagon = Yellow

    // --- NEW --- Setup random generator and shuffle sequence for the first time
    unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    randomGenerator.seed(seed);
    std::shuffle(correctSequence.begin(), correctSequence.end(), randomGenerator);


    stateStartTime = glfwGetTime();

    double lastFrameTime = glfwGetTime();//s1

    // glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, window_callback);

    // --- Knight animation variables
    float knightArmRotation = 0.0f;
    float knightHeadTilt = 0.0f;

    // --- Main Loop ---
    while (!glfwWindowShouldClose(window))
    {
        double currentTime = glfwGetTime();
        double timeInState = currentTime - stateStartTime;

        /*double lastFrameTime = 0.0;
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;*/
        //double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        if (wizardIsDying) {
            wizardFallRotation += deltaTime * 6000.0;
            if (wizardFallRotation < 90.0)
                wizardFallRotation = 90.0;

        }

        // --- Logic and Drawing ---

        drawBackground(programID);
        glClear(GL_COLOR_BUFFER_BIT); // Clear buffer *after* setting color

        // --- Draw Background Scenery ---
        drawBackground(programID);

        // ---------------- DEBUG: Hold P for 2 seconds to skip to FINAL ----------------
        bool pDown = (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS);

        if (pDown && !pWasDown) {
            // just pressed
            pKeyDownStart = currentTime;
        }

        if (pDown && pKeyDownStart > 0.0) {
            // held long enough
            if (currentTime - pKeyDownStart >= 1.0) {
                // jump directly to final stage
                currentState = STATE_FINAL;
                stateStartTime = currentTime;

                // reset bar
                finalBarActive = true;
                finalBarX = finalBarStartX;
                finalResultShown = false;
                prevSpaceDown = false;

                // put knight + wizard in their Stage 2 positions
                knightBasePos.x = -0.6f;
                wizardBasePos.x = 0.9f;

                std::cout << "[DEBUG] Skipped to Stage 2" << std::endl;
            }
        }

        if (!pDown) {
            pKeyDownStart = -1.0; // reset
        }

        pWasDown = pDown;
        // -------------------------------------------------------------------------------


        // --- Game State Logic ---
        switch (currentState)
        {
        case STATE_INTRO_READY:
        {
            // This is where "Memory Test. Are you ready?" text would go.

            // Visual cue: Knight stands ready
            knightArmRotation = 0.0f;
            knightHeadTilt = 0.0f;

            if (timeInState >= 2.0) {
                currentState = STATE_INTRO_3;
                stateStartTime = currentTime;
            }
            break;
        }
        case STATE_INTRO_3:
        {
            // This is where "3" text would go.

            // Visual cue: Flash background
            glClearColor(0.8f, 0.2f, 0.2f, 1.0f); // Red

            if (timeInState >= 0.5) { // Shorter time for countdown
                currentState = STATE_INTRO_2;
                stateStartTime = currentTime;
            }
            break;
        }
        case STATE_INTRO_2:
        {
            // This is where "2" text would go.

            // Visual cue: Flash background
            glClearColor(0.8f, 0.8f, 0.2f, 1.0f); // Yellow

            if (timeInState >= 0.5) {
                currentState = STATE_INTRO_1;
                stateStartTime = currentTime;
            }
            break;
        }
        case STATE_INTRO_1:
        {
            // This is where "1" text would go.

            // Visual cue: Flash background
            glClearColor(0.2f, 0.8f, 0.2f, 1.0f); // Green

            if (timeInState >= 0.5) {
                currentState = STATE_PROMPT;
                stateStartTime = currentTime;
            }
            break;
        }
        case STATE_PROMPT:
        {
            // "Show the sequence" phase
            float alpha = 1.0f;
            if (timeInState > 2.5f) { // Start fading
                alpha = 1.0f - (float)(timeInState - 2.5f) * 2.0f;
                alpha = glm::clamp(alpha, 0.0f, 1.0f);
            }

            // Draw containers
            drawShape(shapeRectangle, programID, glm::vec3(0.0f, 0.3f, 0.0f), 0.0f, glm::vec3(1.4f, 0.25f, 1.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
            for (int i = 0; i < 4; i++) {
                drawShape(shapeSquare, programID, containerPositions[i], 0.0f, glm::vec3(0.2f, 0.2f, 1.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
            }

            // Draw the 4 correct shapes inside the containers
            for (int i = 0; i < 4; i++) {
                int shapeID = correctSequence[i];
                Shape& s = shapeMap[shapeID];
                glm::vec4 color = shapeColors[shapeID];
                color.a = alpha; // Apply the fade (Animation 1)

                drawShape(s, programID, containerPositions[i], 0.0f, glm::vec3(0.15f, 0.15f, 1.0f), color);
            }

            if (timeInState >= 3.0f) {
                currentState = STATE_PUZZLE;
                stateStartTime = currentTime;
            }
            break;
        }

        case STATE_PUZZLE:
        case STATE_LOSE: // Also draw the puzzle in the "LOSE" state
        {
            // "Player Input" phase
            // Draw containers
            drawShape(shapeRectangle, programID, glm::vec3(0.0f, 0.3f, 0.0f), 0.0f, glm::vec3(1.4f, 0.25f, 1.0f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
            for (int i = 0; i < 4; i++) {
                drawShape(shapeSquare, programID, containerPositions[i], 0.0f, glm::vec3(0.2f, 0.2f, 1.0f), glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
            }

            // Draw the 4 shapes at the bottom for the player to click
            for (int i = 0; i < 4; i++) {
                bool isPlaced = false;
                for (int j = 0; j < 4; j++) if (playerContainers[j] == i) isPlaced = true;

                // (Animation 2: Disappear/Reappear)
                if (!isPlaced) {
                    Shape& s = shapeMap[i];
                    glm::vec4 color = shapeColors[i];
                    drawShape(s, programID, puzzleShapePositions[i], 0.0f, glm::vec3(0.15f, 0.15f, 1.0f), color);
                }
            }

            // Draw the shapes the player has placed IN the containers
            for (int i = 0; i < 4; i++) {
                int shapeID = playerContainers[i];
                if (shapeID != -1) {
                    Shape& s = shapeMap[shapeID];
                    glm::vec4 color = shapeColors[shapeID];
                    drawShape(s, programID, containerPositions[i], 0.0f, glm::vec3(0.15f, 0.15f, 1.0f), color);
                }
            }

            // Draw Submit Button
            glm::vec4 btnColor = (currentState == STATE_LOSE) ? glm::vec4(0.8f, 0.2f, 0.2f, 1.0f) : glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);
            drawShape(shapeRectangle, programID, glm::vec3(submitButtonRect.x, submitButtonRect.y, 0.0f), 0.0f, glm::vec3(submitButtonRect.w, submitButtonRect.h, 1.0f), btnColor);
            // --- We would draw "SUBMIT" text on the button here ---

            if (currentState == STATE_LOSE) {
                // "Try again..."
                knightHeadTilt = -20.0f; // Knight slumps (Animation 2)
                knightArmRotation = 0.0f;
                // --- We would draw "Try Again!" text here ---
                if (timeInState >= 2.0f) {
                    // Reset for another try
                    currentState = STATE_PUZZLE;
                    stateStartTime = currentTime;
                    // Clear the containers
                    for (int i = 0; i < 4; i++) playerContainers[i] = -1;
                }
            }
            else {
                knightHeadTilt = 0.0f;
            }
            break;
        }

        //case STATE_WIN:
        //{
        //    // "You win!"
        //    // --- We would draw "Success! Task 2 Unlocked." text here ---

        //    // Visual cue: Knight cheers! (Animation 2)
        //    knightArmRotation = -140.0f;
        //    knightHeadTilt = 0.0f;

        //    // After 3 seconds, we could go to the next task.
        //    // For now, we'll just restart the intro.
        //    if (timeInState >= 3.0) {
        //        currentState = STATE_INTRO_READY;
        //        stateStartTime = currentTime;
        //        // Clear containers for next round
        //        for (int i = 0; i < 4; i++) playerContainers[i] = -1;

        //        // --- NEW: RE-SHUFFLE FOR NEXT ROUND ---
        //        std::shuffle(correctSequence.begin(), correctSequence.end(), randomGenerator);
        //    }
        //    break;
        //}
        case STATE_WIN:
        {
            float animDuration = 1.0f;
            float t = (float)glm::clamp((float)timeInState / animDuration, 0.0f, 1.0f);

            // animate knight and wizard
            float knightStartX = 0.0f;
            float knightTargetX = -0.6f;
            knightBasePos.x = glm::mix(knightStartX, knightTargetX, t);

            float wizardStartX = 1.6f;
            float wizardTargetX = 0.9f;
            wizardBasePos.x = glm::mix(wizardStartX, wizardTargetX, t);

            // draw characters
            drawKnight(programID, knightArmRotation, knightHeadTilt, knightBasePos);
            //drawKnight(programID, knightArmRotation, knightHeadTilt, wizardBasePos);
            drawWizard(programID, knightArmRotation, knightHeadTilt, wizardBasePos, wizardFallRotation);



            // transition to timing minigame
            if (timeInState >= animDuration) {
                currentState = STATE_FINAL;
                stateStartTime = currentTime;
                finalBarActive = true;
                finalBarX = finalBarStartX;
                finalResultShown = false;
                prevSpaceDown = false;
            }
            break;
        }

        //soby1
        case STATE_FINAL:
        {
            // update bar position
            if (finalBarActive) {
                finalBarX += finalBarSpeed * barDirection * (float)deltaTime;
                if (finalBarX + 0.04f >= 1.0f) barDirection = -1; // hit right edge
                if (finalBarX - 0.04f <= -1.0f) barDirection = 1; // hit left edge
            }



            // draw knight & wizard behind UI
            drawKnight(programID, knightArmRotation, knightHeadTilt, knightBasePos);
            drawWizard(programID, knightArmRotation, knightHeadTilt, wizardBasePos, wizardFallRotation);



            //
            // ----- STATIC HIT ZONE -----
            //
            glm::vec4 squareColor = glm::vec4(0.1f, 0.8f, 0.1f, 0.4f); // translucent green
            drawShape(
                shapeRectangle,
                programID,
                glm::vec3(indicatorX, 0.0f, 0.0f),
                0.0f,
                glm::vec3(indicatorSquareHalfSize, indicatorSquareHalfSize, 1.0f),
                squareColor
            );

            // center line (optional, just visual)
            drawShape(
                shapeRectangle,
                programID,
                glm::vec3(indicatorX, 0.0f, 0.0f),
                0.0f,
                glm::vec3(0.01f, indicatorSquareHalfSize * 2.0f, 1.0f),
                glm::vec4(1.0f)
            );

            //
            // ----- MOVING BAR -----
            //
            drawShape(
                shapeRectangle,
                programID,
                glm::vec3(finalBarX, 0.0f, 0.0f),
                0.0f,
                glm::vec3(0.04f, 0.9f, 1.0f),
                glm::vec4(0.9f, 0.2f, 0.2f, 1.0f)
            );

            //
            // ----- SPACE PRESS → CHECK SUCCESS -----
            //
            bool spaceDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
            bool pressed = (spaceDown && !prevSpaceDown);  // edge detection only!

            if (pressed && finalBarActive)
            {
                float dist = fabs(finalBarX - indicatorX);

                if (dist <= indicatorSquareHalfSize) {
                    finalResultText = "YOU WON";
                    wizardIsDying = true;            // <-- THIS LINE
                    wizardFallRotation = 0.0f;       // <-- reset
                    finalResultShown = true;
                    finalResultStartTime = currentTime;
                    glfwSetWindowTitle(window, "You won!");
                }
                else
                {
                    finalResultText = "YOU LOST";
                }

                finalResultShown = true;
                finalResultStartTime = currentTime;
                finalBarActive = false;

                glfwSetWindowTitle(window, finalResultText.c_str());
            }

            prevSpaceDown = spaceDown;

            //
            // ----- BAR MISSED COMPLETELY → AUTO LOSE -----
            //
            if (!finalResultShown && finalBarX > 1.2f)
            {
                finalResultText = "YOU LOST";
                finalResultShown = true;
                finalResultStartTime = currentTime;
                finalBarActive = false;
                glfwSetWindowTitle(window, "You lost");
            }

            //
            // ----- RESULT OVERLAY -----
            //
            if (finalResultShown)
            {
                glm::vec4 overlayColor =
                    (finalResultText == "YOU WON")
                    ? glm::vec4(0.1f, 0.8f, 0.2f, 0.85f)
                    : glm::vec4(0.8f, 0.1f, 0.1f, 0.85f);

                drawShape(
                    shapeRectangle,
                    programID,
                    glm::vec3(0.0f, 0.0f, 0.0f),
                    0.0f,
                    glm::vec3(1.0f, 0.4f, 1.0f),
                    overlayColor
                );

                std::cout << finalResultText << std::endl;

                if ((currentTime - finalResultStartTime) >= 2.0)
                {
                    currentState = STATE_INTRO_READY;
                    stateStartTime = currentTime;
                    for (int i = 0; i < 4; i++) playerContainers[i] = -1;
                    std::shuffle(correctSequence.begin(), correctSequence.end(), randomGenerator);
                    glfwSetWindowTitle(window, "Eloria - The Memory Seal");
                }
            }

            break;
        }




        } // End switch


        // --- Draw the Knight on top of everything ---
        //drawKnight(programID, knightArmRotation, knightHeadTilt);
        // new (normal gameplay keeps knight centered)
        drawKnight(programID, knightArmRotation, knightHeadTilt, knightBasePos);


        // --- Finalize Frame ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    // (We should add cleanup for all shapes here)
    glDeleteProgram(programID);
    glfwTerminate();
    return 0;
}