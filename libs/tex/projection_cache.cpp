#include "projection_cache.h"
#include <fstream>
#include <stdexcept> // For std::runtime_error

namespace tex {

void save_projection_cache(std::string const& filepath, ProjectionCache const& cache) {
    std::ofstream outfile(filepath, std::ios::binary);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }

    std::size_t num_entries = cache.size();
    outfile.write(reinterpret_cast<const char*>(&num_entries), sizeof(num_entries));

    for (const auto& entry : cache) {
        outfile.write(reinterpret_cast<const char*>(&entry.face_id), sizeof(entry.face_id));
        outfile.write(reinterpret_cast<const char*>(&entry.source_view_label), sizeof(entry.source_view_label));
        outfile.write(reinterpret_cast<const char*>(&entry.source_view_rect.min_x), sizeof(entry.source_view_rect.min_x));
        outfile.write(reinterpret_cast<const char*>(&entry.source_view_rect.min_y), sizeof(entry.source_view_rect.min_y));
        outfile.write(reinterpret_cast<const char*>(&entry.source_view_rect.max_x), sizeof(entry.source_view_rect.max_x));
        outfile.write(reinterpret_cast<const char*>(&entry.source_view_rect.max_y), sizeof(entry.source_view_rect.max_y));

        std::size_t patch_texcoords_size = entry.patch_texcoords.size();
        outfile.write(reinterpret_cast<const char*>(&patch_texcoords_size), sizeof(patch_texcoords_size));
        for (const auto& coord : entry.patch_texcoords) {
            outfile.write(reinterpret_cast<const char*>(&coord[0]), sizeof(float));
            outfile.write(reinterpret_cast<const char*>(&coord[1]), sizeof(float));
        }

        outfile.write(reinterpret_cast<const char*>(&entry.target_atlas_idx), sizeof(entry.target_atlas_idx));

        std::size_t atlas_texcoords_size = entry.atlas_texcoords.size();
        outfile.write(reinterpret_cast<const char*>(&atlas_texcoords_size), sizeof(atlas_texcoords_size));
        for (const auto& coord : entry.atlas_texcoords) {
            outfile.write(reinterpret_cast<const char*>(&coord[0]), sizeof(float));
            outfile.write(reinterpret_cast<const char*>(&coord[1]), sizeof(float));
        }

        if (!outfile) {
            throw std::runtime_error("Error writing to file: " + filepath);
        }
    }
    outfile.close();
}

ProjectionCache load_projection_cache(std::string const& filepath) {
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filepath);
    }

    std::size_t num_entries = 0;
    infile.read(reinterpret_cast<char*>(&num_entries), sizeof(num_entries));
    if (!infile) {
        throw std::runtime_error("Error reading number of entries from file: " + filepath);
    }

    ProjectionCache cache;
    cache.reserve(num_entries);

    for (std::size_t i = 0; i < num_entries; ++i) {
        CachedFaceTextureInfo entry;
        infile.read(reinterpret_cast<char*>(&entry.face_id), sizeof(entry.face_id));
        infile.read(reinterpret_cast<char*>(&entry.source_view_label), sizeof(entry.source_view_label));
        infile.read(reinterpret_cast<char*>(&entry.source_view_rect.min_x), sizeof(entry.source_view_rect.min_x));
        infile.read(reinterpret_cast<char*>(&entry.source_view_rect.min_y), sizeof(entry.source_view_rect.min_y));
        infile.read(reinterpret_cast<char*>(&entry.source_view_rect.max_x), sizeof(entry.source_view_rect.max_x));
        infile.read(reinterpret_cast<char*>(&entry.source_view_rect.max_y), sizeof(entry.source_view_rect.max_y));

        std::size_t patch_texcoords_size = 0;
        infile.read(reinterpret_cast<char*>(&patch_texcoords_size), sizeof(patch_texcoords_size));
        if (!infile || patch_texcoords_size != 3) { // Basic validation
            throw std::runtime_error("Error reading patch_texcoords_size or invalid size from file: " + filepath);
        }
        entry.patch_texcoords.resize(patch_texcoords_size);
        for (std::size_t j = 0; j < patch_texcoords_size; ++j) {
            infile.read(reinterpret_cast<char*>(&entry.patch_texcoords[j][0]), sizeof(float));
            infile.read(reinterpret_cast<char*>(&entry.patch_texcoords[j][1]), sizeof(float));
        }

        infile.read(reinterpret_cast<char*>(&entry.target_atlas_idx), sizeof(entry.target_atlas_idx));

        std::size_t atlas_texcoords_size = 0;
        infile.read(reinterpret_cast<char*>(&atlas_texcoords_size), sizeof(atlas_texcoords_size));
         if (!infile || atlas_texcoords_size != 3) { // Basic validation
            throw std::runtime_error("Error reading atlas_texcoords_size or invalid size from file: " + filepath);
        }
        entry.atlas_texcoords.resize(atlas_texcoords_size);
        for (std::size_t j = 0; j < atlas_texcoords_size; ++j) {
            infile.read(reinterpret_cast<char*>(&entry.atlas_texcoords[j][0]), sizeof(float));
            infile.read(reinterpret_cast<char*>(&entry.atlas_texcoords[j][1]), sizeof(float));
        }

        if (!infile) {
            throw std::runtime_error("Error reading entry data from file: " + filepath);
        }
        cache.push_back(entry);
    }

    if (infile.peek() != EOF && !infile.eof()) {
         // This check is not perfectly reliable for all file read errors but can catch trailing data.
        throw std::runtime_error("Extra data found at the end of file: " + filepath);
    }

    infile.close();
    return cache;
}

} // namespace tex
