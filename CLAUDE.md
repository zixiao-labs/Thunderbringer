# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Thunderbringer — high-performance libgit2 native binding for Electron using Node-API (v8) + node-addon-api. Statically links libgit2 v1.9.0 (git submodule at `deps/libgit2`).

## Build & Test Commands

```bash
npm run build          # Incremental build (cmake-js compile)
npm run build:debug    # Debug build
npm run clean          # Clean build artifacts
npm test               # Run all Jest tests
npx jest test/status.test.js  # Run a single test file
```

Output lands in `build/Release/thunderbringer.node`.

## Architecture

### Native Layer (`src/`)

Each Git domain (repository, status, index, commit, etc.) has its own `.cc/.h` pair. All modules register in `addon.cc` via `InitXxx()` functions called in dependency order across 6 phases.

**Two patterns for exposing functionality:**

1. **ObjectWrap classes** (Repository, Index) — C++ classes wrapping a libgit2 handle. Constructor takes `Napi::External<git_xxx>`, destructor frees it. Exposed via `DefineClass` with instance methods.

2. **Static-only modules** (Status, Commit, Diff, Patch, Blame, Remote, Merge, etc.) — Namespace of static functions, no persistent handle.

### Async Workers (`src/common/async_worker.h`)

Expensive operations return Promises via `ThunderbringerWorker<ResultType>`. Subclasses implement:
- `DoExecute()` — runs on worker thread, uses `git_check()` which throws `GitException`
- `GetResult(Env, result)` — runs on main thread, converts result to JS value

Complex structures (Status, Diff) use intermediate data structs (`StatusResult`, `DiffResult`) to serialize libgit2 data on the worker thread, then convert to JS objects on the main thread.

### Memory Management (`src/common/guard.h`)

`GitGuard<T, FreeFn>` — move-only RAII template. Type aliases: `GitRepoGuard`, `GitCommitGuard`, `GitTreeGuard`, etc. All temporary libgit2 pointers should use a guard.

### Error Handling (`src/common/error.h`)

- Worker thread: `git_check(code)` throws `GitException` on negative return codes
- Main thread: `git_check_throw(env, code)` throws `Napi::Error`

### JS Layer (`lib/`)

`lib/index.js` re-exports native modules. `lib/errors.js` defines custom error classes. `lib/repository.js` wraps static Repository methods.

### Entry Point (`index.js`)

Tries `node-gyp-build()` prebuilts first, then `build/Release/`, then `build/Debug/`.

### Tests (`test/`)

Jest tests. Temp directory helpers in `test/fixtures/setup.js` (`createTempDir`, `cleanupDir`).

## Critical Rules

- `NODE_API_MODULE` macro in `addon.cc` **must** be at file scope, not inside a namespace — the first arg becomes an identifier prefix
- Link target is `libgit2package` (not `libgit2`, which is an OBJECT library)
- `git_commit_create` expects `const git_commit**` — needs explicit cast from `git_commit**`
- Don't hardcode `arch` in cmake-js config — let it auto-detect
