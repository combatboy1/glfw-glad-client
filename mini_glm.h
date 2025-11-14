#ifndef MINI_GLM_H
#define MINI_GLM_H

// mini_glm.h - minimal, dependency-free subset of glm used by the project.
// Column-major 4x4 matrices, vec2/vec3, perspective, lookAt, value_ptr.

#include <cmath>
#include <array>

namespace glm {

    struct vec2 {
        float x, y;
        vec2(float X = 0.0f, float Y = 0.0f) : x(X), y(Y) {}
    };

    struct vec3 {
        float x, y, z;
        vec3(float X = 0.0f, float Y = 0.0f, float Z = 0.0f) : x(X), y(Y), z(Z) {}
        vec3& operator+=(const vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
        vec3 operator+(const vec3& o) const { return vec3(x + o.x, y + o.y, z + o.z); }
        vec3 operator-(const vec3& o) const { return vec3(x - o.x, y - o.y, z - o.z); }
        vec3 operator*(float s) const { return vec3(x * s, y * s, z * s); }
    };

    inline float dot(const vec3& a, const vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline vec3 cross(const vec3& a, const vec3& b) {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    inline vec3 normalize(const vec3& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len <= 1e-9f) return vec3(0.0f, 0.0f, 0.0f);
        return vec3(v.x / len, v.y / len, v.z / len);
    }

    struct mat4 {
        // Column-major: data[col*4 + row]
        std::array<float, 16> data;
        mat4() {
            data = { 1,0,0,0,
                    0,1,0,0,
                    0,0,1,0,
                    0,0,0,1 };
        }
        static mat4 identity() { return mat4(); }
        float* ptr() { return data.data(); }
        const float* ptr() const { return data.data(); }

        mat4 operator*(const mat4& o) const {
            mat4 r;
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k) {
                        float a = data[k * 4 + row];
                        float b = o.data[col * 4 + k];
                        sum += a * b;
                    }
                    r.data[col * 4 + row] = sum;
                }
            }
            return r;
        }
    };

    inline float radians(float deg) {
        return deg * (3.14159265358979323846f / 180.0f);
    }

    inline const float* value_ptr(const mat4& m) {
        return m.ptr();
    }

    inline mat4 perspective(float fovY_degrees, float aspect, float zNear, float zFar) {
        float f = 1.0f / std::tan(radians(fovY_degrees) * 0.5f);
        mat4 m;
        for (int i = 0; i < 16; ++i) m.data[i] = 0.0f;
        m.data[0] = f / aspect;
        m.data[5] = f;
        m.data[10] = (zFar + zNear) / (zNear - zFar);
        m.data[11] = -1.0f;
        m.data[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        return m;
    }

    inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
        vec3 f = normalize(vec3(center.x - eye.x, center.y - eye.y, center.z - eye.z));
        vec3 s = normalize(cross(f, up));
        vec3 u = cross(s, f);

        mat4 m;
        m.data[0] = s.x; m.data[1] = u.x; m.data[2] = -f.x; m.data[3] = 0.0f;
        m.data[4] = s.y; m.data[5] = u.y; m.data[6] = -f.y; m.data[7] = 0.0f;
        m.data[8] = s.z; m.data[9] = u.z; m.data[10] = -f.z; m.data[11] = 0.0f;
        m.data[12] = -dot(s, eye);
        m.data[13] = -dot(u, eye);
        m.data[14] = dot(f, eye);
        m.data[15] = 1.0f;
        return m;
    }

} // namespace glm

#endif // MINI_GLM_H