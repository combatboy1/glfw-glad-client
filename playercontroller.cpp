#include "playercontroller.h"

// Utility implementations
Vec3 PlayerController::normalize(const Vec3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 1e-6f) return Vec3(0.0f, 0.0f, 0.0f);
    return Vec3(v.x / len, v.y / len, v.z / len);
}

Vec3 PlayerController::cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

PlayerController::PlayerController(GLFWwindow* window_)
    : window(window_), position(0.0f, 2.0f, 5.0f), yaw(0.0f), pitch(0.0f),
    walkSpeed(5.0f), mouseSensitivity(0.12f), firstMouse(true), lastX(0.0), lastY(0.0)
{
    if (!window) return;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    glfwSetWindowUserPointer(window, this);
    int w = 800, h = 600;
    glfwGetWindowSize(window, &w, &h);
    lastX = w / 2.0;
    lastY = h / 2.0;
    glfwSetCursorPos(window, lastX, lastY);
}

void PlayerController::update(float dt) {
    if (!window) return;

    double xpos = 0.0, ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);

    int w = 800, h = 600;
    glfwGetWindowSize(window, &w, &h);
    double centerX = w / 2.0;
    double centerY = h / 2.0;

    bool cursorDisabled = (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);

    double xoffset = 0.0, yoffset = 0.0;
    if (cursorDisabled) {
        xoffset = xpos - centerX;
        yoffset = centerY - ypos;
        glfwSetCursorPos(window, centerX, centerY);
        lastX = centerX; lastY = centerY;
    }
    else {
        if (firstMouse) {
            lastX = xpos; lastY = ypos; firstMouse = false;
        }
        xoffset = xpos - lastX;
        yoffset = lastY - ypos;
        lastX = xpos; lastY = ypos;
    }

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw -= static_cast<float>(xoffset);
    pitch += static_cast<float>(yoffset);
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    float yawRad = deg2rad(yaw);
    // Movement uses yaw-only forward on XZ plane
    Vec3 forwardFlat(std::sin(yawRad), 0.0f, std::cos(yawRad));
    // normalize
    forwardFlat = normalize(forwardFlat);
    Vec3 right(std::cos(yawRad), 0.0f, -std::sin(yawRad));
    right = normalize(right);

    float velocity = walkSpeed * dt;
    Vec3 moveDir(0.0f, 0.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { moveDir.x -= forwardFlat.x; moveDir.z -= forwardFlat.z; }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { moveDir.x += forwardFlat.x; moveDir.z += forwardFlat.z; }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { moveDir.x += right.x; moveDir.z += right.z; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { moveDir.x -= right.x; moveDir.z -= right.z; }

    Vec3 horizMove = normalize(moveDir);
    position.x += horizMove.x * velocity;
    position.z += horizMove.z * velocity;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) position.y -= velocity;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwTerminate();
    }
}