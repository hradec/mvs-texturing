/*
 * Copyright (C) 2015, Nils Moehrle
 * TU Darmstadt - Graphics, Capture and Massively Parallel Computing
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD 3-Clause license. See the LICENSE.txt file for details.
 */

#include <set>
#include <list>
#include <iostream>
#include <fstream>

#include <util/timer.h>
#include <mve/image_tools.h>

#include "defines.h"
#include "settings.h"
#include "histogram.h"
#include "texture_patch.h"
#include "texture_atlas.h"

// Defines moved to texture_atlas.h as constexpr
// #define MAX_TEXTURE_SIZE (8 * 1024)
// #define PREF_TEXTURE_SIZE (4 * 1024)
// #define MIN_TEXTURE_SIZE (256)

TEX_NAMESPACE_BEGIN

/**
  * Heuristic to calculate an appropriate texture atlas size.
  * @warning asserts that no texture patch exceeds the dimensions
  * of the maximal possible texture atlas size.
  */
unsigned int
calculate_texture_size(std::list<TexturePatch::ConstPtr> const & texture_patches) {
    unsigned int size = MAX_ATLAS_TEXTURE_SIZE; // Use new constexpr

    while (true) {
        unsigned int total_area = 0;
        unsigned int max_width = 0;
        unsigned int max_height = 0;
        unsigned int padding = size >> 7;

        for (TexturePatch::ConstPtr texture_patch : texture_patches) {
            unsigned int width = texture_patch->get_width() + 2 * padding;
            unsigned int height = texture_patch->get_height() + 2 * padding;

            max_width = std::max(max_width, width);
            max_height = std::max(max_height, height);

            unsigned int area = width * height;
            unsigned int waste = area - texture_patch->get_size();

            /* Only consider patches where the information dominates padding. */
            if (static_cast<double>(waste) / texture_patch->get_size() > 1.0) {
                /* Since the patches are sorted by size we can assume that only
                 * few further patches will contribute to the size and break. */
                break;
            }

            total_area += area;
        }

        assert(max_width < MAX_ATLAS_TEXTURE_SIZE); // Use new constexpr
        assert(max_height < MAX_ATLAS_TEXTURE_SIZE); // Use new constexpr
        if (size > PREF_ATLAS_TEXTURE_SIZE && // Use new constexpr
            max_width < PREF_ATLAS_TEXTURE_SIZE && // Use new constexpr
            max_height < PREF_ATLAS_TEXTURE_SIZE && // Use new constexpr
            total_area / (PREF_ATLAS_TEXTURE_SIZE * PREF_ATLAS_TEXTURE_SIZE) < 8) { // Use new constexpr
            size = PREF_ATLAS_TEXTURE_SIZE; // Use new constexpr
            continue;
        }

        if (size <= MIN_ATLAS_TEXTURE_SIZE) { // Use new constexpr
            return MIN_ATLAS_TEXTURE_SIZE; // Use new constexpr
        }

        if (max_height < size / 2 && max_width < size / 2 &&
            static_cast<double>(total_area) / (size * size) < 0.2) {
            size = size / 2;
            continue;
        }

        return size;
    }
}

bool comp(TexturePatch::ConstPtr first, TexturePatch::ConstPtr second) {
    return first->get_size() > second->get_size();
}

