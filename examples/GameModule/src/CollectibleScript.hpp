#pragma once
#include <Kizuri.hpp>

// Coletável: some ao tocar em qualquer coisa com script/corpo (ex.: jogador).
// Como testar: entidade com Circle/Sprite + BoxCollider2D + Rigidbody2D Static
// + este script. Player com Rigidbody2D Dynamic. Play → encostar → some.
class CollectibleScript : public kizuri::NativeScript {
protected:
    void OnCollisionBegin(kizuri::Entity other) override {
        (void)other;
        DestroyEntity();
    }
};
