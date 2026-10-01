# AionD9MT development

Staging, unstaging, committing, and pushing are allowed within the user's
authorized task scope. The Git restriction in sibling `the-one` applies only
to that repository and does not apply here.
Source lives under `upstream/d9mt`; Aion/Wine adaptations live in `tools/D9MT`.
Keep upstream revisions pinned. Do not check binaries or build caches into Git.
Run `python3 scripts/test.py` after changes. Validate release assets with
`python3 scripts/verify-release.py dist/v0.1.0` before publication.
