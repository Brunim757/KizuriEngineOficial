#pragma once
#include <Kizuri.hpp>





class PlayerController : public kizuri::NativeScript {
protected:
    void OnCreate() override;
    void OnUpdate(kizuri::Timestep ts) override;

private:
    float m_Speed = 5.0f;
};
