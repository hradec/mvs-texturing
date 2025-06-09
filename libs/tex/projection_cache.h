#ifndef TEX_PROJECTION_CACHE_HEADER
#define TEX_PROJECTION_CACHE_HEADER

#include <string>
#include <vector>
#include "math/vector.h" // For math::Vec2f

// Forward declare or include Rect if it's simple enough
namespace tex { // Or appropriate namespace
    template <typename T>
    struct Rect {
        T min_x, min_y, max_x, max_y; // max_x and max_y are inclusive upper bounds
        // Basic constructor
        Rect() : min_x(0), min_y(0), max_x(0), max_y(0) {}
        Rect(T min_x, T min_y, T max_x, T max_y) : min_x(min_x), min_y(min_y), max_x(max_x), max_y(max_y) {}

        T width() const { return max_x - min_x + 1; }
        T height() const { return max_y - min_y + 1; }
    };

    struct CachedFaceTextureInfo {
        std::size_t face_id;
        int source_view_label; // Corresponds to TextureView label
        Rect<int> source_view_rect;
        std::vector<math::Vec2f> patch_texcoords; // 3 UVs

        std::size_t target_atlas_idx;
        std::vector<math::Vec2f> atlas_texcoords; // 3 UVs

        // Basic constructor
        CachedFaceTextureInfo() : face_id(0), source_view_label(-1), target_atlas_idx(0) {
            patch_texcoords.resize(3);
            atlas_texcoords.resize(3);
        }
    };

    typedef std::vector<CachedFaceTextureInfo> ProjectionCache;

    // Function declarations for serialization/deserialization
    void save_projection_cache(std::string const& filepath, ProjectionCache const& cache);
    ProjectionCache load_projection_cache(std::string const& filepath);
} // namespace tex

#endif // TEX_PROJECTION_CACHE_HEADER
