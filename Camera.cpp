//
// Created by joshua on 09.09.22.
//

#include "Camera.hpp"

Camera::Camera() : position(glm::vec3(0, 0, 0)), direction(glm::vec3(0, 0, 1)),
    u(glm::cross(glm::vec3(0, 1, 0), direction)),
    v(glm::cross(direction, u)), near(0.1f), far(1000.0f), fov(90.0f) {}

Camera::Camera(glm::vec3 position, glm::vec3 direction, glm::vec3 up, float near, float far, float fov)
    : position(position), direction(direction), u(glm::cross(up, direction)), v(glm::cross(direction, u)),
    near(near), far(far), fov(fov) {}

glm::vec3 Camera::getPosition() const { return this->position; }

glm::vec3 Camera::getDirection() const { return this->direction; }

glm::vec3 Camera::getUp() const { return this->v; }

glm::vec3 Camera::getRight() const { return this->u; }

float Camera::getNear() const { return this->near; }

float Camera::getFar() const { return this->far; }

float Camera::getFov() const { return this->fov; }

void Camera::setPosition(glm::vec3 position) { this->position = position; }

void Camera::setDirection(glm::vec3 direction) {
    this->direction = direction;
    this->u = glm::cross(glm::vec3(0, 1, 0), direction);
    this->v = glm::cross(direction, u);
}

void Camera::setNear(float near) { this->near = near; }

void Camera::setFar(float far) { this->far = far; }

void Camera::setFov(float fov) { this->fov = fov; }

void Camera::move(glm::vec3 delta) { this->position += delta; }

void Camera::rotate(float angleX, float angleY) {
    glm::quat pitchQ = glm::angleAxis(angleY, this->u);
    glm::quat yawQ = glm::angleAxis(angleX, this->v);
    glm::quat rotationQ = pitchQ * yawQ;
    setDirection(glm::rotate(rotationQ, this->direction));
}
// get rotation matrix of camera
glm::mat4 Camera::getRotationMatrix() {
    glm::vec3 x = glm::normalize(this->u);
    glm::vec3 y = glm::normalize(this->v);
    glm::vec3 z = glm::normalize(this->direction);
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix[0][0] = x.x;
    rotationMatrix[1][0] = x.y;
    rotationMatrix[2][0] = x.z;
    rotationMatrix[0][1] = y.x;
    rotationMatrix[1][1] = y.y;
    rotationMatrix[2][1] = y.z;
    rotationMatrix[0][2] = z.x;
    rotationMatrix[1][2] = z.y;
    rotationMatrix[2][2] = z.z;
    return rotationMatrix;
}