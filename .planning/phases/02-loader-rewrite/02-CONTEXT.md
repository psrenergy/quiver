# Phase 2: Loader Rewrite - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Rewrite `_loader.py` to discover bundled native libraries from `_libs/` subdirectory (wheel install), with fallback for development mode. Targets Windows x64 and Linux x64 only. CI/CD and publishing are separate phases.

</domain>

<decisions>
## Implementation Decisions

### Dev-mode fallback
- Claude has full discretion on the dev-mode discovery strategy (walk-up, PATH-only, or hybrid)
- No backwards compatibility required — WIP project, breaking changes acceptable
- User always runs Python tests via `test.bat` (which sets up PATH), so dev workflow convenience through the loader is not critical
- Silent on success — no logging when libraries load correctly

### Error experience
- Use plain `RuntimeError` (no custom exception class)
- Single error message covering both wheel and dev scenarios (no context-aware branching)
- Error must name the specific library that failed to load (e.g., "Found libquiver but libquiver_c is missing")
- Error must include the actual paths that were searched (e.g., "Searched: /path/to/_libs/, PATH")
- Expose internal packaging details (mention `_libs/` directory by name) — transparency over abstraction

### Fallback chain
- Bundled `_libs/` discovery first — this is the primary path for installed wheels
- If bundled libs are found but fail to load (corrupt, wrong arch), fail immediately with error — do NOT fall through to dev mode
- If `_libs/` does not exist or is empty, fall through to dev-mode discovery
- Claude has discretion on fallback chain depth (two-step vs three-step)
- Expose a module-level `_load_source` attribute (e.g., `'bundled'` or `'development'`) indicating which strategy succeeded
- Claude has discretion on standalone diagnostic mode (`python -m quiverdb._loader`) — use best practice

### Platform handling
- Strip all macOS/darwin code paths — only Windows and Linux supported
- Assume Python 3.13+ — no version guards for `os.add_dll_directory()`
- Windows: use `os.add_dll_directory()` for the bundled lib directory so `libquiver_c.dll` resolves its `libquiver.dll` dependency
- Linux: Claude has discretion on rpath-only vs explicit `LD_LIBRARY_PATH` — use standard practice for Python native wheels
- Derive library file extensions at runtime (from `sys.platform` or similar), not hardcoded per-platform

### Claude's Discretion
- Dev-mode fallback strategy (walk-up vs PATH-only vs hybrid)
- Fallback chain depth (two-step vs three-step)
- Linux dependency resolution approach (rpath vs explicit)
- Whether to include standalone diagnostic mode
- Internal code structure and helper functions

</decisions>

<specifics>
## Specific Ideas

- Error messages should be maximally diagnostic: name the missing library, show searched paths, mention `_libs/` by name
- The `_load_source` attribute enables tests to verify which loading strategy was used
- Current `_loader.py` is in `bindings/python/src/quiverdb/_loader.py` — rewrite in place

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-loader-rewrite*
*Context gathered: 2026-02-25*
