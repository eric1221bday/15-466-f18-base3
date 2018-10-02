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

class TransitionMode: public Mode
{
private:
    bool handle_event(SDL_Event const &event, glm::uvec2 const &window_size) override;
    void update(float elapsed) override;
    void draw(glm::uvec2 const &drawable_size) override;

    GLuint target_texture_id;
    float background_fade = 1.0f;
    float fade_speed = 1.0f;

public:
    explicit TransitionMode(GLuint target_texture_id);

    virtual ~TransitionMode() = default;

    //will render this mode in the background if not null:
    std::shared_ptr<Mode> background;
};


#endif //SHADY_BUSINESS_TRANSITIONMODE_H
