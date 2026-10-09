# Contributing mods

Read [Creating a mod](docs/creating-a-mod.md) for the SDK v2 source layout, build and submission workflow.

Submit source-only pull requests into `devel`. `main` is the published catalogue and only the maintainer promotes tested changes there. Do not submit packages, compiled files, archives, encoded blobs, extracted game material, saves or keys. Include a licence, readable guest sources, manifest and README for each mod. Original artwork must be reproducible from readable committed generator sources; generated art belongs only in build output.

`catalogue.json` contains reviewed source metadata, without download URLs, hashes or sizes. CI derives packages and their download records from those sources and the pinned public SDK in `sdk.json`. Never edit generated `index.json` in a contribution. The guard rejects PR changes to generated metadata, SDK pins, all root `tools/` CI/policy entrypoints, CI-executed minimap tests, workflows and the trusted setup IO helper; maintainers integrate policy changes separately after review.

Setup tools receive the strictest review. Use Python's permitted standard-library parsers and the reviewed `gc-minimap/tools/safe_io.py` capability helper. Import `safe_io` (an alias is allowed) and call only `safe_io.arguments()` without replacement arguments to obtain the manager-provided context. Do not import or access its filesystem modules, constructor, private fields or implementation helpers. No networking, subprocesses, dynamic execution or direct filesystem writes. Input is read-only; output uses flat filenames in the mod's data folder. Static checks and human review reduce risk; they do not constitute an operating-system sandbox. A guest mod is executable code too: review every hook, replacement and service call.

PR CI uses `pull_request`, read-only repository permission, no secrets and no publishing. It compiles guest code directly with clang/lld; submitted Makefiles and setup tools are never executed. Keep original-art generator changes visible for review before trusted builds execute them.

The maintainer reviews the source and test evidence, including both regions/renderers, 30/60 fps, state/save behaviour and disabled-mod identity. See [the maintainer checklist](docs/review-checklist.md).

Promotion is prepared before main moves: build main-channel packages from clean tested source S, verify hosted artifacts, then create metadata-only publish commit P descended from S. Fast-forward main directly to P, so it never exposes a devel index; then fast-forward devel to P. Immutable package tags identify S, while P records generated hashes/URLs. See the maintainer checklist for the complete sequence.
