#!/usr/bin/env julia
#
# Generate the Julia `Artifacts.toml` for Quiver from prebuilt native libraries,
# (optionally) upload the per-platform tarballs to S3, and emit the toml.
#
# This is vendored deliberately: it has NO dependency on ArtifactsGenerator.jl and
# uses only Julia stdlibs (Tar, SHA, TOML) plus the `gzip` and `aws` CLIs, which are
# present on the CI runner. It is the single source of truth for how Quiver's binary
# artifacts are packaged and described to the published Quiver.jl mirror.
#
# Usage:
#   julia scripts/julia/generate_artifacts.jl \
#       --version 0.8.0 \
#       --output  /path/to/Artifacts.toml \
#       --platform linux-x86_64   /path/to/linux/libs \
#       --platform windows-x86_64 /path/to/windows/libs \
#       [--no-upload]
#
# Each `--platform <tag> <dir>` pair names a known platform and the directory that
# holds its libquiver / libquiver_c shared libraries. The tarball is laid out so the
# Quiver loader resolves `joinpath(artifact_dir, library_dir())` -> the libraries.
#
# Env overrides: QUIVER_S3_BUCKET (default "julia-artifacts"),
#                QUIVER_S3_PREFIX (default "quiver").

using Tar
using SHA
using TOML

const S3_BUCKET = get(ENV, "QUIVER_S3_BUCKET", "julia-artifacts")
const S3_PREFIX = get(ENV, "QUIVER_S3_PREFIX", "quiver")

# Per-platform packaging rules:
#   subdir -> the directory the loader expects (matches library_dir())
#   ext    -> shared-library extension
#   tags   -> Artifacts.toml platform selectors. `libc` keeps a glibc build from
#             being served to musl hosts; cxxstring_abi is intentionally omitted
#             because the Julia boundary is the extern-"C" libquiver_c.
const PLATFORMS = Dict(
    "linux-x86_64"   => (subdir = "lib", ext = ".so",  tags = Dict("os" => "linux",   "arch" => "x86_64", "libc" => "glibc")),
    "windows-x86_64" => (subdir = "bin", ext = ".dll", tags = Dict("os" => "windows", "arch" => "x86_64")),
)

# libquiver_c depends on libquiver; both ship in the same directory.
const LIB_STEMS = ("libquiver", "libquiver_c")

function parse_args(argv)
    version = nothing
    output = "Artifacts.toml"
    upload = true
    platforms = Pair{String,String}[]
    i = 1
    while i <= length(argv)
        a = argv[i]
        if a == "--version"
            version = argv[i + 1]; i += 2
        elseif a == "--output"
            output = argv[i + 1]; i += 2
        elseif a == "--no-upload"
            upload = false; i += 1
        elseif a == "--platform"
            push!(platforms, String(argv[i + 1]) => String(argv[i + 2])); i += 3
        else
            error("Unknown argument: $a")
        end
    end
    version === nothing && error("--version is required")
    isempty(platforms) && error("at least one --platform is required")
    return (; version, output, upload, platforms)
end

# Stage a platform's libraries under the loader-expected subdir and return the dir.
function stage_platform(tag, srcdir)
    haskey(PLATFORMS, tag) || error("Unknown platform tag: $tag")
    info = PLATFORMS[tag]
    staging = mktempdir()
    libdir = joinpath(staging, info.subdir)
    mkpath(libdir)
    for stem in LIB_STEMS
        name = stem * info.ext
        src = joinpath(srcdir, name)
        isfile(src) || error("Missing $name in $srcdir for platform $tag")
        cp(src, joinpath(libdir, name))
    end
    return staging
end

# Create the gzipped tarball; return (tgz_path, git-tree-sha1, sha256-of-tgz).
# The tree hash is computed from the same clean tar that gets gzipped, so it matches
# what Pkg recomputes after download+extract.
function build_tarball(tag, staging, outdir)
    tar_path = joinpath(outdir, "quiver-$tag.tar")
    Tar.create(staging, tar_path)
    tree_sha1 = Tar.tree_hash(tar_path)
    tgz_path = joinpath(outdir, "quiver-$tag.tgz")
    run(pipeline(`gzip -n -9 -c $tar_path`; stdout = tgz_path))
    rm(tar_path)
    sha256_hex = bytes2hex(open(SHA.sha256, tgz_path))
    return (; tgz_path, tree_sha1, sha256 = sha256_hex)
end

function main(argv)
    args = parse_args(argv)
    outdir = mktempdir()
    entries = Dict{String,Any}[]
    for (tag, srcdir) in args.platforms
        staging = stage_platform(tag, srcdir)
        tb = build_tarball(tag, staging, outdir)
        filename = basename(tb.tgz_path)
        url = "https://$S3_BUCKET.s3.amazonaws.com/$S3_PREFIX/$(args.version)/$filename"
        if args.upload
            # public-read ACL so Pkg can fetch the artifact anonymously
            # (matches ArtifactsGenerator.jl's `x-amz-acl: public-read` on PSRIO uploads).
            run(`aws s3 cp $(tb.tgz_path) s3://$S3_BUCKET/$S3_PREFIX/$(args.version)/$filename --acl public-read`)
        end
        entry = Dict{String,Any}(PLATFORMS[tag].tags)
        entry["git-tree-sha1"] = tb.tree_sha1
        entry["download"] = [Dict("url" => url, "sha256" => tb.sha256)]
        push!(entries, entry)
        println(stderr, "[$tag] git-tree-sha1=$(tb.tree_sha1) sha256=$(tb.sha256)")
        println(stderr, "       url=$url")
    end
    open(args.output, "w") do io
        TOML.print(io, Dict("quiver" => entries))
    end
    println(stderr, "Wrote $(args.output) ($(length(entries)) platform(s))")
end

main(ARGS)
