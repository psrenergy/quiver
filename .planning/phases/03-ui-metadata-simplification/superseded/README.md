# Superseded — the deleted "Structured Attribute Metadata" phase

These four files are the executed history of Phase 3's **previous** incarnation, kept because they
document code that shipped and is being reverted — not because they describe current work.

Plans 03-01 and 03-02 landed commits `1c1bcca`…`810e7b3`: a public `UIMetadata` type, a 64-byte
`quiver_ui_metadata_t`, three C++ getters, the C API vocabulary pair and a Python decoder. The
scope correction of 2026-09-20 (see `.planning/ROADMAP.md` §Coverage Notes) retired META-01…06 and
made `describe` the sidecar's only consumer, so the re-scoped Phase 3 deletes all of it.

Plans 03-03…03-07 were never started and were removed outright.

They live in this subdirectory rather than the phase root so GSD's plan discovery does not count
them as the current phase's work — the re-scoped phase has no plans yet.
