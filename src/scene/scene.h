#pragma once
#include "common.h"
#include "data/model.h"

#define LAYER_VISIBLE         (1 << 0)
#define LAYER_SHADOW_CASTER   (1 << 1)
#define LAYER_SHADOW_RECEIVER (1 << 2)

typedef struct GameObject {
    GameObject* parent;
    uint32_t layers;
    mat4 localTransform;
    mat4 worldTransform;
    enum GameObject_Type {
        GAMEOBJECT_NONE,
        GAMEOBJECT_MODEL,
        GAMEOBJECT_POINT_LIGHT,
    } type;
    union GameObject_Data {
        Model* model;
        struct GameObject_Data_PointLight {
            vec3 position;
            vec3 color;
            float intensity;
        } pointLight;
    };
} GameObject;

static inline void InitGameObject (GameObject* obj, GameObject* parent) {
    obj->parent = parent;
    glm_mat4_identity(obj->localTransform);
    glm_mat4_identity(obj->worldTransform);
    obj->type = GAMEOBJECT_NONE;
}

static inline void InitGameObject_Model (GameObject* obj, GameObject* parent, Model* model) {
    InitGameObject(obj, parent);
    obj->type = GAMEOBJECT_MODEL;
    obj->model = model;
    obj->layers = LAYER_VISIBLE | LAYER_SHADOW_CASTER | LAYER_SHADOW_RECEIVER;
}