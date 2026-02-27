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

function run!(runner::LuaRunner, script::String)
    err = C.quiver_lua_runner_run(runner.ptr, script)
    if err != C.QUIVER_OK
        out_error = Ref{Ptr{Cchar}}(C_NULL)
        get_err = C.quiver_lua_runner_get_error(runner.ptr, out_error)
        if get_err == C.QUIVER_OK && out_error[] != C_NULL
            error_msg = unsafe_string(out_error[])
            if !isempty(error_msg)
                throw(DatabaseException(error_msg))
            end
        end
        throw(DatabaseException("Lua script execution failed"))
    end
    return nothing
end

function close!(runner::LuaRunner)
    if runner.ptr != C_NULL
        C.quiver_lua_runner_free(runner.ptr)
        runner.ptr = C_NULL
    end
    return nothing
end
