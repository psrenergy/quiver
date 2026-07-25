mutable struct LuaRunner
    ptr::Ptr{C.quiver_lua_runner}
    db::Database  # Keep reference to prevent GC
end

function LuaRunner(db::Database)
    out_runner = Ref{Ptr{C.quiver_lua_runner}}(C_NULL)
    check(C.quiver_lua_runner_new(db.ptr, out_runner))
    runner = LuaRunner(out_runner[], db)
    finalizer(r -> r.ptr != C_NULL && C.quiver_lua_runner_free(r.ptr), runner)
    return runner
end

"""
    run!(runner::LuaRunner, script::String)

Execute a Lua script against the database.

Returns the script's return value encoded as JSON, or `""` if it returned nothing.

To execute a script without keeping its writes, wrap the call in [`dry_run`](@ref).
"""
function run!(runner::LuaRunner, script::String)
    out_result = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_lua_runner_run(runner.ptr, script, out_result))
    result = unsafe_string(out_result[])
    C.quiver_lua_runner_free_string(out_result[])
    return result
end

function close!(runner::LuaRunner)
    if runner.ptr != C_NULL
        C.quiver_lua_runner_free(runner.ptr)
        runner.ptr = C_NULL
    end
    return nothing
end
