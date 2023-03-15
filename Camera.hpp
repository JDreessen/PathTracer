//
// Created by joshua on 09.09.22.
//

#ifndef PATHTRACER_CAMERA_HPP
#define PATHTRACER_CAMERA_HPP


#include "glm/vec3.hpp"
#include "glm/geometric.hpp"
#include "glm/gtx/quaternion.hpp"

class Camera {
    glm::vec3 position{};
    glm::vec3 direction{};
    glm::vec3 v{};
    glm::vec3 u{};
    float near;
    float far;
    float fov;

public:
    Camera();
    Camera(glm::vec3 position, glm::vec3 direction, glm::vec3 up, float near, float far, float fov);

    glm::vec3 getPosition() const;
    glm::vec3 getDirection() const;
    glm::vec3 getUp() const;
    glm::vec3 getRight() const;
    float getNear() const;
    float getFar() const;
    float getFov() const;

    void setPosition(glm::vec3 position);
    void setDirection(glm::vec3 direction);
    void setNear(float near);
    void setFar(float far);
    void setFov(float fov);

    void move(glm::vec3 delta);
    void rotate(float angleX, float angleY);
    glm::mat4 getRotationMatrix();
};


#endif //PATHTRACER_CAMERA_HPP
