mutable struct Sandbox
    ptr::Ptr{C.quiver_sandbox}
    # Keeps the Database from being GC'd. Does NOT protect against an explicit `close!` -- the
    # C++ sandbox borrows a raw `Database&`, so a sandbox must not outlive its database.
    db::Database
end

"""
    Sandbox(db::Database)

Lua `print()` writes and flushes an INFO entry through the database logger. `log_path`
selects its file destination, including for in-memory and read-only databases. Without it,
writable databases use `quiver_database.log` and other databases use only the console.
Console output follows the database's `console_level` setting.
"""
function Sandbox(db::Database)
    out_sandbox = Ref{Ptr{C.quiver_sandbox}}(C_NULL)
    check(C.quiver_sandbox_new(db.ptr, out_sandbox))
    sandbox = Sandbox(out_sandbox[], db)
    finalizer(r -> r.ptr != C_NULL && C.quiver_sandbox_free(r.ptr), sandbox)
    return sandbox
end

# `timeout_ms > 0` stops a script still running after that many milliseconds; 0 means no limit.
function run!(sandbox::Sandbox, script::String; timeout_ms::Integer = 0)
    out_result = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_sandbox_run(sandbox.ptr, script, timeout_ms, out_result))
    result = unsafe_string(out_result[])
    C.quiver_database_free_string(out_result[])
    return result
end

function close!(sandbox::Sandbox)
    if sandbox.ptr != C_NULL
        C.quiver_sandbox_free(sandbox.ptr)
        sandbox.ptr = C_NULL
    end
    return nothing
end
