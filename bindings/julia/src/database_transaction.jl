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
    out_active = Ref{Cint}(0)
    check(C.quiver_database_in_transaction(db.ptr, out_active))
    return out_active[] != 0
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

function begin_dry_run!(db::Database)
    check(C.quiver_database_begin_dry_run(db.ptr))
    return nothing
end

function end_dry_run!(db::Database)
    check(C.quiver_database_end_dry_run(db.ptr))
    return nothing
end

function in_dry_run(db::Database)
    out_active = Ref{Cint}(0)
    check(C.quiver_database_in_dry_run(db.ptr, out_active))
    return out_active[] != 0
end

"""
    dry_run(fn, db::Database)

Run `fn(db)` inside a transaction that is always rolled back, and return its result.

While the dry run is active, `begin_transaction!`/`commit!`/`rollback!` are absorbed (no-ops), so
code that manages its own transactions composes instead of erroring on a nested `BEGIN`. A nested
rollback is therefore not partial -- everything is undone when the dry run ends.
"""
function dry_run(fn, db::Database)
    begin_dry_run!(db)
    result = try
        fn(db)
    catch
        # Best-effort only while an exception is already in flight, mirroring `transaction`.
        try
            end_dry_run!(db)
        catch
        end
        rethrow()
    end
    # On the success path the rollback is the wrapper's whole promise -- let a failure surface.
    end_dry_run!(db)
    return result
end
