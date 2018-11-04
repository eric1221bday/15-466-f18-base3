#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "TransitionMode.h"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "load_save_png.hpp"
#include "texture_program.hpp"
#include "depth_program.hpp"
#include "shady_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <ctime>


Load<MeshBuffer> meshes(LoadTagDefault, []()
{
    return new MeshBuffer(data_path("gateway.pnct"));
});

Load<GLuint> meshes_for_texture_program(LoadTagDefault, []()
{
    return new GLuint(meshes->make_vao_for_program(texture_program->program));
});

Load<GLuint> meshes_for_shady_program(LoadTagDefault, []()
{
    return new GLuint(meshes->make_vao_for_program(shady_program->program));
});

Load<GLuint> meshes_for_depth_program(LoadTagDefault, []()
{
    return new GLuint(meshes->make_vao_for_program(depth_program->program));
});

//used for fullscreen passes:
Load<GLuint> empty_vao(LoadTagDefault, []()
{
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindVertexArray(0);
    return new GLuint(vao);
});

GLuint load_texture(std::string const &filename)
{
    glm::uvec2 size;
    std::vector<glm::u8vec4> data;
    load_png(filename, &size, &data, LowerLeftOrigin);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ERRORS();

    return tex;
}

static Scene::Transform *camera_parent_transform = nullptr;

static Scene::Camera *camera = nullptr;

static Scene::Transform *spot_parent_transform = nullptr;

static Scene::Lamp *spot = nullptr;

static std::vector<std::string> stone_types = {};

static std::vector<GLuint *> images = {};

static Scene *current_scene = nullptr;

Load<GLuint> wood_tex(LoadTagDefault, []()
{
    return new GLuint(load_texture(data_path("textures/wood.png")));
});

Load<GLuint> marble_tex(LoadTagDefault, []()
{
    return new GLuint(load_texture(data_path("textures/marble.png")));
});

Load<GLuint> stone_tex(LoadTagDefault, []()
{
    return new GLuint(load_texture(data_path("textures/Stones_01_Atlas_Diffuse_01.png")));
});

Load<GLuint> hourglass_neb_tex(LoadTagDefault, []()
{
    GLuint *tex = new GLuint(load_texture(data_path("textures/hst_hourglass_nebula.png")));
    images.push_back(tex);
    return tex;
});

Load<GLuint> hst_lagoon_detail_tex(LoadTagDefault, []()
{
    GLuint *tex = new GLuint(load_texture(data_path("textures/hst_lagoon_detail.png")));
    images.push_back(tex);
    return tex;
});

Load<GLuint> hst_orion_nebula_tex(LoadTagDefault, []()
{
    GLuint *tex = new GLuint(load_texture(data_path("textures/hst_orion_nebula.png")));
    images.push_back(tex);
    return tex;
});

Load<GLuint> hst_pillars_m16_close_tex(LoadTagDefault, []()
{
    GLuint *tex = new GLuint(load_texture(data_path("textures/hst_pillars_m16_close.png")));
    images.push_back(tex);
    return tex;
});

Load<GLuint> hst_stingray_nebula_tex(LoadTagDefault, []()
{
    GLuint *tex = new GLuint(load_texture(data_path("textures/hst_stingray_nebula.png")));
    images.push_back(tex);
    return tex;
});

Load<GLuint> white_tex(LoadTagDefault, []()
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glm::u8vec4 white(0xff, 0xff, 0xff, 0xff);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, glm::value_ptr(white));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return new GLuint(tex);
});

