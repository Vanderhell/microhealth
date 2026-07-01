# Release Process

This repository does not treat every passing CI run as a release. Use an explicit release process.

## Preconditions

- Working tree is clean.
- Documentation matches the shipped API.
- Changelog entries are under `Unreleased` until the release is intentional.
- All intended local and CI gates for the target release have passed.
- The tag to be created does not already exist.

## Release checklist

1. Review `git status --short`.
2. Review `git tag --list --sort=version:refname`.
3. Review `git describe --tags --always --dirty`.
4. Confirm README, API docs, and changelog match the code being released.
5. Confirm CI covers the required compiler and consumer matrix.
6. Create a dedicated release commit if documentation or metadata changed.
7. Create a new tag only once, intentionally.
8. Publish release notes from `Unreleased` content.

## Rules

- Never move or reuse historical tags.
- Never describe an unreleased state as an already published version.
- Do not publish contradictory version metadata in CMake or docs.
- If no tag exists for a claimed version, keep the changes under `Unreleased`.

## After release

- Start a fresh `Unreleased` section in `CHANGELOG.md`.
- Keep documentation aligned with the released public API.
