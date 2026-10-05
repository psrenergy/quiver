mutable struct Sandbox
    ptr::Ptr{C.quiver_sandbox}
    # Keeps the Database from being GC'd. Does NOT protect against an explicit `close!` -- the
    # C++ sandbox borrows a raw `Database&`, so a sandbox must not outlive its database.
    db::Database
end

function Sandbox(db::Database)
    out_sandbox = Ref{Ptr{C.quiver_sandbox}}(C_NULL)
    check(C.quiver_sandbox_new(db.ptr, out_sandbox))
    sandbox = Sandbox(out_sandbox[], db)
    finalizer(r -> r.ptr != C_NULL && C.quiver_sandbox_free(r.ptr), sandbox)
    return sandbox
end

"""
    run!(sandbox::Sandbox, script::String)

Execute a Lua script against the database.

Returns the script's return value encoded as JSON, or `""` if it returned nothing.

To execute a script without keeping its writes, wrap the call in [`dry_run`](@ref).
"""
function run!(sandbox::Sandbox, script::String)
    out_result = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_sandbox_run(sandbox.ptr, script, out_result))
    result = unsafe_string(out_result[])
    C.quiver_sandbox_free_string(out_result[])
    return result
end

function close!(sandbox::Sandbox)
    if sandbox.ptr != C_NULL
        C.quiver_sandbox_free(sandbox.ptr)
        sandbox.ptr = C_NULL
    end
    return nothing
end
