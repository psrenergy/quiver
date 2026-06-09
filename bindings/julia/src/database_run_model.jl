function run_model!(db::Database)
    out_exit_code = Ref{Cint}(0)
    check(C.quiver_database_run_model(db.ptr, out_exit_code))
    return Int(out_exit_code[])
end
