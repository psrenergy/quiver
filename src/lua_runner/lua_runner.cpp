#include "quiver/lua_runner.h"

#include "csv/csv_write.h"
#include "lua_runner/internal.h"
#include "quiver/binary/binary_file.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace quiver::lua_internal {

// True while some writer this run handed out for `resolved_path` is alive and unclosed. Two
// writers on one path each open with ios::trunc and write from offset 0, so the second one
// silently discards everything the first buffered -- the same hazard db:open_file's
// process-global write registry (src/binary/binary_file.cpp) already refuses. Reopening a path
// whose previous writer was closed stays legal (it truncates).
bool RunHandles::path_has_open_writer(const std::string& resolved_path) const {
    for (const auto& [path, weak] : open_writers) {
        if (path != resolved_path) {
            continue;
        }
        if (const auto writer = weak.lock(); writer && !writer->is_closed()) {
            return true;
        }
    }
    return false;
}

// Only expired entries go: a closed-but-alive writer stays, and erase_if keeps the survivors'
// order, so the close order at run()'s exit is unchanged. Without the prune, every writer a long
// script ever opened would stay listed until run() returns; with it, only live ones and any dropped
// since the last GC collection do.
void RunHandles::add_writer(
    const std::string& resolved_path,
    const std::shared_ptr<quiver::csv_write::Writer>& writer
) {
    std::erase_if(open_writers, [](const auto& entry) { return entry.second.expired(); });
    open_writers.emplace_back(resolved_path, writer);
}

// Same rule as add_writer.
void RunHandles::add_binary_file(const std::shared_ptr<BinaryFile>& file) {
    std::erase_if(open_binary_files, [](const auto& weak) { return weak.expired(); });
    open_binary_files.push_back(file);
}

// The run-exit flush mechanism, for every writer and binary file handle. A CsvWriter the script
// left reachable -- `w = db:write_csv(...)` without `local`, the Lua default -- is a GC root, so
// collect_garbage() never finalizes it and its buffered rows never reach disk. Closing through
// this registry instead makes the flush independent of reachability, which is what the documented
// guarantee ("the file is complete and re-readable even without w:close()") actually promises.
void RunHandles::close_open_handles() {
    for (const auto& [path, weak] : open_writers) {
        if (const auto writer = weak.lock()) {
            try {
                writer->close("close");
            } catch (const std::exception&) {
                // Runs at run()'s scope exit, including exception unwinding: there is no caller
                // to report a flush failure to. ~Writer swallowed it identically before.
            }
        }
    }
    open_writers.clear();
    for (const auto& weak : open_binary_files) {
        if (const auto file = weak.lock()) {
            try {
                file->close();
            } catch (const std::exception&) {
                // Same as the CSV loop above: nobody to report a flush failure to at scope exit.
            }
        }
    }
    open_binary_files.clear();
}

}  // namespace quiver::lua_internal

namespace quiver {

struct LuaRunner::Impl {
    Database& db;
    // Declared before `lua`: the state, and every closure that captured `handles`, is torn down first.
    lua_internal::RunHandles handles;
    sol::state lua;

    explicit Impl(Database& database) : db(database) {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::coroutine,
            sol::lib::utf8
        );
        // Scripts may not load Lua source from disk, and load() takes text chunks only: Lua does not
        // verify bytecode, so a crafted binary chunk could read and write host memory. The wrapper
        // forces mode "t" whatever the caller passed; `...` forwards env only when one was given,
        // because stock load tells a missing env (global environment) from an explicit nil.
        lua["dofile"] = sol::lua_nil;
        lua["loadfile"] = sol::lua_nil;
        lua.safe_script(R"lua(
            local load = load
            _G.load = function(chunk, chunkname, _, ...) return load(chunk, chunkname, "t", ...) end
        )lua");
        sol::table ns = lua.create_named_table("quiver");
        // The only Database usertype: registering it again would clear every method bound before.
        auto bind = lua.new_usertype<Database>("Database");
        lua_internal::bind_database(bind);
        lua_internal::bind_create(bind);
        lua_internal::bind_read(bind);
        lua_internal::bind_update(bind);
        lua_internal::bind_delete(bind);
        lua_internal::bind_describe(bind);
        lua_internal::bind_metadata(bind);
        lua_internal::bind_query(bind);
        lua_internal::bind_time_series(bind);
        lua_internal::bind_csv_export(bind);
        lua_internal::bind_csv_import(bind);
        lua_internal::bind_csv(lua, bind, handles);
        lua_internal::bind_binary(lua, bind, ns, db, handles);
        lua["db"] = &db;
    }
};

LuaRunner::LuaRunner(Database& db) : impl_(std::make_unique<Impl>(db)) {}

LuaRunner::~LuaRunner() = default;

LuaRunner::LuaRunner(LuaRunner&&) noexcept = default;

LuaRunner& LuaRunner::operator=(LuaRunner&&) noexcept = default;

std::string LuaRunner::run(const std::string& script) {
    // A writer (or any other unique_ptr + sol::no_constructor usertype, e.g. CsvWriter)
    // the script leaves unreachable at run()'s return is never collected on its own -- sol::state
    // is a long-lived member of Impl, so nothing forces a GC cycle between script executions.
    // GcGuard's destructor runs one full collection, unconditionally, on every exit path (normal
    // return, the empty-return, and exception unwinding alike), which synchronously finalizes any
    // such object and therefore flushes/closes its underlying resource (a probe
    // ran this exact one-call-suffices check against this repo's own vendored sol2/Lua build).
    // Declared BEFORE `result`: C++ destroys stack locals in reverse declaration order, so this
    // guard (declared first) is destroyed AFTER `result` (declared second) -- releasing
    // `result`'s Lua stack reference before the collection below runs. Declaring the guard after
    // `result` would collect while a live stack reference still anchors the script's userdata.
    // close_open_handles() runs first and does NOT depend on reachability: it closes CSV writers and
    // binary files. A writer the script assigned to a global (`w = db:write_csv(...)`, the Lua
    // default spelling) is a GC root, so collect_garbage() alone would leave its rows in the ofstream
    // buffer and the file at 0 bytes -- and a db:open_file writer's path in the write registry.
    // The collection still runs afterwards for every other unique_ptr usertype.
    struct GcGuard {
        Impl& impl;
        ~GcGuard() {
            impl.handles.close_open_handles();
            impl.lua.collect_garbage();
        }
    } gc_guard{*impl_};

    // Text only, like load(): a precompiled chunk passed as the script would skip the same check.
    auto& lua = impl_->lua;
    auto result =
        lua.safe_script(script, sol::script_pass_on_error, sol::detail::default_chunk_name(), sol::load_mode::text);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }

    if (result.return_count() == 0) {
        return {};
    }

    return lua_internal::encode_return_json(result.get<sol::object>(0));
}

}  // namespace quiver
