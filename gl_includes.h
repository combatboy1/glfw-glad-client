#ifndef GL_INCLUDES_H
#define GL_INCLUDES_H

// Cross-platform GL loader + windowing include.
// GLAD must be generated for OpenGL 3.3 core (or compatible) and glad.c must be compiled into the project.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// GLAD loader (must be included BEFORE GLFW)
#include <glad/glad.h>

// Tell GLFW not to include any GL headers itself
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#endif // GL_INCLUDES_H