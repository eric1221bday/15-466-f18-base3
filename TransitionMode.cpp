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
static GLint texture_draw_bound = -1;
static GLint viewport_vec2 = -1;

Load<GLuint> fadeout_program(LoadTagInit, []()
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

Load<GLuint> expanding_bounds_program(LoadTagInit, []()
{
    GLuint *ret = new GLuint(compile_program(
        "#version 330\n"
        "void main() {\n"
        "	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
        "}\n",
        "#version 330\n"
        "uniform sampler2D gateway_tex;\n"
        "uniform float bounds;\n"
        "uniform vec2 viewport;\n"
        "in vec4 gl_FragCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "       vec2 lower_margin = viewport / 3.0f;\n"
        "       vec2 upper_margin = 2.0f * viewport / 3.0f;\n"
        "       vec2 scaling = 1.0f / (upper_margin - lower_margin);\n"
        "       vec2 norm_coord = gl_FragCoord.xy / viewport;\n"
        "       if (norm_coord.x >= bounds && norm_coord.y >= bounds && \n"
        "           norm_coord.x <= (1.0f - bounds) && norm_coord.y <= (1.0f - bounds)) {\n"
        "           fragColor = texture(gateway_tex, norm_coord);\n"
        "       } else {\n"
        "           fragColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);\n"
        "       }\n"
        "}\n"
    ));

    texture_draw_bound = glGetUniformLocation(*ret, "bounds");
    viewport_vec2 = glGetUniformLocation(*ret, "viewport");

    return ret;
});

TransitionMode::TransitionMode(GLuint target_texture_id, std::shared_ptr<bool> reset)
    : target_texture_id(target_texture_id), reset(reset)
{}

bool TransitionMode::handle_event(SDL_Event const &e, glm::uvec2 const &window_size)
{
    return false;
}

void TransitionMode::update(float elapsed)
{
    if (bounds <= 0.0f) {
      background_fade = std::min(1.0f, background_fade + fade_speed * elapsed);
    }

    bounds = std::max(0.0f, bounds - expand_speed * elapsed);

    if (background_fade >= 1.0f) {
        *reset = true;
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

        // binding the gateway texture to index 3
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, target_texture_id);

        glUseProgram(*expanding_bounds_program);
        glUniform1f(texture_draw_bound, bounds);
        glUniform2fv(viewport_vec2, 1, glm::value_ptr(glm::vec2(drawable_size.x, drawable_size.y)));
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);


        glUseProgram(*fadeout_program);
        glUniform4fv(fade_program_color, 1, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, background_fade)));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glUseProgram(0);
        glDisable(GL_BLEND);
    }

    glEnable(GL_DEPTH_TEST);
}
