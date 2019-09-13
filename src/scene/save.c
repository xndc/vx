#include "save.h"
#include "data/model.h"

static const char MAGIC[] = "VXEngine Scene v1.0\n";

// Loads a scene from the given file. Initializes the given scene object.
void LoadScene (Scene* scene, const char* filename) {
    static char buf[128]; // temporary storage used by various parts of this function

    vxCheck(scene != NULL);
    vxLog("Reading into scene 0x%jx from file %s...", scene, filename);

    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        vxLog("Read failed: can't open file! %s", strerror(errno));
        return;
    }
    fseek(f, 0L, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    #define READ(buf, size) do { \
        size_t r = fread(buf, 1, size, f); \
        if (r != size) { \
            vxLog("Read failed: fread %ju bytes, expected %ju (%s)", r, size, strerror(errno)); \
            if (feof(f)) { vxLog("Read failed: end of file"); } \
            goto fail; \
        } \
    } while(0)

    #define SCAN(expected, ...) do { \
        int scanned = fscanf(f, __VA_ARGS__); \
        if (scanned != expected) { \
            vxLog("Read failed: fscanf read %d elements, expected %d", scanned, expected); \
            if (feof(f)) { vxLog("Read failed: end of file"); } \
            goto fail; \
        } \
    } while(0)

    READ(buf, sizeof(MAGIC)-1);
    if (strncmp(buf, MAGIC, sizeof(MAGIC)-1) != 0) {
        vxLog("Read failed: not a valid scene file");
        goto fail;
    }

    int numObjects;
    SCAN(1, "%d objects", &numObjects);

    // Preallocate:
    InitScene(scene);
    for (int iobj = 0; iobj < numObjects; iobj++) {
        AddObject(scene, NULL, GAMEOBJECT_NULL);
    }

    for (int iobj = 0; iobj < numObjects; iobj++) {
        GameObject* obj = &scene->objects[iobj];
        char type;
        int iparent; // -1 for none, index of parent otherwise
        SCAN(12, "\n%c %d pos(%g %g %g) rot(%g %g %g %g) scl(%g %g %g)", &type, &iparent,
            &obj->localPosition[0], &obj->localPosition[1], &obj->localPosition[2],
            &obj->localRotation[0], &obj->localRotation[1], &obj->localRotation[2], &obj->localRotation[3],
            &obj->localScale[0], &obj->localScale[1], &obj->localScale[2]);

        if (iparent >= 0) {
            obj->parent = &scene->objects[iparent];
        }
        
        switch (type) {
            case 'N': {
                obj->type = GAMEOBJECT_NULL;
            } break;

            case 'M': {
                obj->type = GAMEOBJECT_MODEL;
                SCAN(1, " %127s", buf);
                Model* mdl = NULL;
                for (size_t i = 0; i < ModelCount; i++) {
                    if (strncmp(buf, Models[i]->name, sizeof(buf)) == 0) {
                        mdl = Models[i];
                        break;
                    }
                }
                if (mdl == NULL) {
                    vxLog("Read failed: unknown model %s for object %d", buf, iobj);
                    goto fail;
                }
                obj->model.model = mdl;
            } break;

            case 'D': {
                obj->type = GAMEOBJECT_DIRECTIONAL_LIGHT;
                SCAN(3, " color(%g %g %g)",
                    &obj->directionalLight.color[0],
                    &obj->directionalLight.color[1],
                    &obj->directionalLight.color[2]);
            } break;

            case 'P': {
                obj->type = GAMEOBJECT_POINT_LIGHT;
                SCAN(3, " color(%g %g %g)",
                    &obj->pointLight.color[0],
                    &obj->pointLight.color[1],
                    &obj->pointLight.color[2]);
            } break;

            case 'L': {
                obj->type = GAMEOBJECT_LIGHT_PROBE;
                float* xp = obj->lightProbe.colorXp;
                float* xn = obj->lightProbe.colorXn;
                float* yp = obj->lightProbe.colorYp;
                float* yn = obj->lightProbe.colorYn;
                float* zp = obj->lightProbe.colorZp;
                float* zn = obj->lightProbe.colorZn;
                SCAN(3, " xp(%g %g %g)", &xp[0], &xp[1], &xp[2]);
                SCAN(3, " xn(%g %g %g)", &xn[0], &xn[1], &xn[2]);
                SCAN(3, " yp(%g %g %g)", &yp[0], &yp[1], &yp[2]);
                SCAN(3, " yn(%g %g %g)", &yn[0], &yn[1], &yn[2]);
                SCAN(3, " zp(%g %g %g)", &zp[0], &zp[1], &zp[2]);
                SCAN(3, " zn(%g %g %g)", &zn[0], &zn[1], &zn[2]);
            } break;

            default: {
                vxLog("Read failed: object %d has unknown type %c (%u)", iobj, type, type);
                goto fail;
            } break;
        } 

        glm_vec3_copy(obj->localPosition, obj->lastLocalPosition);
        glm_quat_copy(obj->localRotation, obj->lastLocalRotation);
        glm_vec3_copy(obj->localScale,    obj->lastLocalScale);
        obj->needsUpdate = true;
    }

    #undef READ
    #undef SCAN

    fclose(f);
    return;

    fail:
    InitScene(scene);
    fclose(f);
}

