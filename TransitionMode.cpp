//
// Created by Eric Fang on 10/1/18.
//

#include "TransitionMode.h"

#include "Load.hpp"
#include "compile_program.hpp"
#include "draw_text.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>

//---------- resources ------------

static GLint fade_program_color = -1;

Load<GLuint> transition_program(LoadTagInit, []()
{
    GLuint *ret = new GLuint(compile_program(
        "#version 330\n"
        "void main() {\n"
        "	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
        "}\n",
        "#version 330\n"
        "uniform vec4 color;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "	fragColor = color;\n"
        "}\n"
    ));

    fade_program_color = glGetUniformLocation(*ret, "color");

    return ret;
});

TransitionMode::TransitionMode(GLuint target_texture_id)
    : target_texture_id(target_texture_id)
{}

bool TransitionMode::handle_event(SDL_Event const &e, glm::uvec2 const &window_size)
{
    return false;
}

void TransitionMode::update(float elapsed)
{
    background_fade = std::max(0.0f, background_fade - fade_speed * elapsed);

    if (background_fade <= 0.0f) {
        Mode::set_current(background);
    }
}

void TransitionMode::draw(glm::uvec2 const &drawable_size)
{
    if (background) {
        background->draw(drawable_size);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(*transition_program);
        glUniform4fv(fade_program_color, 1, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, background_fade)));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glUseProgram(0);
        glDisable(GL_BLEND);
    }

    glEnable(GL_DEPTH_TEST);
}