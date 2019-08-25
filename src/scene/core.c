#include "core.h"

void UpdateScene (Scene* scene) {
    bool worldMatricesNeedUpdate = false;
    for (size_t i = 0; i < scene->size; i++) {
        GameObject* obj = &scene->objects[i];
        if (!glm_vec3_eqv(obj->localPosition, obj->lastLocalPosition) ||
            !glm_vec3_eqv(obj->localScale,    obj->lastLocalScale) ||
            !glm_vec4_eqv(obj->localRotation, obj->lastLocalRotation))
        {
            glm_translate_make(obj->localMatrix, obj->localPosition);
            glm_quat_rotate(obj->localMatrix, obj->localRotation, obj->localMatrix);
            glm_scale(obj->localMatrix, obj->localScale);
            worldMatricesNeedUpdate = true;
            glm_vec3_copy(obj->localPosition, obj->lastLocalPosition);
            glm_vec3_copy(obj->localScale,    obj->lastLocalScale);
            glm_quat_copy(obj->localRotation, obj->lastLocalRotation);
        }
    }
    if (worldMatricesNeedUpdate) {
        for (size_t i = 0; i < scene->size; i++) {
            GameObject* obj = &scene->objects[i];
            glm_mat4_copy(obj->localMatrix, obj->worldMatrix);
            GameObject* parent = obj->parent;
            while (parent != NULL) {
                // FIXME: correct order?
                glm_mat4_mul(obj->worldMatrix, parent->localMatrix, obj->worldMatrix);
                obj->worldMatrixChanged = true;
                parent = parent->parent;
            }
        }
    } else {
        for (size_t i = 0; i < scene->size; i++) {
            GameObject* obj = &scene->objects[i];
            if (obj->worldMatrixChanged) {
                glm_mat4_copy(obj->worldMatrix, obj->lastWorldMatrix);
                obj->worldMatrixChanged = false;
            }
        }
    }
}

void InitScene (Scene* scene) {
    scene->slots = 2048;
    scene->objects = vxAlloc(scene->slots, GameObject);
}

GameObject* AddObject (Scene* scene, GameObject* parent, GameObjectType type) {
    scene->size++;
    if (scene->size > scene->slots) {
        vxLog("Warning: Scene object limit hit!");
        return NULL;
    }
    GameObject* obj = &scene->objects[scene->size - 1];
    memset(obj, 0, sizeof(GameObject));
    obj->parent = parent;
    obj->type = type;
    glm_vec3_zero(obj->localPosition);
    glm_vec3_zero(obj->lastLocalPosition);
    glm_vec3_one(obj->localScale);
    glm_vec3_one(obj->lastLocalScale);
    glm_quat_identity(obj->localRotation);
    glm_quat_identity(obj->lastLocalRotation);
    glm_mat4_identity(obj->localMatrix);
    glm_mat4_identity(obj->worldMatrix);
    glm_mat4_identity(obj->lastWorldMatrix);
    return obj;
}

static void sAllocRenderList (RenderList* rl) {
    rl->meshes = vxAlloc(rl->meshSlots, RenderableMesh);
    rl->directionalLights = vxAlloc(rl->directionalLightSlots, RenderableDirectionalLight);
    rl->pointLights = vxAlloc(rl->pointLightSlots, RenderablePointLight);
    rl->lightProbes = vxAlloc(rl->lightProbeSlots, RenderableLightProbe);
}

void ClearRenderList (RenderList* rl) {
    // Assume the list is uninitialized if rl->meshSlots is zero:
    if (rl->meshSlots < RenderList_DefaultMeshSlots) {
        rl->meshSlots = RenderList_DefaultMeshSlots;
        rl->directionalLightSlots = RenderList_DefaultDirectionalLightSlots;
        rl->pointLightSlots = RenderList_DefaultPointLightSlots;
        rl->lightProbeSlots = RenderList_DefaultLightProbeSlots;
        sAllocRenderList(rl);
    }
    rl->meshCount = 0;
    rl->directionalLightCount = 0;
    rl->pointLightCount = 0;
    rl->lightProbeCount = 0;
}

#define MAKE_RENDERABLE_ADD_FUNCTION(type, slotsField, countField, objField) \
    static type* sAdd ## type (RenderList* rl) { \
        rl->countField++; \
        if (rl->countField > rl->slotsField) { \
            rl->slotsField = rl->countField * 2; \
            rl->objField = (type*) vxAlignedRealloc(rl->objField, rl->slotsField, sizeof(type), vxAlignOf(type)); \
        } \
        return &rl->objField[rl->countField - 1]; \
    }

MAKE_RENDERABLE_ADD_FUNCTION(RenderableMesh, meshSlots, meshCount, meshes);
MAKE_RENDERABLE_ADD_FUNCTION(RenderableDirectionalLight, directionalLightSlots, directionalLightCount, directionalLights);
MAKE_RENDERABLE_ADD_FUNCTION(RenderablePointLight, pointLightSlots, pointLightCount, pointLights);
MAKE_RENDERABLE_ADD_FUNCTION(RenderableLightProbe, lightProbeSlots, lightProbeCount, lightProbes);

#undef MAKE_RENDERABLE_ADD_FUNCTION

void UpdateRenderList (RenderList* rl, Scene* scene) {
    ClearRenderList(rl);
    for (size_t i = 0; i < scene->size; i++) {
        GameObject* obj = &scene->objects[i];
        switch (obj->type) {
            case GAMEOBJECT_MODEL: {
                Model* mdl = obj->model.model;
                for (size_t imesh = 0; imesh < mdl->meshCount; imesh++) {
                    RenderableMesh* rmesh = sAddRenderableMesh(rl);
                    rmesh->mesh = mdl->meshes[imesh];
                    rmesh->material = mdl->meshMaterials[imesh];
                    // FIXME: correct order?
                    glm_mat4_mul(obj->worldMatrix,     mdl->meshTransforms[imesh], rmesh->worldMatrix);
                    glm_mat4_mul(obj->lastWorldMatrix, mdl->meshTransforms[imesh], rmesh->lastWorldMatrix);
                }
            } break;
            
            case GAMEOBJECT_DIRECTIONAL_LIGHT: {
                RenderableDirectionalLight* rdl = sAddRenderableDirectionalLight(rl);
                glm_vec3_normalize_to(obj->localPosition, rdl->position);
                glm_vec3_copy(obj->directionalLight.color, rdl->color);
            } break;

            case GAMEOBJECT_POINT_LIGHT: {
                RenderablePointLight* rpl = sAddRenderablePointLight(rl);
                glm_vec3_copy(obj->localPosition, rpl->position);
                glm_vec3_copy(obj->pointLight.color, rpl->color);
            } break;

            case GAMEOBJECT_LIGHT_PROBE: {
                RenderableLightProbe* rlp = sAddRenderableLightProbe(rl);
                glm_vec3_copy(obj->localPosition, rlp->position);
                glm_vec3_copy(obj->lightProbe.colorXp, ((vec3*)rlp->colors)[0]);
                glm_vec3_copy(obj->lightProbe.colorXn, ((vec3*)rlp->colors)[1]);
                glm_vec3_copy(obj->lightProbe.colorYp, ((vec3*)rlp->colors)[2]);
                glm_vec3_copy(obj->lightProbe.colorYn, ((vec3*)rlp->colors)[3]);
                glm_vec3_copy(obj->lightProbe.colorZp, ((vec3*)rlp->colors)[4]);
                glm_vec3_copy(obj->lightProbe.colorZn, ((vec3*)rlp->colors)[5]);
            } break;
        }
    }
}