# AionD9MT

Aion 1.9's Direct3D 9 to Metal renderer for ReRun on Apple Silicon.
This repository owns the renderer source, Wine 11 adapter, Aion patches, and
regression probes. It does not contain the Aion game client or server.

## Source

`upstream/d9mt` is the unmodified source snapshot from
[neo773/d9mt](https://github.com/neo773/d9mt) at
`237e2935e58355d1ee41fda097e1af272d5f62f0`, including its vendored dependencies.
Prebuilt binaries from upstream's unused v2 experiment are excluded from Git.
`tools/D9MT` contains the Aion and Wine adaptations previously developed in
ReRun. `release-manifest.json` records the source pins, patch hashes, compatible
Wine runtime, and archive checksum.

The original upstream README remains at `upstream/d9mt/README.md`.
Its CrossOver installation commands should not be used for ReRun.

## Checks

```sh
python3 scripts/test.py
python3 scripts/verify-release.py dist/v0.1.1
```

## Build

Requires Xcode with the Metal compiler, Rosetta 2, mingw-w64, glslang, bison,
flex, Python 3, and Git. Wine's Unix driver is x86_64 and runs through Rosetta.

```sh
bash scripts/build.sh
```

The build uses an isolated working copy of the upstream snapshot and never
installs into CrossOver or ReRun. Generated components are under `.build/`.
The imported `v0.1.0` binary is the exact previously tested ReRun bundle;
rebuilding may produce different bytes and requires a new checksum and release.

## Release delivery

Keep binary assets in ignored `dist/`, outside Git history. Publish
`dist/v0.1.1/AionD9MT.tar.gz`, `SHA256SUMS`, and `release-manifest.json`
as assets of GitHub Release `v0.1.1` after committing and pushing the source.
The repository must be public for ReRun's unauthenticated downloads.

ReRun pins the versioned asset URL and SHA-256. It downloads on the first Play
with Metal D9MT selected, reuses verified installations offline, and preserves
its separate Wine prefix. Publish the renderer release before shipping that
ReRun build. Never replace a published archive under the same tag; make a new
release and update ReRun's pin instead.

The portable checks cover adapters and patches; visible rendering and real
camera input still require an Aion gameplay check on a fresh ReRun installation.

## Attribution and distribution review

The renderer incorporates neo773/d9mt, its vendored DXVK and SPIRV-Cross code,
DXMT v0.80's winemetal components, and a patched Sikarugir Wine 11 driver.
Preserve the notices supplied with those sources. The imported D9MT snapshot
has no root license file; confirm its distribution terms before publishing the
binary. Do not assign a new blanket license to upstream code. Wine and DXMT
source and notices must accompany their binary distribution as required by
their respective licenses.
