#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <random>

// The 'GameMode' mode is the main gameplay mode:

struct GameMode: public Mode
{
    GameMode();
    virtual ~GameMode();

    //handle_event is called when new mouse or keyboard events are received:
    // (note that this might be many times per frame or never)
    //The function should return 'true' if it handled the event.
    virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

    //update is called at the start of a new frame, after events are handled:
    virtual void update(float elapsed) override;

    //draw is called after update:
    virtual void draw(glm::uvec2 const &drawable_size) override;

    void reset_game();

    struct StoneInfo
    {
        Scene::Object *stone;
        float velocity;
        float angle;
        glm::vec3 axis;

        StoneInfo(Scene::Object *stone, const float velocity, const float angle, const glm::vec3 axis)
            : stone(stone), velocity(velocity), angle(angle), axis(axis)
        {}
    };

    struct Controls
    {
        bool snap;
    };

    Controls current_controls;
    float viewpoint_angle = 0.0f;
    float spot_spin = 0.0f;
    float current_time = 0.0f;
    float target_time;
    float target_viewpoint_angle;
    static const uint32_t asteroid_num = 60;
    Scene::Camera *target_camera = nullptr;
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution_x, distribution_y, distribution_z, distribution_axis,
        distribution_velocity, distribution_time, distribution_angle;
    std::uniform_int_distribution<uint32_t> distribution_mesh;
    std::vector<StoneInfo> stones;

};
