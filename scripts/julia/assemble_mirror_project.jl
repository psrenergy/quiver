#!/usr/bin/env julia
#
# Produce the published Quiver.jl `Project.toml` from the canonical
# bindings/julia/Project.toml. Keeps [deps]/[compat] in sync with the monorepo so
# the generated mirror never drifts, while overriding the *registered* package
# identity (name + UUID) and stamping the synced version. The published mirror
# consumes binaries via Artifacts, so any Quiver_jll dependency is stripped.
#
# Usage:
#   julia assemble_mirror_project.jl <canonical_project> <output_project> <version> <uuid>

using TOML

length(ARGS) == 4 || error("Usage: assemble_mirror_project.jl <canonical> <output> <version> <uuid>")
canonical, output, version, uuid = ARGS

project = TOML.parsefile(canonical)
project["name"] = "Quiver"
project["uuid"] = uuid
project["version"] = version

# The mirror is a plain S3-artifact package; it must never depend on Quiver_jll.
haskey(project, "deps") && delete!(project["deps"], "Quiver_jll")
haskey(project, "compat") && delete!(project["compat"], "Quiver_jll")

open(output, "w") do io
    TOML.print(io, project; sorted = true)
end

println(stderr, "Wrote $output (name=Quiver uuid=$uuid version=$version)")
