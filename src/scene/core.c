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

GameObject* AddObject (Scene* scene, GameObject* parent, GameObjectType type) {
    scene->size++;
    if (scene->size > scene->slots) {
        scene->slots = scene->size * 2;
        scene->objects = (GameObject*) vxAlignedRealloc(
            scene->objects, scene->slots, sizeof(GameObject), vxAlignOf(GameObject));
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
}

void ClearRenderList (RenderList* rl) {
    // Assume the list is uninitialized if rl->meshSlots is zero:
    if (rl->meshSlots < RenderList_DefaultMeshSlots) {
        rl->meshSlots = RenderList_DefaultMeshSlots;
        rl->directionalLightSlots = RenderList_DefaultDirectionalLightSlots;
        rl->pointLightSlots = RenderList_DefaultPointLightSlots;
        sAllocRenderList(rl);
    }
    rl->meshCount = 0;
    rl->directionalLightCount = 0;
    rl->pointLightCount = 0;
}

static RenderableMesh* sAddRenderableMesh (RenderList* rl) {
    rl->meshCount++;
    if (rl->meshCount > rl->meshSlots) {
        rl->meshSlots = rl->meshCount * 2;
        rl->meshes = (RenderableMesh*) vxAlignedRealloc(
            rl->meshes,
            rl->meshSlots,
            sizeof(RenderableMesh),
            vxAlignOf(RenderableMesh));
    }
    return &rl->meshes[rl->meshCount - 1];
}

static RenderableDirectionalLight* sAddRenderableDirectionalLight (RenderList* rl) {
    rl->directionalLightCount++;
    if (rl->directionalLightCount > rl->directionalLightSlots) {
        rl->directionalLightSlots = rl->directionalLightCount * 2;
        rl->directionalLights = (RenderableDirectionalLight*) vxAlignedRealloc(
            rl->directionalLights,
            rl->directionalLightSlots,
            sizeof(RenderableDirectionalLight),
            vxAlignOf(RenderableDirectionalLight));
    }
    return &rl->directionalLights[rl->directionalLightCount - 1];
}

static RenderablePointLight* sAddRenderablePointLight (RenderList* rl) {
    rl->pointLightCount++;
    if (rl->pointLightCount > rl->pointLightSlots) {
        rl->pointLightSlots = rl->pointLightCount * 2;
        rl->pointLights = (RenderablePointLight*) vxAlignedRealloc(
            rl->pointLights,
            rl->pointLightSlots,
            sizeof(RenderablePointLight),
            vxAlignOf(RenderablePointLight));
    }
    return &rl->pointLights[rl->pointLightCount - 1];
}

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
        }
    }
}