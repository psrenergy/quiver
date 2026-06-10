#!/usr/bin/env julia

using Tar
using SHA
using TOML

const S3_BUCKET = get(ENV, "QUIVER_S3_BUCKET", "julia-artifacts")
const S3_PREFIX = get(ENV, "QUIVER_S3_PREFIX", "quiver")

# macOS ships libquiver under its install name (libquiver.0.dylib, SOVERSION 0): dyld
# resolves libquiver_c's @rpath/libquiver.0.dylib dependency filesystem-first, and a second
# libquiver.dylib copy in the same directory could be loaded as a duplicate image. Linux
# instead relies on ld.so SONAME matching of the preloaded libquiver.so, so the artifact
# omits libquiver.so.0.
const PLATFORMS = Dict(
    "linux-x86_64"   => (subdir = "lib", files = ("libquiver.so", "libquiver_c.so"),         tags = Dict("os" => "linux",   "arch" => "x86_64", "libc" => "glibc")),
    "macos-aarch64"  => (subdir = "lib", files = ("libquiver.0.dylib", "libquiver_c.dylib"), tags = Dict("os" => "macos",   "arch" => "aarch64")),
    "windows-x86_64" => (subdir = "bin", files = ("libquiver.dll", "libquiver_c.dll"),       tags = Dict("os" => "windows", "arch" => "x86_64")),
)

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

function stage_platform(tag, srcdir)
    haskey(PLATFORMS, tag) || error("Unknown platform tag: $tag")
    info = PLATFORMS[tag]
    staging = mktempdir()
    libdir = joinpath(staging, info.subdir)
    mkpath(libdir)
    for name in info.files
        src = joinpath(srcdir, name)
        isfile(src) || error("Missing $name in $srcdir for platform $tag")
        dst = joinpath(libdir, name)
        # realpath: in CI srcdir holds real files, but a local run against build/lib would
        # otherwise copy CMake's symlinks as symlinks and tar dangling links.
        cp(realpath(src), dst)
        chmod(dst, 0o755)
    end
    return staging
end

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
