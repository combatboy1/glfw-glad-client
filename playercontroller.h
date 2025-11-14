#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include "gl_includes.h"
#include <cmath>
#include <iostream>

// Simple Vec3 used by PlayerController (keeps this internal to avoid external dependency)
struct Vec3 {
    float x, y, z;
    Vec3(float X = 0, float Y = 0, float Z = 0) :x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
};

class PlayerController {
public:
    explicit PlayerController(GLFWwindow* window);

    // Update the controller (movement + mouse rotation)
    void update(float dt);

    Vec3 getPosition() const { return position; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }

    void setPosition(const Vec3& pos) { position = pos; }
    void setOrientation(float newYaw, float newPitch) { yaw = newYaw; pitch = newPitch; }

private:
    GLFWwindow* window;
    Vec3 position;
    float yaw;   // degrees
    float pitch; // degrees

    float walkSpeed;
    float mouseSensitivity;

    bool firstMouse;
    double lastX, lastY;

    static float deg2rad(float d) { return d * (3.14159265358979323846f / 180.0f); }
    static Vec3 normalize(const Vec3& v);
    static Vec3 cross(const Vec3& a, const Vec3& b);
};

#endif // PLAYERCONTROLLER_H