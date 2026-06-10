#!/usr/bin/env julia

using Tar
using SHA
using TOML

const S3_BUCKET = get(ENV, "QUIVER_S3_BUCKET", "julia-artifacts")
const S3_PREFIX = get(ENV, "QUIVER_S3_PREFIX", "quiver")

const PLATFORMS = Dict(
    "linux-x86_64"   => (subdir = "lib", ext = ".so",    tags = Dict("os" => "linux",   "arch" => "x86_64", "libc" => "glibc")),
    "macos-x86_64"   => (subdir = "lib", ext = ".dylib", tags = Dict("os" => "macos",   "arch" => "x86_64")),
    "windows-x86_64" => (subdir = "bin", ext = ".dll",   tags = Dict("os" => "windows", "arch" => "x86_64")),    
)

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
        dst = joinpath(libdir, name)
        cp(src, dst)
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
