/*
 * Copyright (C) 2015, Nils Moehrle
 * TU Darmstadt - Graphics, Capture and Massively Parallel Computing
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD 3-Clause license. See the LICENSE.txt file for details.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <tbb/task_scheduler_init.h>
#include <omp.h>

#include <util/timer.h>
#include <util/system.h>
#include <util/file_system.h>
#include <mve/mesh_io_ply.h>

#include "tex/util.h"
#include "tex/timer.h"
#include "tex/debug.h"
#include "tex/texturing.h"
#include "tex/progress_counter.h"
#include "tex/projection_cache.h" // Added for projection cache
#include "mve/image_tools.h"      // For mve::image::crop

#include "arguments.h"

// Placeholder for rasterizer - should be in its own .h/.cpp
namespace tex {
    void rasterize_triangle_placeholder(
        mve::ByteImage::Ptr target_img,
        math::Vec2f const* target_uvs, // Array of 3 Vec2f
        mve::FloatImage::ConstPtr source_patch_img,
        math::Vec2f const* source_uvs) { // Array of 3 Vec2f
        // This is a placeholder. A real implementation is needed.
        // It would rasterize the target triangle and sample from the source.
        // For now, maybe fill the bounding box of the target triangle with a color.
        if (!target_img || !source_patch_img) return;

        // Simplified: find bounding box of target_uvs and fill with a fixed color from source
        float min_x = std::min({target_uvs[0][0], target_uvs[1][0], target_uvs[2][0]});
        float min_y = std::min({target_uvs[0][1], target_uvs[1][1], target_uvs[2][1]});
        float max_x = std::max({target_uvs[0][0], target_uvs[1][0], target_uvs[2][0]});
        float max_y = std::max({target_uvs[0][1], target_uvs[1][1], target_uvs[2][1]});

        int i_min_x = static_cast<int>(std::floor(min_x * target_img->width()));
        int i_min_y = static_cast<int>(std::floor(min_y * target_img->height()));
        int i_max_x = static_cast<int>(std::ceil(max_x * target_img->width()));
        int i_max_y = static_cast<int>(std::ceil(max_y * target_img->height()));

        i_min_x = std::max(0, std::min(target_img->width() -1, i_min_x));
        i_min_y = std::max(0, std::min(target_img->height() -1, i_min_y));
        i_max_x = std::max(0, std::min(target_img->width() -1, i_max_x));
        i_max_y = std::max(0, std::min(target_img->height() -1, i_max_y));

        // Get a color from the center of the source patch
        mve::Vec3f color_f = source_patch_img->at(source_patch_img->width()/2, source_patch_img->height()/2, 0);
        mve::Vec3uc color_uc(color_f[0] * 255.0f, color_f[1] * 255.0f, color_f[2] * 255.0f);

        for (int x = i_min_x; x <= i_max_x; ++x) {
            for (int y = i_min_y; y <= i_max_y; ++y) {
                target_img->at(x,y,0) = color_uc[0];
                target_img->at(x,y,1) = color_uc[1];
                target_img->at(x,y,2) = color_uc[2];
            }
        }
    }
}


int main(int argc, char **argv) {
    util::system::print_build_timestamp(argv[0]);
    util::system::register_segfault_handler();

    Timer timer;
    util::WallTimer wtimer;

    Arguments conf;
    try {
        conf = parse_args(argc, argv);
    } catch (std::invalid_argument & ia) {
        std::cerr << ia.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (!conf.input_projection_cache_file.empty()) {
        std::cout << "Entering UV-preserving re-texturing mode using projection cache." << std::endl;
        tex::TextureViews texture_views; // For new images
        mve::TriangleMesh::Ptr mesh;
        tex::TextureAtlases texture_atlases;

        // Load Projection Cache
        tex::ProjectionCache projection_cache;
        std::cout << "\tLoading projection cache from: " << conf.input_projection_cache_file << std::endl;
        try {
            projection_cache = tex::load_projection_cache(conf.input_projection_cache_file);
        } catch (std::exception &e) {
            std::cerr << "\tError loading projection cache: " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (projection_cache.empty()) {
            std::cerr << "\tProjection cache is empty or invalid." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::cout << "\tProjection cache loaded with " << projection_cache.size() << " entries." << std::endl;

        // Check for required alternative_image_folder
        if (conf.alternative_image_folder.empty()) {
            std::cerr << "Error: --alternative_image_folder is required when using --input_projection_cache." << std::endl;
            std::exit(EXIT_FAILURE);
        }

        // Load Mesh
        std::cout << "\tLoading mesh: " << conf.in_mesh << std::endl;
        try {
            mesh = mve::geom::load_ply_mesh(conf.in_mesh);
        } catch (std::exception& e) {
            std::cerr << "\tCould not load mesh: " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }

        // Prepare tmp_dir (needed for generate_texture_views if undistortion happens)
        std::string const out_dir_retex = util::fs::dirname(conf.out_prefix);
         if (!util::fs::dir_exists(out_dir_retex.c_str())) { // Ensure output dir exists for tmp
            util::fs::mkdir(out_dir_retex.c_str());
        }
        std::string const tmp_dir_retex = util::fs::join_path(out_dir_retex, "tmp_retex");
        if (!util::fs::dir_exists(tmp_dir_retex.c_str())) {
            util::fs::mkdir(tmp_dir_retex.c_str());
        }


        // Load Alternative Images into TextureViews
        // (The check for conf.alternative_image_folder.empty() is now done above)
        std::cout << "\tLoading alternative images from: " << conf.alternative_image_folder << std::endl;
        tex::generate_texture_views(conf.alternative_image_folder, &texture_views, tmp_dir_retex);
        if (texture_views.empty()) {
            std::cerr << "\tError: No texture views generated from alternative image folder." << std::endl;
            std::exit(EXIT_FAILURE);
        }

        // Determine Atlas Properties & Create Atlases
        std::size_t num_atlases = 0;
        if (!projection_cache.empty()) {
            for (tex::CachedFaceTextureInfo const& info : projection_cache) {
                if (info.target_atlas_idx != static_cast<std::size_t>(-1) && info.target_atlas_idx + 1 > num_atlases) {
                    num_atlases = info.target_atlas_idx + 1;
                }
            }
        }
        if (num_atlases == 0 && !projection_cache.empty()) num_atlases = 1;
        else if (num_atlases == 0 && projection_cache.empty()) {
             std::cout << "\tWarning: Projection cache is empty, no atlases to generate." << std::endl;
        }


        // Use PREF_ATLAS_TEXTURE_SIZE from texture_atlas.h (needs to be included in texrecon.cpp)
        // Make sure tex/texture_atlas.h is included for PREF_ATLAS_TEXTURE_SIZE
        unsigned int atlas_size = tex::PREF_ATLAS_TEXTURE_SIZE;
        std::cout << "\tCreating " << num_atlases << " texture atlas(es) with fixed size " << atlas_size << "x" << atlas_size << "." << std::endl;

        std::vector<tex::TextureAtlas::Faces> atlas_faces_collected(num_atlases);
        std::vector<tex::TextureAtlas::Texcoords> atlas_texcoords_collected(num_atlases);

        for (tex::CachedFaceTextureInfo const& info : projection_cache) {
            if (info.target_atlas_idx == static_cast<std::size_t>(-1)) continue; // Skip faces not mapped to an atlas
            atlas_faces_collected[info.target_atlas_idx].push_back(info.face_id);
            for(const auto& uv : info.atlas_texcoords) { // These are already normalized UVs for the face corners
                atlas_texcoords_collected[info.target_atlas_idx].push_back(uv);
            }
        }

        for (std::size_t i = 0; i < num_atlases; ++i) {
            tex::TextureAtlas::Ptr current_atlas = tex::TextureAtlas::create(atlas_size);
            current_atlas->pre_populate_layout(atlas_faces_collected[i], {}, atlas_texcoords_collected[i]);
            texture_atlases.push_back(current_atlas);
        }

        // Reconstruct Atlases with New Textures
        std::map<int, tex::TextureView*> label_to_view_map;
        for (tex::TextureView& tv : texture_views) {
            label_to_view_map[tv.get_label()] = &tv;
        }

        std::cout << "\tReconstructing atlases with new textures..." << std::endl;
        for (tex::CachedFaceTextureInfo const& info : projection_cache) {
            if (info.target_atlas_idx == static_cast<std::size_t>(-1)) continue;

            if (label_to_view_map.find(info.source_view_label) == label_to_view_map.end()) {
                std::cerr << "\tWarning: Source view label " << info.source_view_label << " for face " << info.face_id << " not found in alternative images. Skipping this face." << std::endl;
                continue;
            }
            tex::TextureView* source_view = label_to_view_map.at(info.source_view_label);

            try {
                source_view->load_image();
            } catch (std::exception &e) {
                std::cerr << "\tWarning: Could not load image " << source_view->get_image_path() << " for face " << info.face_id << ". Skipping. Error: " << e.what() << std::endl;
                continue;
            }

            mve::ByteImage::ConstPtr full_source_img = source_view->get_image();
            if(full_source_img == nullptr) {
                 std::cerr << "\tWarning: Image data is null for " << source_view->get_image_path() << " for face " << info.face_id << ". Skipping." << std::endl;
                source_view->release_image();
                continue;
            }

            // Ensure rect is within bounds of the source image
            tex::Rect<int> view_rect = info.source_view_rect;
            if (view_rect.min_x < 0 || view_rect.min_y < 0 ||
                view_rect.max_x >= full_source_img->width() || view_rect.max_y >= full_source_img->height() ||
                view_rect.width() <= 0 || view_rect.height() <= 0) {
                std::cerr << "\tWarning: Invalid source_view_rect for face " << info.face_id << ". Rect: "
                          << view_rect.min_x << "," << view_rect.min_y << " to " << view_rect.max_x << "," << view_rect.max_y
                          << " in image of size " << full_source_img->width() << "x" << full_source_img->height() << ". Skipping." << std::endl;
                source_view->release_image();
                continue;
            }

            mve::FloatImage::Ptr patch_image_data_float;
            try {
                 mve::ByteImage::Ptr patch_image_data_byte = mve::image::crop(full_source_img,
                    view_rect.width(), view_rect.height(), view_rect.min_x, view_rect.min_y, mve::ByteImage::Vec3uc(255,0,255));
                patch_image_data_float = mve::image::byte_to_float_image(patch_image_data_byte);

            } catch (std::exception &e) {
                 std::cerr << "\tWarning: Cropping failed for face " << info.face_id << ". Error: " << e.what() << ". Skipping." << std::endl;
                 source_view->release_image();
                 continue;
            }
            source_view->release_image();

            if (conf.settings.tone_mapping == tex::TONE_MAPPING_GAMMA) { // Assuming TONE_MAPPING_GAMMA is accessible
                mve::image::gamma_correct(patch_image_data_float, 1.0f / 2.2f); // Same as in generate_texture_atlases
            }

            tex::TextureAtlas::Ptr current_atlas = texture_atlases.at(info.target_atlas_idx);
            mve::ByteImage::Ptr target_atlas_image = current_atlas->get_mutable_image();

            // Call placeholder rasterizer
            tex::rasterize_triangle_placeholder(target_atlas_image, info.atlas_texcoords.data(), patch_image_data_float, info.patch_texcoords.data());
        }

        std::cout << "\tFinalizing all atlases..." << std::endl;
        for (tex::TextureAtlas::Ptr atlas : texture_atlases) {
            atlas->finalize(); // This will call merge_texcoords() among other things.
        }

        // Skip Seam Leveling (as decided)
        std::cout << "\tSeam leveling skipped in UV-preserving re-texturing mode." << std::endl;

        // Build and Save Model
        std::cout << "\tBuilding and saving model..." << std::endl;
        tex::Model model;
        tex::build_model(mesh, texture_atlases, &model); // This should now work with pre-populated layouts
        tex::Model::save(model, conf.out_prefix);
        std::cout << "\tModel saved to " << conf.out_prefix << ".*" << std::endl;

        // Cleanup tmp_dir_retex
        util::fs::rmdir(tmp_dir_retex.c_str()); // Basic cleanup, might need to remove files first if any were left by generate_texture_views

        std::cout << "UV-preserving re-texturing finished." << std::endl;
        std::exit(EXIT_SUCCESS);
    }

    // Original main execution path if not in re-texturing mode
    std::string const out_dir = util::fs::dirname(conf.out_prefix);

    if (!util::fs::dir_exists(out_dir.c_str())) {
        std::cerr << "Destination directory does not exist!" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::string const tmp_dir = util::fs::join_path(out_dir, "tmp");
    if (!util::fs::dir_exists(tmp_dir.c_str())) {
        util::fs::mkdir(tmp_dir.c_str());
    } else {
        std::cerr
            << "Temporary directory \"tmp\" exists within the destination directory.\n"
            << "Cannot continue since this directory would be delete in the end.\n"
            << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Set the number of threads to use.
    tbb::task_scheduler_init schedule(conf.num_threads > 0 ? conf.num_threads : tbb::task_scheduler_init::automatic);
    if (conf.num_threads > 0) {
        omp_set_dynamic(0);
        omp_set_num_threads(conf.num_threads);
    }

    std::cout << "Load and prepare mesh: " << std::endl;
    mve::TriangleMesh::Ptr mesh;
    try {
        mesh = mve::geom::load_ply_mesh(conf.in_mesh);
    } catch (std::exception& e) {
        std::cerr << "\tCould not load mesh: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    mve::MeshInfo mesh_info(mesh);
    tex::prepare_mesh(&mesh_info, mesh);

    std::cout << "Generating texture views: " << std::endl;
    tex::TextureViews texture_views;
    std::string effective_image_source_path = conf.in_scene;
    if (!conf.alternative_image_folder.empty()) {
        effective_image_source_path = conf.alternative_image_folder;
        std::cout << "\tUsing alternative image folder: " << effective_image_source_path << std::endl;
    }
    tex::generate_texture_views(effective_image_source_path, &texture_views, tmp_dir);

    write_string_to_file(conf.out_prefix + ".conf", conf.to_string());
    timer.measure("Loading");

    std::size_t const num_faces = mesh->get_faces().size() / 3;

    std::cout << "Building adjacency graph: " << std::endl;
    tex::Graph graph(num_faces);
    tex::build_adjacency_graph(mesh, mesh_info, &graph);

    if (conf.labeling_file.empty()) {
        std::cout << "View selection:" << std::endl;
        util::WallTimer rwtimer;

        tex::DataCosts data_costs(num_faces, texture_views.size());
        if (conf.data_cost_file.empty()) {
            tex::calculate_data_costs(mesh, &texture_views, conf.settings, &data_costs);

            if (conf.write_intermediate_results) {
                std::cout << "\tWriting data cost file... " << std::flush;
                tex::DataCosts::save_to_file(data_costs, conf.out_prefix + "_data_costs.spt");
                std::cout << "done." << std::endl;
            }
        } else {
            std::cout << "\tLoading data cost file... " << std::flush;
            try {
                tex::DataCosts::load_from_file(conf.data_cost_file, &data_costs);
            } catch (util::FileException e) {
                std::cout << "failed!" << std::endl;
                std::cerr << e.what() << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::cout << "done." << std::endl;
        }
        timer.measure("Calculating data costs");

        try {
            tex::view_selection(data_costs, &graph, conf.settings);
        } catch (std::runtime_error& e) {
            std::cerr << "\tOptimization failed: " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        timer.measure("Running MRF optimization");
        std::cout << "\tTook: " << rwtimer.get_elapsed_sec() << "s" << std::endl;

        /* Write labeling to file. */
        if (conf.write_intermediate_results) {
            std::vector<std::size_t> labeling(graph.num_nodes());
            for (std::size_t i = 0; i < graph.num_nodes(); ++i) {
                labeling[i] = graph.get_label(i);
            }
            vector_to_file(conf.out_prefix + "_labeling.vec", labeling);
        }
    } else {
        std::cout << "Loading labeling from file... " << std::flush;

        /* Load labeling from file. */
        std::vector<std::size_t> labeling = vector_from_file<std::size_t>(conf.labeling_file);
        if (labeling.size() != graph.num_nodes()) {
            std::cerr << "Wrong labeling file for this mesh/scene combination... aborting!" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        /* Transfer labeling to graph. */
        for (std::size_t i = 0; i < labeling.size(); ++i) {
            const std::size_t label = labeling[i];
            if (label > texture_views.size()){
                std::cerr << "Wrong labeling file for this mesh/scene combination... aborting!" << std::endl;
                std::exit(EXIT_FAILURE);
            }
            graph.set_label(i, label);
        }

        std::cout << "done." << std::endl;
    }

    tex::TextureAtlases texture_atlases;
    {
        /* Create texture patches and adjust them. */
        tex::TexturePatches texture_patches;
        tex::VertexProjectionInfos vertex_projection_infos;
        tex::ProjectionCache projection_cache; // Instantiate projection cache
        std::cout << "Generating texture patches:" << std::endl;
        tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views,
            conf.settings, &vertex_projection_infos, &texture_patches, &projection_cache);

        if (conf.settings.global_seam_leveling) {
            std::cout << "Running global seam leveling:" << std::endl;
            tex::global_seam_leveling(graph, mesh, mesh_info, vertex_projection_infos, &texture_patches);
            timer.measure("Running global seam leveling");
        } else {
            ProgressCounter texture_patch_counter("Calculating validity masks for texture patches", texture_patches.size());
            #pragma omp parallel for schedule(dynamic)
            for (std::size_t i = 0; i < texture_patches.size(); ++i) {
                texture_patch_counter.progress<SIMPLE>();
                TexturePatch::Ptr texture_patch = texture_patches[i];
                std::vector<math::Vec3f> patch_adjust_values(texture_patch->get_faces().size() * 3, math::Vec3f(0.0f));
                texture_patch->adjust_colors(patch_adjust_values);
                texture_patch_counter.inc();
            }
            timer.measure("Calculating texture patch validity masks");
        }

        if (conf.settings.local_seam_leveling) {
            std::cout << "Running local seam leveling:" << std::endl;
            tex::local_seam_leveling(graph, mesh, vertex_projection_infos, &texture_patches);
        }
        timer.measure("Running local seam leveling");

        /* Generate texture atlases. */
        std::cout << "Generating texture atlases:" << std::endl;
        tex::generate_texture_atlases(mesh, &texture_patches, conf.settings, &texture_atlases, &projection_cache);
    }

    /* Create and write out obj model. */
    {
        std::cout << "Building objmodel:" << std::endl;
        tex::Model model;
        tex::build_model(mesh, texture_atlases, &model);
        timer.measure("Building OBJ model");

        std::cout << "\tSaving model... " << std::flush;
        tex::Model::save(model, conf.out_prefix);
        std::cout << "done." << std::endl;
        timer.measure("Saving");
    }

    std::cout << "Whole texturing procedure took: " << wtimer.get_elapsed_sec() << "s" << std::endl;
    timer.measure("Total");
    if (conf.write_timings) {
        timer.write_to_file(conf.out_prefix + "_timings.csv");
    }

    // Save projection cache if requested
    if (!conf.output_projection_cache_file.empty()) {
        std::cout << "Saving projection cache to: " << conf.output_projection_cache_file << std::endl;
        try {
            tex::save_projection_cache(conf.output_projection_cache_file, projection_cache);
            std::cout << "\tProjection cache saved." << std::endl;
        } catch (std::exception &e) {
            std::cerr << "\tError saving projection cache: " << e.what() << std::endl;
        }
    }

    if (conf.write_view_selection_model) {
        texture_atlases.clear();
        std::cout << "Generating debug texture patches:" << std::endl;
        {
            tex::TexturePatches texture_patches;
            generate_debug_embeddings(&texture_views);
            tex::VertexProjectionInfos vertex_projection_infos; // Will only be written
            // Note: Projection cache not passed for debug model generation
            tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views,
                conf.settings, &vertex_projection_infos, &texture_patches, nullptr);
            tex::generate_texture_atlases(mesh, &texture_patches, conf.settings, &texture_atlases, nullptr);
        }

        std::cout << "Building debug objmodel:" << std::endl;
        {
            tex::Model model;
            tex::build_model(mesh, texture_atlases, &model);
            std::cout << "\tSaving model... " << std::flush;
            tex::Model::save(model, conf.out_prefix + "_view_selection");
            std::cout << "done." << std::endl;
        }
    }

    /* Remove temporary files. */
    for (util::fs::File const & file : util::fs::Directory(tmp_dir)) {
        util::fs::unlink(util::fs::join_path(file.path, file.name).c_str());
    }
    util::fs::rmdir(tmp_dir.c_str());

    return EXIT_SUCCESS;
}
