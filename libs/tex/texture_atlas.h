/*
 * Copyright (C) 2015, Nils Moehrle
 * TU Darmstadt - Graphics, Capture and Massively Parallel Computing
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD 3-Clause license. See the LICENSE.txt file for details.
 */

#ifndef TEX_TEXTUREATLAS_HEADER
#define TEX_TEXTUREATLAS_HEADER

#include "tex/defines.h" // For TEX_NAMESPACE_BEGIN and TEX_NAMESPACE_END

#include <vector>

#include <util/exception.h>
#include <math/vector.h>
#include <mve/mesh.h>
#include <mve/image.h>

#include "tri.h"
#include "texture_patch.h"
#include "rectangular_bin.h"

TEX_NAMESPACE_BEGIN // Ensure it's within the namespace if not already

constexpr unsigned int MAX_ATLAS_TEXTURE_SIZE = 8 * 1024;
constexpr unsigned int PREF_ATLAS_TEXTURE_SIZE = 4 * 1024;
constexpr unsigned int MIN_ATLAS_TEXTURE_SIZE = 256;

/**
  * Class representing a texture atlas.
  */
class TextureAtlas {
    public:
        typedef std::shared_ptr<TextureAtlas> Ptr;

        typedef std::vector<std::size_t> Faces;
        typedef std::vector<std::size_t> TexcoordIds;
        typedef std::vector<math::Vec2f> Texcoords;

    private:
        unsigned int const size;
        unsigned int const padding;
        bool finalized;

        Faces faces;
        Texcoords texcoords;
        TexcoordIds texcoord_ids;

        mve::ByteImage::Ptr image;
        mve::ByteImage::Ptr validity_mask;

        RectangularBin::Ptr bin;

        void apply_edge_padding(void);
        void merge_texcoords(void);

    public:
        TextureAtlas(unsigned int size);

        static TextureAtlas::Ptr create(unsigned int size);

        Faces const & get_faces(void) const;
        TexcoordIds const & get_texcoord_ids(void) const;
        Texcoords const & get_texcoords(void) const;
        mve::ByteImage::ConstPtr get_image(void) const;
        mve::ByteImage::Ptr get_mutable_image(void); // New method

        bool insert(TexturePatch::ConstPtr texture_patch);
        void pre_populate_layout(Faces const& new_faces, TexcoordIds const& new_ids, Texcoords const& new_uvs); // New method

        void finalize(void);
};

inline TextureAtlas::Ptr
TextureAtlas::create(unsigned int size) {
    return Ptr(new TextureAtlas(size));
}

inline TextureAtlas::Faces const &
TextureAtlas::get_faces(void) const {
    return faces;
}

inline TextureAtlas::TexcoordIds const &
TextureAtlas::get_texcoord_ids(void) const {
    return texcoord_ids;
}

inline TextureAtlas::Texcoords const &
TextureAtlas::get_texcoords(void) const {
    return texcoords;
}

inline mve::ByteImage::ConstPtr
TextureAtlas::get_image(void) const {
    if (!finalized) {
        throw util::Exception("Texture atlas not finalized");
    }
    return image;
}

inline mve::ByteImage::Ptr
TextureAtlas::get_mutable_image(void) {
    // Image is allocated in constructor. Here, just return it.
    // Finalized check might be relevant depending on usage.
    return image;
}

// Declaration for pre_populate_layout needs to be added to the class body,
// definition would typically be in the .cpp file or inline if simple.
// For now, leaving as declaration. Implementation will be needed if used.

TEX_NAMESPACE_END // Ensure it's within the namespace

#endif /* TEX_TEXTUREATLAS_HEADER */
