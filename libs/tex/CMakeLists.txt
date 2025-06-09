file (GLOB HEADERS "*.h")
# file (GLOB SOURCES "[^_]*.cpp") # Replaced with explicit list
set(SOURCES
    build_adjacency_graph.cpp
    build_obj_model.cpp
    calculate_data_costs.cpp
    generate_debug_embeddings.cpp
    generate_texture_atlases.cpp
    generate_texture_patches.cpp
    generate_texture_views.cpp
    global_seam_leveling.cpp
    histogram.cpp
    local_seam_leveling.cpp
    material_lib.cpp
    obj_model.cpp
    poisson_blending.cpp
    prepare_mesh.cpp
    projection_cache.cpp
    rectangular_bin.cpp
    seam_leveling.cpp
    texture_atlas.cpp
    texture_patch.cpp
    texture_view.cpp
    timer.cpp
    tri.cpp
    uni_graph.cpp
    view_selection.cpp
)

set(LIBRARY tex)
add_library(${LIBRARY} STATIC ${SOURCES})
set_property(TARGET ${LIBRARY} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
add_dependencies(${LIBRARY} ext_mve ext_rayint ext_eigen ext_mapmap)
target_link_libraries(${LIBRARY} ${TBB_LIBRARIES} -lmve -lmve_util ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${TIFF_LIBRARIES})
install(TARGETS ${LIBRARY} ARCHIVE DESTINATION lib)
