set(ENGINE_ASSET_SRC
    "engine/asset/asset.c"
    "engine/asset/texture.c"
    "engine/asset/texture_manager.c"
    "engine/asset/mesh.c"
    "engine/asset/sprite.c")

set(ENGINE_MATERIAL_SRC
    "engine/material/material.c")

set(ENGINE_RENDERER_SRC
    "engine/renderer/renderer.c")

set(ENGINE_RHI_SRC
    "engine/rhi/rhi.c")

set(ENGINE_NOFILTER_SRC
    "engine/engine.c"
    "engine/engine_context.c"
    "engine/module.c"
    "engine/module.c"
    "engine/config.c")

set(ENGINE_SOURCES
    ${ENGINE_ASSET_SRC}
    ${ENGINE_MATERIAL_SRC}
    ${ENGINE_RENDERER_SRC}
    ${ENGINE_RHI_SRC}
    ${ENGINE_NOFILTER_SRC}
)