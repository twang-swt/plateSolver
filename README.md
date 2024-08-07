PlateSolver
================

A command line program that performs astrometric plate solving offline. Based on astrometry.net

Works on Windows, Mac, Android.

Usage
----------------

This program needs astrometry.net index files. They are hosted at https://astrometry.net/use.html
The index files have `.fits` file extension.
```
dir_of_astrometry_net_index_files/
    index-4119.fits 
    index-4118.fits 
    index-4117.fits
    ...
```

PlateSolver loads all index files in the directory. The availability of index files determine the field size range that can be solved. Larger index files allow images with smaller field size to be solved.

Input image can be .jpg, .fits or other formats.

`--solver_position` specifies the image coordinates ("0,0" is top left) at which to find RA/Dec.

```sh
./PlateSolver \
  --fits "dir_of_astrometry_net_index_files" \
  -i "image_to_solve.jpg" \
  --solver_position 0,0
```

Building
----------------
PlateSolver directly depends on the StellarSolver project (https://github.com/rlancaste/stellarsolver).
Currently a fork a is used: https://github.com/twang-swt/stellarsolver

License
----------------
SPDX-License-Identifier: GPL-3.0-or-later

