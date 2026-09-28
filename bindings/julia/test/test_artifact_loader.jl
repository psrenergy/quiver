module TestArtifactLoader

using Artifacts
using Quiver
using Test

@testset "Artifact loader" begin
    artifacts_toml = Artifacts.find_artifacts_toml(joinpath(@__DIR__, "..", "src"))
    if artifacts_toml === nothing
        @test Quiver.C._quiver_artifact_hash === nothing
    else
        @test Quiver.C._quiver_artifact_hash == Artifacts.artifact_hash("quiver", artifacts_toml)
    end
end

end
