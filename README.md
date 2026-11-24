
<p align="center">
    <img src="./assets/logo.png"/>
</p>

---

# imh.h
Image Hider [header-only library](./imh.h).

### Usage
Idea is similar to [stb header-only libraries](https://github.com/nothings/stb).
To use the library you need to first define the implementation and then include the header.

```
#define IMH_IMPLEMENTATION
#include "imh.h"
```

Library is commented as much as I deemed necessary.
API is not that complex either (only 12 functions, from which only 4 are really useful outside of internals).


# image_hider
Sample [encoder/decoder](./image_hider.c) for PNG images and arbitrary data.

Utilizes [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) and [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) by Sean Barrett and contributors.

To compile with GCC (requires at least C99):
```
gcc -std=c99 image_hider.c -o image_hider
```

To test the decoder, grab the logo PNG from the top of readme and get a zip file from it!
