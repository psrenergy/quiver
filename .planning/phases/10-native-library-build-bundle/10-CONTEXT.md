# Phase 10: Native Library Build & Bundle - Context

**Gathered:** 2026-04-18
**Status:** Ready for planning
**Mode:** Auto-generated (infrastructure phase — discuss skipped)

<domain>
## Phase Boundary

Bundle pre-built native libraries (libquiver + libquiver_c) for Linux x64 and Windows x64 in the JSR package. Update the CI publish workflow to build natives before JSR publish. The loader already has Tier 1 search for libs/{os}-{arch}/.

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — pure infrastructure phase. Use ROADMAP phase goal, success criteria, and codebase conventions to guide decisions.

Key context from user:
- Must bundle binaries so consumers don't need C++ toolchain
- Follow the "most correct JSR way" to do this
- Old npm workflow pattern: build-native job → download artifacts → publish with binaries
- Platform keys: linux-x64 (ubuntu, build/lib/), windows-x64 (windows, build/bin/Release/ or build/bin/)
- loader.ts Tier 1 already searches libs/{os}-{arch}/ relative to source

</decisions>

<code_context>
## Existing Code Insights

Codebase context will be gathered during plan-phase research.

</code_context>

<specifics>
## Specific Ideas

No specific requirements — infrastructure phase. Refer to ROADMAP phase description and success criteria.

</specifics>

<deferred>
## Deferred Ideas

None — discuss phase skipped.

</deferred>
