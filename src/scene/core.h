#pragma once
#include "common.h"
#include "data/model.h"

typedef enum GameObjectType {
    GAMEOBJECT_NULL,
    GAMEOBJECT_MODEL,
    GAMEOBJECT_DIRECTIONAL_LIGHT,
    GAMEOBJECT_POINT_LIGHT,
    GAMEOBJECT_LIGHT_PROBE,
} GameObjectType;

typedef struct GameObject_Model {
    Model* model;
} GameObject_Model;

typedef struct GameObject_DirectionalLight {
    vec3 color;
    bool editorIntensityMode;
} GameObject_DirectionalLight;

typedef struct GameObject_PointLight {
    vec3 color;
    bool editorIntensityMode;
} GameObject_PointLight;

typedef struct GameObject_LightProbe {
    vec3 colorXp;
    vec3 colorXn;
    vec3 colorYp;
    vec3 colorYn;
    vec3 colorZp;
    vec3 colorZn;
    bool editorIntensityMode;
} GameObject_LightProbe;

typedef struct GameObject {
    GameObjectType type;
    struct GameObject* parent;
    vec3   localPosition;
    vec3   localScale;
    versor localRotation;
    vec3   lastLocalPosition;
    vec3   lastLocalScale;
    versor lastLocalRotation;
    mat4   localMatrix;     // read-only
    mat4   worldMatrix;     // read-only
    mat4   lastWorldMatrix; // read-only
    bool   worldMatrixChanged;
    union {
        GameObject_Model model;
        GameObject_DirectionalLight directionalLight;
        GameObject_PointLight pointLight;
        GameObject_LightProbe lightProbe;
    };
} GameObject;


typedef struct Scene {
    size_t slots;
    size_t size;
    GameObject* objects;
} Scene;

void InitScene (Scene* scene);
void UpdateScene (Scene* scene);
GameObject* AddObject (Scene* scene, GameObject* parent, GameObjectType type);


typedef struct RenderableMesh {
    mat4 worldMatrix;
    mat4 lastWorldMatrix;
    Material* material;
    Mesh mesh;
} RenderableMesh;

typedef struct RenderableDirectionalLight {
    vec3 position; // normalized
    vec3 color;
} RenderableDirectionalLight;

typedef struct RenderablePointLight {
    vec3 position;
    vec3 color;
} RenderablePointLight;

typedef struct RenderableLightProbe {
    vec3 position;
    vec3 colors [6]; // X+, X-, Y+, Y-, Z+, Z-
} RenderableLightProbe;

static const size_t RenderList_DefaultMeshSlots = 1024;
static const size_t RenderList_DefaultDirectionalLightSlots = 4;
static const size_t RenderList_DefaultPointLightSlots = 128;
static const size_t RenderList_DefaultLightProbeSlots = 64;

typedef struct RenderList {
    size_t meshSlots;
    size_t meshCount;
    RenderableMesh* meshes;
    size_t directionalLightSlots;
    size_t directionalLightCount;
    RenderableDirectionalLight* directionalLights;
    size_t pointLightSlots;
    size_t pointLightCount;
    RenderablePointLight* pointLights;
    size_t lightProbeSlots;
    size_t lightProbeCount;
    RenderableLightProbe* lightProbes;
} RenderList;

void ClearRenderList (RenderList* rl);
void UpdateRenderList (RenderList* rl, Scene* scene);