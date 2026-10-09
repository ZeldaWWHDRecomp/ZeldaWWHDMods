# Maintainer review and publication

## Source review

- Require a readable source-only diff, licence and README for every current mod. Reject compiled files, archives, encoded or minified payloads and all game data. Verify original-art generators and provenance; no extracted maps/models/sounds enter packages or CI artifacts.
- Read hooks and replacements, scopes affecting ordinary actors, original-function delegation, timing, bounds and input/file/HUD service use. Verify replacement conflicts remain explicit errors. Review manifest options, dependencies and each setup permission.
- List every `run_tool` entry and its imported local helper closure. Read the entire setup implementation and capability helper, not just the guard result. Confirm read-only selected inputs, bounded parsing and flat output names confined to mod data. Reject networking, subprocesses, dynamic execution and direct writes. Static checks are not a sandbox and cannot prove arbitrary Python harmless.
- Setup capability exception: only `gc-minimap/tools/safe_io.py` may use filesystem primitives. Its changes are blocked by trusted-base PR policy and need separate maintainer review. Reject symlink escapes and path replacement races; bound input/output sizes. Do not relax the policy just to admit legacy Pillow/numpy/dtk scripts.
- Check generated catalogue metadata against manifest/source metadata and package bytes. Contributors supply neither packages nor hashes. Inspect workflow/policy/SDK pin changes separately.

## Verification

Require catalogue install/setup/enable/restart and disable/remove checks for USA/EU and Metal/Vulkan. Review 30 fps, interpolated 60 fps and true-60 timing, state loads, save slots, cancellation/recovery, simultaneous mods and unchanged disabled frames. Compare panels with the legacy implementation in the same scene. Preserve documented prototype limitations and report missing evidence honestly. CI never receives game fixtures or derived screenshots.

## Hosting and promotion

Set `MOD_PACKAGE_BASE` once to the HTTPS package-download root (default: GitHub release downloads). The build appends immutable `devel-<full-source-commit>` or `main-<full-source-commit>` tags, then package names. Main and devel packages never share a tag. Manual dispatch produces artifacts only, never creates a release, pushes or changes repository contents. The maintainer uploads the reviewed artifact to the matching immutable release and installs its generated `index.json` on the matching branch. Never replace an existing published asset.

Keep `catalogue.json` as authored metadata; `index.json` is generated delivery metadata. For promotion, fast-forward main to the reviewed devel commit, then run the main-channel build/publish step and commit its generated index through the maintainer-controlled process. Sync that publication commit back into devel before the next promotion to retain fast-forward ancestry. Do not resolve divergent published indexes with an unreviewed merge. Since the repo is private, public raw/download URLs will return 404 until the lead makes it public or selects public hosting.

Testers use `WWHD_MOD_CATALOGUE=https://raw.githubusercontent.com/ZeldaWWHDRecomp/ZeldaWWHDMods/devel/index.json`; players retain the main URL. A local index can be selected for offline lifecycle testing; its package records still use absolute URLs and test fixtures must supply downloads without weakening production URL checks.

## GitHub settings (lead action)

Protect `devel` and `main`: require pull requests, CODEOWNER approval and at least one approving review; dismiss stale approvals; require resolved conversations and the `packages` check; require branches up to date; disallow force pushes and deletion. Restrict main updates to the maintainer and disable bypasses where available. Configure Actions fork approval, read-only workflow tokens, no untrusted secrets, and no `pull_request_target`. CODEOWNERS alone does not enable these protections. Policy/bootstrap changes require explicit maintainer review and integration; they intentionally cannot pass ordinary contributor PR policy.
