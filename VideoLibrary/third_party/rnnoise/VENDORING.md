RNNoise vendored from https://gitlab.xiph.org/xiph/rnnoise (master, May 2026).

Upstream layout preserved: `include/rnnoise.h` is the public header, `src/`
holds the implementation and the baked-in default model (`rnn_data.c`).
No upstream source was modified.

License: see `COPYING` (BSD-3).

The DesktopRecorderLibrary build pulls the `.c` files directly into the
VideoLibrary project. RNNoise opts into the built-in model when
`rnnoise_create` is called with `nullptr` for the model argument; no
external model file is needed.
