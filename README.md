MVS-Texturing
--------------------------------------------------------------------------------

Welcome to our project that textures 3D reconstructions from images.
This project focuses on 3D reconstructions generated using structure from
motion and multi-view stereo techniques, however, it is not limited to this
setting.

The algorithm was published in Sept. 2014 on the
*European Conference on Computer Vision*. Please refer to our project website
(http://www.gcc.tu-darmstadt.de/home/proj/texrecon/)
for the paper and further information.

*Please be aware that while the interface of the `texrecon` application is
relatively stable the interface of the `tex` library is currently subject to
frequent changes.*


Dependencies
--------------------------------------------------------------------------------

The code and the build system have the following prerequisites:

- cmake (>= 3.1)
- git
- make
- gcc (>= 5.0.0) or a compatible compiler
- libpng, libjpg, libtiff, libtbb


Furthermore the build system automatically downloads and compiles the following
dependencies (so there is nothing you need to do here):

- rayint
    https://github.com/nmoehrle/rayint
- Eigen
    http://eigen.tuxfamily.org
- Multi-View Environment
    http://www.gcc.tu-darmstadt.de/home/proj/mve
- mapMAP
    http://www.gcc.tu-darmstadt.de/home/proj/mapmap


Compilation ![Build Status](https://travis-ci.org/nmoehrle/mvs-texturing.svg)
--------------------------------------------------------------------------------

1.  `git clone https://github.com/nmoehrle/mvs-texturing.git`
2.  `cd mvs-texturing`
3.  `mkdir build && cd build && cmake ..`
4.  `make` (or `make -j` for parallel compilation)

If something goes wrong during compilation you should check the output of the
cmake step. CMake checks all dependencies and reports if anything is missing.

If you think that there is some problem with the build process on our side
please tell us.

If you are trying to compile this under windows (which should be possible but
we haven't checked it) and you feel like we should make minor fixes to support
this better, you can also tell us.


Execution
--------------------------------------------------------------------------------

As input our algorithm requires a triangulated 3D model and images that are
registered against this model. One way to obtain this is to:
*   import images, infer camera parameters and reconstruct depth maps
    using the [Multi-View Environment]
    (http://www.gcc.tu-darmstadt.de/home/proj/mve/),
    and
*   fuse these depth maps into a combined 3D model using the
    [Floating Scale Surface Reconstruction]
    (http://www.gcc.tu-darmstadt.de/home/proj/fssr/)
    algorithm.

A quick guide on how to use these applications can be found on our project [website](http://www.gcc.tu-darmstadt.de/home/proj/texrecon/).

By starting the application without any parameters and you will get a
description of the expected file formats and optional parameters.


UV-Preserving Re-Texturing (Experimental)
--------------------------------------------------------------------------------

This feature allows you to re-texture a mesh using a previously generated UV layout and seam configuration, applying new images or modified processing settings while preserving the texture coordinates of the original texturing run. This is useful if you want to, for example, apply different lighting conditions (by providing new images) or try different seam leveling settings without re-calculating the entire UV parameterization.

The process involves two stages:

1.  **Cache Generation**: During an initial texturing run, you save a "projection cache". This cache stores information about which parts of which source images were used for each face, and how these parts are mapped to the texture atlases.
    To generate the cache, run `texrecon` as usual, but add the `--output_projection_cache <cache_filepath.pcache>` option:
    ```bash
    ./apps/texrecon/texrecon <path_to_scene> <path_to_mesh.ply> <output_prefix_stage1> --output_projection_cache <path_to_your_cache.pcache> [other_options]
    ```
    This will produce the standard textured model (`<output_prefix_stage1>.obj`, etc.) and the projection cache file (`<path_to_your_cache.pcache>`).

2.  **Re-Texturing from Cache**: To re-texture using the generated cache, you provide the *same input mesh* and the cache file, along with a *new set of images* (if desired) and a *different output prefix*.
    ```bash
    ./apps/texrecon/texrecon <dummy_scene_not_used> <path_to_mesh.ply> <output_prefix_stage2> --input_projection_cache <path_to_your_cache.pcache> --alternative_image_folder <path_to_new_images_folder> [other_options_for_postprocessing]
    ```
    *   `<dummy_scene_not_used>`: The first positional argument (input scene) is not strictly used when re-texturing from a cache but is still required by the argument parser. You can provide the original scene path or a placeholder.
    *   `<path_to_mesh.ply>`: **Must be the same mesh file used in Stage 1.**
    *   `<output_prefix_stage2>`: Use a different output prefix to avoid overwriting your original textured model.
    *   `--input_projection_cache <path_to_your_cache.pcache>`: Specifies the cache file to use.
    *   `--alternative_image_folder <path_to_new_images_folder>`: Specifies the folder containing the new set of images to apply. This is **required** for re-texturing.
    *   `[other_options_for_postprocessing]`: Options like `--tone_mapping` can be changed. Options affecting UV generation or view selection (e.g., `--data_term`, `--outlier_removal` for view selection) will be ignored as this information is taken from the cache.

**Important Considerations**:
*   The input mesh (`IN_MESH`) provided in both stages must be identical.
*   The `--alternative_image_folder` is mandatory for the re-texturing stage.
*   The output prefix should be different for the second stage to avoid overwriting results.

**Current Limitations**:
*   **Rasterizer**: The current implementation uses a placeholder for transferring new textures onto the existing UV layout. This placeholder fills the bounding box of each face's UV chart in the atlas with a single color sampled from the new texture. A proper triangle rasterizer/warper is needed for visually correct results.
*   **Seam Leveling**: Global and local seam leveling are currently skipped in the UV-preserving re-texturing mode.
*   **Atlas Size**: The re-texturing process currently assumes a fixed default atlas size for the new atlases. If the original texturing run used a different atlas size (e.g., due to very large or very small texture patches), this might lead to suboptimal results or errors. Future improvements might include storing atlas dimensions in the cache.


Troubleshooting
--------------------------------------------------------------------------------

When you encounter errors or unexpected behavior please make sure to switch
the build type to debug e.g. `cmake -DCMAKE_BUILD_TYPE=DEBUG ..`, recompile
and rerun the application. Because of the computational complexity the default
build type is RELWITHDEBINFO which enables optimization but also ignores
assertions. However, these assertions could give valuable insight in failure cases.


License, Patents and Citing
--------------------------------------------------------------------------------
Our software is licensed under the BSD 3-Clause license, for more details see
the LICENSE.txt file.

If you use our texturing code for research purposes, please cite our paper:
```
@inproceedings{Waechter2014Texturing,
  title    = {Let There Be Color! --- {L}arge-Scale Texturing of {3D} Reconstructions},
  author   = {Waechter, Michael and Moehrle, Nils and Goesele, Michael},
  booktitle= {Proceedings of the European Conference on Computer Vision},
  year     = {2014},
  publisher= {Springer},
}
```

Contact
--------------------------------------------------------------------------------
If you have trouble compiling or using this software, if you found a bug or if
you have an important feature request, please use the issue tracker of github:
https://github.com/nmoehrle/mvs-texturing

For further questions you may contact us at
mvs-texturing(at)gris.informatik.tu-darmstadt.de