Load<Scene> scene(LoadTagDefault, []()
{
    Scene *ret = new Scene;
    current_scene = ret;

    //load transform hierarchy:
    ret->load(data_path("gateway.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m)
    {
        if (m.find("Stone") != std::string::npos) {
            stone_types.push_back(m);
        }
    });

    //look up camera parent transform:
    for (Scene::Transform *t = ret->first_transform; t != nullptr; t = t->alloc_next) {
        if (t->name == "CameraParent") {
            if (camera_parent_transform) throw std::runtime_error("Multiple 'CameraParent' transforms in scene.");
            camera_parent_transform = t;
        }
        if (t->name == "SpotParent") {
            if (spot_parent_transform) throw std::runtime_error("Multiple 'SpotParent' transforms in scene.");
            spot_parent_transform = t;
        }

    }
    if (!camera_parent_transform) throw std::runtime_error("No 'CameraParent' transform in scene.");
    if (!spot_parent_transform) throw std::runtime_error("No 'SpotParent' transform in scene.");

    //look up the camera:
    for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
        if (c->transform->name == "Camera") {
            if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
            camera = c;
        }
    }
    if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

    //look up the spotlight:
    for (Scene::Lamp *l = ret->first_lamp; l != nullptr; l = l->alloc_next) {
        if (l->transform->name == "Spot") {
            if (spot) throw std::runtime_error("Multiple 'Spot' objects in scene.");
            if (l->type != Scene::Lamp::Spot) throw std::runtime_error("Lamp 'Spot' is not a spotlight.");
            spot = l;
        }
    }
    if (!spot) throw std::runtime_error("No 'Spot' spotlight in scene.");

    return ret;
});

GameMode::GameMode()
    : generator(std::time(nullptr)),
      distribution_x(-4.0f, 4.0f),
      distribution_y(-4.0f, 4.0f),
      distribution_z(0.0f, 4.0f),
      distribution_axis(-1.0f, 1.0f),
      distribution_velocity(0.0f, 6.0f),
      distribution_time(0.0f, 1.0f),
      distribution_angle(0.0f, 360.f),
      distribution_mesh(0, stone_types.size() - 1),
      distribution_images(0, images.size() - 1)
{
    Scene::Object::ProgramInfo shady_program_info;
    shady_program_info.program = shady_program->program;
    shady_program_info.vao = *meshes_for_shady_program;
    shady_program_info.mvp_mat4 = shady_program->object_to_clip_mat4;
    shady_program_info.mv_mat4 = shady_program->object_to_light_mat4;
    shady_program_info.itmv_mat3 = shady_program->normal_to_light_mat3;

    Scene::Object::ProgramInfo depth_program_info;
    depth_program_info.program = depth_program->program;
    depth_program_info.vao = *meshes_for_depth_program;
    depth_program_info.mvp_mat4 = depth_program->object_to_clip_mat4;

    for (uint32_t i = 0; i < asteroid_num; i++) {
        Scene::Transform *t = current_scene->new_transform();

        Scene::Object *obj = current_scene->new_object(t);
        obj->programs[Scene::Object::ProgramTypeDefault] = shady_program_info;
        obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *stone_tex;

        obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

        MeshBuffer::Mesh const &mesh = meshes->lookup(stone_types[distribution_mesh(generator)]);
        obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

        obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;

        stones.emplace_back(obj, 0.0f, 0.0f, glm::vec3());
    }

    reset = std::make_shared<bool>(false);

    reset_game();
}

GameMode::~GameMode()
{
}

