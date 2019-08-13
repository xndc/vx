#pragma once
#include "common.h"
#include "data/model.h"

#define LAYER_ALL             0xFFFFFFFF
#define LAYER_VISIBLE         (1 << 0)
#define LAYER_SHADOW_CASTER   (1 << 1)
#define LAYER_SHADOW_RECEIVER (1 << 2)

typedef struct GameObject {
    struct GameObject* parent;
    uint32_t layers;
    mat4 localTransform;
    mat4 worldTransform;
    enum GameObject_Type {
        GAMEOBJECT_NONE,
        GAMEOBJECT_MESH,
        GAMEOBJECT_PROBE,
        GAMEOBJECT_POINT_LIGHT,
    } type;
    union GameObject_Data {
        Mesh* mesh;

        struct GameObject_Data_Probe {
            vec3 position;
            union { vec3 color_zp; vec3 color_u; };
            union { vec3 color_zn; vec3 color_d; };
            union { vec3 color_yp; vec3 color_n; };
            union { vec3 color_yn; vec3 color_s; };
            union { vec3 color_xp; vec3 color_e; };
            union { vec3 color_xn; vec3 color_w; };
        } probe;

        struct GameObject_Data_DirectionalLight {
            vec3 direction;
            vec3 color; // HDR
        };

        struct GameObject_Data_PointLight {
            vec3 position;
            vec3 color; // HDR
        } pointLight;
    };
} GameObject;

static inline void InitGameObject (GameObject* obj, GameObject* parent) {
    obj->parent = parent;
    glm_mat4_identity(obj->localTransform);
    glm_mat4_identity(obj->worldTransform);
    obj->type = GAMEOBJECT_NONE;
}

static inline void InitGameObject_Mesh (GameObject* obj, GameObject* parent, Mesh* mesh) {
    InitGameObject(obj, parent);
    obj->type = GAMEOBJECT_MESH;
    obj->mesh = mesh;
    obj->layers = LAYER_VISIBLE | LAYER_SHADOW_CASTER | LAYER_SHADOW_RECEIVER;
}

static inline void InitGameObject_Probe (GameObject* obj, vec3 position, float intensity) {
    InitGameObject(obj, NULL);
    obj->type = GAMEOBJECT_PROBE;
    glm_vec3_copy(position, obj->probe.position);
    // Fill with some default colours:
    float i = intensity;
    glm_vec3_copy((vec3) {1.0f * i, 1.0f * i, 1.0f * i}, obj->probe.color_u);
    glm_vec3_copy((vec3) {0.5f * i, 0.5f * i, 0.5f * i}, obj->probe.color_d);
    glm_vec3_copy((vec3) {0.8f * i, 0.8f * i, 1.0f * i}, obj->probe.color_n);
    glm_vec3_copy((vec3) {0.6f * i, 0.5f * i, 0.7f * i}, obj->probe.color_s);
    glm_vec3_copy((vec3) {0.8f * i, 0.7f * i, 0.9f * i}, obj->probe.color_w);
    glm_vec3_copy((vec3) {0.8f * i, 0.7f * i, 0.9f * i}, obj->probe.color_e);
}

void InitScene();

GameObject* CreateGameObject (GameObject* parent);
GameObject* CreateGameObject_Mesh (GameObject* parent, Mesh* mesh);
GameObject* CreateGameObject_Probe (vec3 position, float intensity);
GameObject* CreateGameObject_DirectionalLight (vec3 color);
GameObject* CreateGameObject_PointLight (GameObject* parent, vec3 position, vec3 color);

size_t GetSceneMeshCount  (uint32_t layers);
size_t GetMeshesFromScene (uint32_t layers, size_t bufsize, Mesh* buffer);

size_t GetSceneProbeCount ();
size_t GetProbesFromScene (size_t bufsize, struct GameObject_Data_Probes* buffer);

size_t GetSceneDirectionalLightCount ();
size_t GetDirectionalLightsFromScene (size_t bufsize, struct GameObject_Data_DirectionalLight* buffer);

size_t GetScenePointLightCount ();
size_t GetPointLightsFromScene (size_t bufsize, struct GameObject_Data_PointLight* buffer);