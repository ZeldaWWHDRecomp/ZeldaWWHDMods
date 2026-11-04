# Source provenance

Ported from [BeZide93/ZeldaWWHDRecomp](https://github.com/BeZide93/ZeldaWWHDRecomp/tree/96a8996c66efa650c2848c79e5b1ab56c6f31172/examples/guest-mods/dark-link), commit `96a8996c66efa650c2848c79e5b1ab56c6f31172`, version 0.1.36.

The mod ID, options and data filenames are preserved. The catalogue compiles `src/mod.c` with the SDK pinned by the repository. Existing gameplay regression tests are retained under `tests/`. No runtime patch or binary from the recomp repository is included.

## Validation scope

The migrated entrypoint compiles against the unchanged SDK pin `fab80fcec42a25000a97137e2ee4eb2042b858c7`. USA and EU translation/native compilation and the retained synthetic regression checks were run locally. Synthetic runtime/HUD execution covers USA; EU is translated and compiled. No real game files or device session were used for this migration. Existing gameplay limitations remain; package validation is not an in-game acceptance test.
