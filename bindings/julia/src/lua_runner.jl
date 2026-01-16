struct LuaRunner
    ptr::Ptr{C.psr_lua_runner}
    db::Database  # Keep reference to prevent GC
end

function LuaRunner(db::Database)
    ptr = C.psr_lua_runner_new(db.ptr)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create LuaRunner"))
    end
    return LuaRunner(ptr, db)
end

function run!(runner::LuaRunner, script::String)
    err = C.psr_lua_runner_run(runner.ptr, script)
    if err != C.PSR_OK
        error_ptr = C.psr_lua_runner_get_error(runner.ptr)
        if error_ptr != C_NULL
            error_msg = unsafe_string(error_ptr)
            throw(DatabaseException("Lua error: $error_msg"))
        else
            throw(DatabaseException("Lua script execution failed"))
        end
    end
    return nothing
end

function close!(runner::LuaRunner)
    C.psr_lua_runner_free(runner.ptr)
    return nothing
end
