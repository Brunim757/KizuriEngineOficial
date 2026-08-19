#include "kizuri/renderer/Camera.hpp"

namespace kizuri {


OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
    : m_ProjectionMatrix(glm::ortho(left, right, bottom, top, -1.0f, 1.0f)), m_ViewMatrix(1.0f) {
    m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
}

void OrthographicCamera::SetProjection(float left, float right, float bottom, float top) {
    m_ProjectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
}

void OrthographicCamera::RecalculateViewMatrix() {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
                           glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));
    m_ViewMatrix = glm::inverse(transform);
    m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
}


PerspectiveCamera::PerspectiveCamera(float fovDeg, float aspectRatio, float nearClip, float farClip) {
    SetPerspective(fovDeg, aspectRatio, nearClip, farClip);
    RecalculateViewMatrix();
}

void PerspectiveCamera::SetPerspective(float fovDeg, float aspectRatio, float nearClip, float farClip) {
    m_FOV = fovDeg; m_Aspect = aspectRatio; m_Near = nearClip; m_Far = farClip;
    m_ProjectionMatrix = glm::perspective(glm::radians(fovDeg), aspectRatio, nearClip, farClip);
}

void PerspectiveCamera::RecalculateViewMatrix() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    front.y = sin(glm::radians(m_Pitch));
    front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    m_Forward = glm::normalize(front);
    m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

void PerspectiveCamera::SetWorldTransform(const glm::mat4& world) {
    
    
    
    m_Position = glm::vec3(world[3]);
    glm::vec3 forward = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 up = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 1.0f, 0.0f));
    m_Forward = forward;
    m_ViewMatrix = glm::lookAt(m_Position, m_Position + forward, up);
}

} 
