function tests_path()
    # Monorepo: SQL schemas live in the repo-root tests/ dir (shared across all bindings).
    # Generated Quiver.jl mirror: tests/schemas/ is vendored under this test/ dir, so when
    # the repo-root tests/ is absent fall back to @__DIR__. One file works in both repos.
    monorepo_tests = joinpath(@__DIR__, "..", "..", "..", "tests")
    return isdir(joinpath(monorepo_tests, "schemas")) ? monorepo_tests : @__DIR__
end