// Saves a scene to the given file, overwriting any existing contents.
void SaveScene (Scene* scene, const char* filename) {
    vxCheck(scene != NULL);
    vxLog("Writing scene 0x%jx with %ju objects into file %s...", scene, scene->size, filename);
    FILE* f = fopen(filename, "w");
    if (f == NULL) {
        vxLog("Write failed: can't open file! %s", strerror(errno));
        return;
    }

    fputs(MAGIC, f);
    int numObjects = (int) scene->size;
    fprintf(f, "%d objects", numObjects);

    for (int iobj = 0; iobj < numObjects; iobj++) {
        GameObject* obj = &scene->objects[iobj];
        int iparent = -1;
        if (obj->parent != NULL) {
            iparent = obj->parent - scene->objects; // should result in a correct index, right?
        }
        char type;
        switch (obj->type) {
            case GAMEOBJECT_NULL:               { type = 'N'; } break;
            case GAMEOBJECT_MODEL:              { type = 'M'; } break;
            case GAMEOBJECT_DIRECTIONAL_LIGHT:  { type = 'D'; } break;
            case GAMEOBJECT_POINT_LIGHT:        { type = 'P'; } break;
            case GAMEOBJECT_LIGHT_PROBE:        { type = 'L'; } break;
            default: {
                vxLog("Write warning: skipping object %d (0x%jx) due to unknown type %d", iobj, obj, obj->type);
                continue;
            } break;
        }
        fprintf(f, "\n%c %d pos(%g %g %g) rot(%g %g %g %g) scl(%g %g %g)", type, iparent,
            obj->localPosition[0], obj->localPosition[1], obj->localPosition[2],
            obj->localRotation[0], obj->localRotation[1], obj->localRotation[2], obj->localRotation[3],
            obj->localScale[0], obj->localScale[1], obj->localScale[2]);
        switch (obj->type) {
            case GAMEOBJECT_MODEL: {
                vxCheck(obj->model.model != NULL);
                fprintf(f, " %s", obj->model.model->name);
            } break;
            case GAMEOBJECT_DIRECTIONAL_LIGHT: {
                fprintf(f, " color(%g %g %g)",
                    obj->directionalLight.color[0],
                    obj->directionalLight.color[1],
                    obj->directionalLight.color[2]);
            } break;
            case GAMEOBJECT_POINT_LIGHT: {
                fprintf(f, " color(%g %g %g)",
                    obj->pointLight.color[0],
                    obj->pointLight.color[1],
                    obj->pointLight.color[2]);
            } break;
            case GAMEOBJECT_LIGHT_PROBE: {
                float* xp = obj->lightProbe.colorXp;
                float* xn = obj->lightProbe.colorXn;
                float* yp = obj->lightProbe.colorYp;
                float* yn = obj->lightProbe.colorYn;
                float* zp = obj->lightProbe.colorZp;
                float* zn = obj->lightProbe.colorZn;
                fprintf(f, " xp(%g %g %g)", xp[0], xp[1], xp[2]);
                fprintf(f, " xn(%g %g %g)", xn[0], xn[1], xn[2]);
                fprintf(f, " yp(%g %g %g)", yp[0], yp[1], yp[2]);
                fprintf(f, " yn(%g %g %g)", yn[0], yn[1], yn[2]);
                fprintf(f, " zp(%g %g %g)", zp[0], zp[1], zp[2]);
                fprintf(f, " zn(%g %g %g)", zn[0], zn[1], zn[2]);
            } break;
        }
    }
    fclose(f);
}