void GameMode::reset_game()
{
    target_time = distribution_time(generator);
//    target_time = 0.0f;
    target_viewpoint_angle = glm::radians(distribution_angle(generator));
//    target_viewpoint_angle = 0.0f;
    viewpoint_angle = glm::radians(distribution_angle(generator));
//    viewpoint_angle = 0.0f;
    current_time = distribution_time(generator);
//    current_time = 0.0f;

    current_target_texture = *images[distribution_images(generator)];

    for (auto &info : stones) {
        info.stone->transform->scale = glm::vec3(0.03f, 0.03f, 0.03f);
        info.stone->transform->position =
            glm::vec3(distribution_x(generator), distribution_y(generator), distribution_z(generator));
        info.axis = glm::normalize(glm::vec3(distribution_axis(generator),
                                             distribution_axis(generator),
                                             distribution_axis(generator)));
        info.angle = distribution_angle(generator);
        info.stone->transform->rotation = glm::angleAxis(info.angle, info.axis);
        info.velocity = distribution_velocity(generator);
    }
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{
    //ignore any keys that are the result of automatic key repeat:
    if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
        return false;
    }

    // handle tracking the state of WSAD for movement control:
    if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
        if (evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
            current_controls.snap = (evt.type == SDL_KEYDOWN);
            return true;
        }
    }

    if (evt.type == SDL_MOUSEMOTION) {
        if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            viewpoint_angle += 5.0f * evt.motion.xrel / float(window_size.x);
            current_time = glm::clamp(current_time + 2.0f * evt.motion.yrel / float(window_size.y), 0.0f, 1.0f);
            return true;
        }
        if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            spot_spin += 5.0f * evt.motion.xrel / float(window_size.x);
            return true;
        }

    }

    return false;
}

