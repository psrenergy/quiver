function begin_transaction!(db::Database)
    check(C.quiver_database_begin_transaction(db.ptr))
    return nothing
end

function commit!(db::Database)
    check(C.quiver_database_commit(db.ptr))
    return nothing
end

function rollback!(db::Database)
    check(C.quiver_database_rollback(db.ptr))
    return nothing
end

function in_transaction(db::Database)
    out_active = Ref{Bool}(false)
    check(C.quiver_database_in_transaction(db.ptr, out_active))
    return out_active[]
end

function transaction(fn, db::Database)
    begin_transaction!(db)
    try
        result = fn(db)
        commit!(db)
        return result
    catch
        try
            rollback!(db)
        catch
        end
        rethrow()
    end
end
