//
// Created by joshua on 09.09.22.
//

#include "Camera.hpp"

Camera::Camera() {
    position = glm::vec3(0, 0, 0);
    direction = glm::vec3(0, 0, 1);
    up = glm::vec3(0, 1, 0);
    right = glm::cross(direction, up);
    near = 0.1f;
    far = 1000.0f;
    fov = 90.0f;

}

Camera::Camera(glm::vec3 position, glm::vec3 direction, glm::vec3 up, float near, float far, float fov) {
    this->position = position;
    this->direction = direction;
    this->up = up;
    this->right = glm::cross(direction, up);
    this->near = near;
    this->far = far;
    this->fov = fov;
}

glm::vec3 Camera::getPosition() const { return this->position; }

glm::vec3 Camera::getDirection() const { return this->direction; }

glm::vec3 Camera::getUp() const { return this->up; }

glm::vec3 Camera::getRight() const { return this->right; }

float Camera::getNear() const { return this->near; }

float Camera::getFar() const { return this->far; }

float Camera::getFov() const { return this->fov; }

void Camera::setPosition(glm::vec3 position) { this->position = position; }

void Camera::setDirection(glm::vec3 direction) {
    this->direction = direction;
    this->right = glm::cross(direction, up);
}