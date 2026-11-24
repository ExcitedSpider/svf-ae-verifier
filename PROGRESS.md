# Progress (UTC)

- 2026-11-24 22:30 — Cloned the pinned upstream object from the local checkout to comply with the GitHub restriction. Started an out-of-tree Release build in the specified LLVM 22 image.
- 2026-11-24 22:31 — Probed the image for an importable `pysvf` and searched the local workspace, image and available caches for binding sources. The image has neither `pysvf` nor pip, and the pinned tree points to a separate GitHub repository. With GitHub access prohibited and no local source to compile, the bindings could not be built against the pin or tested for final-state access in any mode. Switched to the C++ fallback.
