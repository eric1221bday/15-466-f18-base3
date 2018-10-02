//
// Created by Eric Fang on 10/1/18.
//

#ifndef SHADY_BUSINESS_TRANSITIONMODE_H
#define SHADY_BUSINESS_TRANSITIONMODE_H

#include "Mode.hpp"
#include "GL.hpp"

#include <functional>
#include <vector>
#include <string>
#include <memory>

class TransitionMode: public Mode
{
private:
    bool handle_event(SDL_Event const &event, glm::uvec2 const &window_size) override;
    void update(float elapsed) override;
    void draw(glm::uvec2 const &drawable_size) override;

    GLuint target_texture_id;
    float background_fade = 0.0f;
    float fade_speed = 0.4f;
    float bounds = 1.0f/3.0f;
    float expand_speed = 1.f;

public:
    TransitionMode(GLuint target_texture_id, std::shared_ptr<bool> reset);

    virtual ~TransitionMode() = default;

    //will render this mode in the background if not null:
    std::shared_ptr<Mode> background;

    // trigger reset in main game
    std::shared_ptr<bool> reset;
};


#endif //SHADY_BUSINESS_TRANSITIONMODE_H