void
generate_texture_atlases(mve::TriangleMesh::ConstPtr mesh, // Added mesh
    std::vector<TexturePatch::Ptr> * orig_texture_patches,
    Settings const & settings, std::vector<TextureAtlas::Ptr> * texture_atlases,
    ProjectionCache * io_projection_cache) { // Added io_projection_cache

    std::list<TexturePatch::ConstPtr> texture_patches;
    while (!orig_texture_patches->empty()) {
        TexturePatch::Ptr texture_patch = orig_texture_patches->back();
        orig_texture_patches->pop_back();

        if (settings.tone_mapping != TONE_MAPPING_NONE) {
            mve::image::gamma_correct(texture_patch->get_image(), 1.0f / 2.2f);
        }

        texture_patches.push_back(texture_patch);
    }

    std::cout << "\tSorting texture patches... " << std::flush;
    /* Improve the bin-packing algorithm efficiency by sorting texture patches
     * in descending order of size. */
    texture_patches.sort(comp);
    std::cout << "done." << std::endl;

    std::size_t const total_num_patches = texture_patches.size();
    std::size_t remaining_patches = texture_patches.size();
    std::ofstream tty("/dev/tty", std::ios_base::out);

    #pragma omp parallel
    {
    #pragma omp single
    {

    while (!texture_patches.empty()) {
        unsigned int texture_size = calculate_texture_size(texture_patches);

        texture_atlases->push_back(TextureAtlas::create(texture_size));
        TextureAtlas::Ptr texture_atlas = texture_atlases->back();

        /* Try to insert each of the texture patches into the texture atlas. */
        std::list<TexturePatch::ConstPtr>::iterator it = texture_patches.begin();
        for (; it != texture_patches.end();) {
            std::size_t done_patches = total_num_patches - remaining_patches;
            int precent = static_cast<float>(done_patches)
                / total_num_patches * 100.0f;
            if (total_num_patches > 100
                && done_patches % (total_num_patches / 100) == 0) {

                tty << "\r\tWorking on atlas " << texture_atlases->size() << " "
                 << precent << "%... " << std::flush;
            }

            if (texture_atlas->insert(*it)) {
                it = texture_patches.erase(it);
                remaining_patches -= 1;
            } else {
                ++it;
            }
        }

        #pragma omp task
        texture_atlas->finalize();
    }

    std::cout << "\r\tWorking on atlas " << texture_atlases->size()
        << " 100%... done." << std::endl;
    util::WallTimer timer;
    std::cout << "\tFinalizing texture atlases... " << std::flush;
    #pragma omp taskwait
    std::cout << "done. (Took: " << timer.get_elapsed_sec() << "s)" << std::endl;

    /* End of single region */
    }
    /* End of parallel region. */
    }

    // Update projection cache with atlas information
    if (io_projection_cache != nullptr && mesh != nullptr) {
        mve::TriangleMesh::FaceList const& mesh_all_faces = mesh->get_faces(); // Original mesh faces
        // mve::TriangleMesh::VertexList const& mesh_vertices = mesh->get_vertices(); // Original mesh vertices (not directly needed with current map approach)

        for (std::size_t atlas_idx = 0; atlas_idx < texture_atlases->size(); ++atlas_idx) {
            TextureAtlas::ConstPtr atlas = (*texture_atlases)[atlas_idx];

            mve::TriangleMesh::FaceList const& atlas_mesh_face_ids = atlas->get_faces();
            tex::TextureAtlas::TexcoordList const& atlas_uvs = atlas->get_texcoords();
            tex::TextureAtlas::TexcoordIdList const& atlas_vertex_ids_for_uvs = atlas->get_texcoord_ids();

            std::map<std::size_t, math::Vec2f> vertex_to_uv_map;
            for(size_t i=0; i < atlas_vertex_ids_for_uvs.size(); ++i) {
                vertex_to_uv_map[atlas_vertex_ids_for_uvs[i]] = atlas_uvs[i];
            }

            for (std::size_t i = 0; i < atlas_mesh_face_ids.size(); ++i) {
                std::size_t current_mesh_face_id = atlas_mesh_face_ids[i];

                auto info_iter = std::find_if(io_projection_cache->begin(), io_projection_cache->end(),
                                                 [current_mesh_face_id](CachedFaceTextureInfo const& cfti){ return cfti.face_id == current_mesh_face_id; });

                if (info_iter != io_projection_cache->end()) {
                    info_iter->target_atlas_idx = atlas_idx;

                    std::size_t v0_id = mesh_all_faces[current_mesh_face_id * 3 + 0];
                    std::size_t v1_id = mesh_all_faces[current_mesh_face_id * 3 + 1];
                    std::size_t v2_id = mesh_all_faces[current_mesh_face_id * 3 + 2];

                    if (info_iter->atlas_texcoords.size() != 3) info_iter->atlas_texcoords.resize(3);

                    auto uv0_it = vertex_to_uv_map.find(v0_id);
                    auto uv1_it = vertex_to_uv_map.find(v1_id);
                    auto uv2_it = vertex_to_uv_map.find(v2_id);

                    if (uv0_it != vertex_to_uv_map.end() &&
                        uv1_it != vertex_to_uv_map.end() &&
                        uv2_it != vertex_to_uv_map.end()) {
                        info_iter->atlas_texcoords[0] = uv0_it->second;
                        info_iter->atlas_texcoords[1] = uv1_it->second;
                        info_iter->atlas_texcoords[2] = uv2_it->second;
                    } else {
                        // Optionally log a warning here if UVs are not found for a vertex of a face in the atlas
                        // For now, they will remain as initialized (likely 0,0 from CachedFaceTextureInfo constructor or previous values)
                    }
                }
            }
        }
    }
}

TEX_NAMESPACE_END
