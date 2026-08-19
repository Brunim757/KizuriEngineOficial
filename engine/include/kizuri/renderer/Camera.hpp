#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace kizuri {

class OrthographicCamera {
public:
    OrthographicCamera(float left, float right, float bottom, float top);

    void SetProjection(float left, float right, float bottom, float top);

    const glm::vec3& GetPosition() const { return m_Position; }
    void SetPosition(const glm::vec3& pos) { m_Position = pos; RecalculateViewMatrix(); }

    float GetRotation() const { return m_Rotation; }
    void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }

    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
    const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

private:
    void RecalculateViewMatrix();

    glm::mat4 m_ProjectionMatrix;
    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ViewProjectionMatrix;

    glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
    float m_Rotation = 0.0f;
};

class PerspectiveCamera {
public:
    PerspectiveCamera(float fovDeg, float aspectRatio, float nearClip, float farClip);

    void SetPerspective(float fovDeg, float aspectRatio, float nearClip, float farClip);
    void SetPosition(const glm::vec3& pos) { m_Position = pos; RecalculateViewMatrix(); }
    void SetRotation(float yawDeg, float pitchDeg) { m_Yaw = yawDeg; m_Pitch = pitchDeg; RecalculateViewMatrix(); }

    void SetWorldTransform(const glm::mat4& world);

    const glm::vec3& GetPosition() const { return m_Position; }
    glm::vec3 GetForward() const { return m_Forward; }
    float GetFOV() const { return m_FOV; }
    float GetAspect() const { return m_Aspect; }
    float GetNearClip() const { return m_Near; }
    float GetFarClip() const { return m_Far; }

    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
    glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

private:
    void RecalculateViewMatrix();

    glm::mat4 m_ProjectionMatrix{ 1.0f };
    glm::mat4 m_ViewMatrix{ 1.0f };

    glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };
    glm::vec3 m_Forward{ 0.0f, 0.0f, -1.0f };
    float m_Yaw = -90.0f, m_Pitch = 0.0f;
    float m_FOV = 45.0f, m_Aspect = 16.0f / 9.0f, m_Near = 0.1f, m_Far = 1000.0f;
};

}