void GameMode::update(float elapsed)
{
    if (*reset) {
        reset_game();
        *reset = false;
    }

    camera_parent_transform->rotation = glm::angleAxis(viewpoint_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    spot_parent_transform->rotation = glm::angleAxis(spot_spin, glm::vec3(0.0f, 0.0f, 1.0f));

    for (auto &info : stones) {
        info.stone->transform->rotation = glm::angleAxis(current_time * info.velocity + info.angle, info.axis);
    }

    if (current_controls.snap) {
        current_controls.snap = false;

        if (std::abs(current_time - target_time) < 0.05f && std::abs(std::fmod(viewpoint_angle, 2*M_PI) - target_viewpoint_angle) < 0.1) {
            current_time = target_time;
            viewpoint_angle = target_viewpoint_angle;

            show_transition();
        }
    }
}

//GameMode will render to some offscreen framebuffer(s).
//This code allocates and resizes them as needed:
struct Framebuffers
{
    glm::uvec2 size = glm::uvec2(0, 0); //remember the size of the framebuffer

    //This framebuffer is used for fullscreen effects:
    GLuint color_tex = 0;
    GLuint depth_tex = 0;
    GLuint fb = 0;

    //This framebuffer is used for shadow maps:
    glm::uvec2 shadow_size = glm::uvec2(0, 0);
    GLuint shadow_color_tex = 0; //DEBUG
    GLuint shadow_depth_tex = 0;
    GLuint shadow_fb = 0;

    void allocate(glm::uvec2 const &new_size, glm::uvec2 const &new_shadow_size)
    {
        //allocate full-screen framebuffer:
        if (size != new_size) {
            size = new_size;

            if (color_tex == 0) glGenTextures(1, &color_tex);
            glBindTexture(GL_TEXTURE_2D, color_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            if (depth_tex == 0) glGenTextures(1, &depth_tex);
            glBindTexture(GL_TEXTURE_2D, depth_tex);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_DEPTH_COMPONENT24,
                         size.x,
                         size.y,
                         0,
                         GL_DEPTH_COMPONENT,
                         GL_UNSIGNED_BYTE,
                         NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            if (fb == 0) glGenFramebuffers(1, &fb);
            glBindFramebuffer(GL_FRAMEBUFFER, fb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
            check_fb();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            GL_ERRORS();
        }

        //allocate shadow map framebuffer:
        if (shadow_size != new_shadow_size) {
            shadow_size = new_shadow_size;

            if (shadow_color_tex == 0) glGenTextures(1, &shadow_color_tex);
            glBindTexture(GL_TEXTURE_2D, shadow_color_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shadow_size.x, shadow_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);


            if (shadow_depth_tex == 0) glGenTextures(1, &shadow_depth_tex);
            glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_DEPTH_COMPONENT24,
                         shadow_size.x,
                         shadow_size.y,
                         0,
                         GL_DEPTH_COMPONENT,
                         GL_UNSIGNED_BYTE,
                         NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            if (shadow_fb == 0) glGenFramebuffers(1, &shadow_fb);
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadow_color_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth_tex, 0);
            check_fb();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            GL_ERRORS();
        }
    }
} fbs;

void GameMode::draw(glm::uvec2 const &drawable_size)
{
    fbs.allocate(drawable_size, glm::uvec2(512, 512));

    //Draw scene to shadow map for spotlight:
    glBindFramebuffer(GL_FRAMEBUFFER, fbs.shadow_fb);
    glViewport(0, 0, fbs.shadow_size.x, fbs.shadow_size.y);

    glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    //render only back faces to shadow map (prevent shadow speckles on fronts of objects):
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);

    scene->draw(spot, Scene::Object::ProgramTypeShadow);

    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GL_ERRORS();

    glm::mat4 target_camera_projection;
    glm::mat4 target_camera_world_to_local;
    glm::mat4 target_camera_to_world;

    {
        //Draw scene to shadow map for target viewpoint:
        glBindFramebuffer(GL_FRAMEBUFFER, fbs.fb);
        glViewport(0, 0, drawable_size.x, drawable_size.y);
        camera->aspect = drawable_size.x / float(drawable_size.y);

        glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        //render only back faces to shadow map (prevent shadow speckles on fronts of objects):
        glCullFace(GL_FRONT);
        glEnable(GL_CULL_FACE);

        // set camera to target viewpoint
        camera_parent_transform->rotation = glm::angleAxis(target_viewpoint_angle, glm::vec3(0.0f, 0.0f, 1.0f));

        // set stones to target time
        for (auto &info : stones) {
            info.stone->transform->rotation = glm::angleAxis(target_time * info.velocity + info.angle, info.axis);
        }

        scene->draw(camera, Scene::Object::ProgramTypeShadow);

        // reset stones to current time
        for (auto &info : stones) {
            info.stone->transform->rotation = glm::angleAxis(current_time * info.velocity + info.angle, info.axis);
        }

        target_camera_projection = camera->make_projection();
        target_camera_world_to_local = camera->transform->make_world_to_local();
        target_camera_to_world = camera->transform->make_local_to_world();

        // reset camera to current viewpoint
        camera_parent_transform->rotation = glm::angleAxis(viewpoint_angle, glm::vec3(0.0f, 0.0f, 1.0f));

        glDisable(GL_CULL_FACE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        GL_ERRORS();
    }

    glViewport(0, 0, drawable_size.x, drawable_size.y);
    camera->aspect = drawable_size.x / float(drawable_size.y);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    {
        //set up light positions:
        glUseProgram(texture_program->program);

        //don't use distant directional light at all (color == 0):
        glUniform3fv(texture_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
        glUniform3fv(texture_program->sun_direction_vec3,
                     1,
                     glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f))));
        //use hemisphere light for subtle ambient light:
        glUniform3fv(texture_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
        glUniform3fv(texture_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

        glm::mat4 world_to_spot =
            //This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture
            // coordinates ([0,1]^2) and depth map Z values ([0,1]):
            glm::mat4(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.5f, 0.5f, 0.5f + 0.00001f /* <-- bias */, 1.0f
            )
                //this is the world-to-clip matrix used when rendering the shadow map:
                * spot->make_projection() * spot->transform->make_world_to_local();

        glUniformMatrix4fv(texture_program->light_to_spot_mat4, 1, GL_FALSE, glm::value_ptr(world_to_spot));

        glm::mat4 spot_to_world = spot->transform->make_local_to_world();
        glUniform3fv(texture_program->spot_position_vec3, 1, glm::value_ptr(glm::vec3(spot_to_world[3])));
        glUniform3fv(texture_program->spot_direction_vec3, 1, glm::value_ptr(-glm::vec3(spot_to_world[2])));
        glUniform3fv(texture_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

        glm::vec2 spot_outer_inner = glm::vec2(std::cos(0.5f * spot->fov), std::cos(0.85f * 0.5f * spot->fov));
        glUniform2fv(texture_program->spot_outer_inner_vec2, 1, glm::value_ptr(spot_outer_inner));
    }

    {
        //set up light positions:
        glUseProgram(shady_program->program);

        //don't use distant directional light at all (color == 0):
        glUniform3fv(shady_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
        glUniform3fv(shady_program->sun_direction_vec3,
                     1,
                     glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f))));
        //use hemisphere light for subtle ambient light:
        glUniform3fv(shady_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
        glUniform3fv(shady_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

        glm::mat4 world_to_spot =
            //This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture
            // coordinates ([0,1]^2) and depth map Z values ([0,1]):
            glm::mat4(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.5f, 0.5f, 0.5f + 0.00001f /* <-- bias */, 1.0f
            )
                //this is the world-to-clip matrix used when rendering the shadow map:
                * spot->make_projection() * spot->transform->make_world_to_local();

        glUniformMatrix4fv(shady_program->light_to_spot_mat4, 1, GL_FALSE, glm::value_ptr(world_to_spot));

        glm::mat4 spot_to_world = spot->transform->make_local_to_world();
        glUniform3fv(shady_program->spot_position_vec3, 1, glm::value_ptr(glm::vec3(spot_to_world[3])));
        glUniform3fv(shady_program->spot_direction_vec3, 1, glm::value_ptr(-glm::vec3(spot_to_world[2])));
        glUniform3fv(shady_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

        glm::vec2 spot_outer_inner = glm::vec2(std::cos(0.5f * spot->fov), std::cos(0.85f * 0.5f * spot->fov));
        glUniform2fv(shady_program->spot_outer_inner_vec2, 1, glm::value_ptr(spot_outer_inner));

        glm::mat4 world_to_target =
            glm::mat4(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.5f, 0.5f, 0.5f + 0.00001f /* <-- bias */, 1.0f
            )
                //this is the world-to-clip matrix used when rendering the shadow map:
                * target_camera_projection * target_camera_world_to_local;

        glUniformMatrix4fv(shady_program->light_to_target_mat4, 1, GL_FALSE, glm::value_ptr(world_to_target));

        glUniform3fv(shady_program->target_position_vec3, 1, glm::value_ptr(glm::vec3(target_camera_to_world[3])));
        glUniform3fv(shady_program->target_direction_vec3, 1, glm::value_ptr(-glm::vec3(target_camera_to_world[2])));

        glUniform2fv(shady_program->screen_size_vec2, 1, glm::value_ptr(glm::vec2(drawable_size.x, drawable_size.y)));
    }


    //This code binds texture index 1 to the shadow map:
    // (note that this is a bit brittle -- it depends on none of the objects in the scene having a texture of
    // index 1 set in their material data; otherwise scene::draw would unbind this texture):
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fbs.shadow_depth_tex);
    //The shadow_depth_tex must have these parameters set to be used as a sampler2DShadow in the shader:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    //NOTE: however, these are parameters of the texture object, not the binding point, so there is no need to set
    // them *each frame*. I'm doing it here so that you are likely to see that they are being set.

    // binding the target shadow map to index 2
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, fbs.depth_tex);
    //The shadow_depth_tex must have these parameters set to be used as a sampler2DShadow in the shader:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

    // binding the gateway texture to index 3
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, current_target_texture);

    glActiveTexture(GL_TEXTURE0);

    scene->draw(camera);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GL_ERRORS();
}

void GameMode::show_transition()
{
    std::shared_ptr<TransitionMode> transition = std::make_shared<TransitionMode>(current_target_texture, reset);

    std::shared_ptr<Mode> game = shared_from_this();
    transition->background = game;

    Mode::set_current(transition);
}
