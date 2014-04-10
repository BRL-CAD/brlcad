//========================================================================
// Cursor input bug test
// Copyright (c) Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
//
// This test came about as the result of bugs #1262764, #1726540 and
// #1726592, all reported by the user peterpp, hence the name
//
// The utility of this test outside of these bugs is uncertain
//
//========================================================================

#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>

static GLboolean reopen = GL_FALSE;
static double cursor_x;
static double cursor_y;

static void toggle_cursor(GLFWwindow* window)
{
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
    {
        printf("Released cursor\n");
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    else
    {
        printf("Captured cursor\n");
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void cursor_position_callback(GLFWwindow* window, double x, double y)
{
    printf("Cursor moved to: %f %f (%f %f)\n", x, y, x - cursor_x, y - cursor_y);
    cursor_x = x;
    cursor_y = y;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    switch (key)
    {
        case GLFW_KEY_SPACE:
        {
            if (action == GLFW_PRESS)
                toggle_cursor(window);

            break;
        }

        case GLFW_KEY_R:
        {
            if (action == GLFW_PRESS)
                reopen = GL_TRUE;

            break;
        }
    }
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

static GLFWwindow* open_window(void)
{
    GLFWwindow* window = glfwCreateWindow(640, 480, "Peter Detector", NULL, NULL);
    if (!window)
        return NULL;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwGetCursorPos(window, &cursor_x, &cursor_y);
    printf("Cursor position: %f %f\n", cursor_x, cursor_y);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetKeyCallback(window, key_callback);

    return window;
}

int main(void)
{
    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    window = open_window();
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glClearColor(0.f, 0.f, 0.f, 0.f);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwWaitEvents();

        if (reopen)
        {
            glfwDestroyWindow(window);
            window = open_window();
            if (!window)
            {
                glfwTerminate();
                exit(EXIT_FAILURE);
            }

            reopen = GL_FALSE;
        }
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

