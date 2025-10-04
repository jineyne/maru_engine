#ifndef ENGINE_ASSET_SPRITE_H
#define ENGINE_ASSET_SPRITE_H

#include <stdint.h>
#include "texture.h"
#include "texture_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t sprite_handle_t;
#define SPRITE_HANDLE_INVALID ((sprite_handle_t)0)

typedef struct sprite_desc {
    texture_handle_t texture;
    float width;
    float height;
} sprite_desc_t;

/* System management */
int sprite_system_init(size_t capacity);
void sprite_system_shutdown(void);

/* Sprite operations */
sprite_handle_t sprite_create(const sprite_desc_t *desc);
void sprite_destroy(sprite_handle_t h);

/* Rendering */
struct rhi_cmd;
void sprite_draw(struct rhi_cmd *cmd, sprite_handle_t h, float x, float y);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_ASSET_SPRITE_H */
