#include "quiver/sandbox.h"

#include "csv/csv_write.h"
#include "database_impl.h"
#include "quiver/binary/binary_file.h"
#include "quiver/database.h"
#include "sandbox/internal.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace quiver::lua_internal {

constexpr int kTimeLimitCheckEvery = 10000;

// run()'s time limit, read by the Lua count hook, the prelude's wrappers and SQLite's progress handler.
// limit_ms == 0 means no limit: between runs, and in a run without one.
struct TimeLimit {
    // Set by run() before start(): the Database the Sandbox borrows can be move-assigned between runs.
    sqlite3* connection = nullptr;
    int64_t limit_ms = 0;
    std::chrono::steady_clock::time_point deadline;
    bool timed_out = false;
    std::string message;

    // True, latched in timed_out, once the deadline has passed.
    bool exceeded();
    void start(lua_State* L, int64_t timeout_ms);
    void stop(lua_State* L);
};

namespace {

// A name as the filesystems quiver ships on (NTFS, APFS) match it, because the part of a path that
// does not exist yet keeps the script's spelling: case-folded, and spelled the way Win32 opens it --
// a name ends at its first ':' ("x::$DATA" is x's own data, "x::$INDEX_ALLOCATION" makes directory
// x) and loses trailing dots and spaces ("x." and "x " open x). Elsewhere that only refuses a few
// names that would not have collided.
std::string fold(std::string name) {
    name.resize(std::min(name.find(':'), name.size()));
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
        name.pop_back();
    }
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

// Its address keys the Sandbox's TimeLimit in the Lua registry.
const char time_limit_key = 0;

// The Lua half of the time limit: the count hook, and the check the prelude's wrappers run (see the
// prelude in Impl). lua_error, never a C++ throw: this runs inside the Lua VM.
int raise_if_past_deadline(lua_State* L) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, &time_limit_key);
    auto* limit = static_cast<TimeLimit*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (limit->exceeded()) {
        lua_pushstring(L, limit->message.c_str());
        return lua_error(L);
    }
    return 0;
}

void time_limit_hook(lua_State* L, lua_Debug* /*ar*/) {
    raise_if_past_deadline(L);
}

// The SQLite half: a runaway query (a recursive CTE through query_*) fails with SQLITE_INTERRUPT. It
// interrupts only while a read is running. Never a write, because SQLite answers an interrupted write by
// rolling back the whole transaction, which may be the host's; query_* is read-only, so every statement
// a script writes itself is still covered. Never a halted statement either: SQLite calls the handler
// once more after OP_Halt, when the finished write is no longer busy, and would report it interrupted.
int interrupt_if_past_deadline(void* data) {
    auto& limit = *static_cast<TimeLimit*>(data);
    if (!limit.exceeded()) {
        return 0;
    }
    bool reading = false;
    for (
        auto* stmt = sqlite3_next_stmt(limit.connection, nullptr); stmt != nullptr;
        stmt = sqlite3_next_stmt(limit.connection, stmt)
    ) {
        if (sqlite3_stmt_busy(stmt) == 0) {
            continue;
        }
        if (sqlite3_stmt_readonly(stmt) == 0) {
            return 0;
        }
        reading = true;
    }
    return reading ? 1 : 0;
}

}  // namespace

bool TimeLimit::exceeded() {
    if (limit_ms == 0) {
        return false;
    }
    if (!timed_out) {
        timed_out = std::chrono::steady_clock::now() >= deadline;
    }
    return timed_out;
}

// ponytail: a soft limit, checked every kTimeLimitCheckEvery Lua instructions and SQLite VM steps. A long
// read query is interrupted, but work outside both runs to completion before the limit is seen: an
// expression save over a large .qvr, an XLSX load, a typed write, export_csv's file write, a Lua string
// pattern call (string.find/match/gsub) backtracking over a long input. Lua runs __gc finalizers, and the
// __close of a coroutine the limit killed, with hooks off, so a loop inside one is not stopped. A
// coroutine created by an earlier run() without a limit carries no hook; memory stays uncapped. A hard
// limit means running the sandbox in a process the host can kill.
void TimeLimit::start(lua_State* L, int64_t timeout_ms) {
    timed_out = false;
    limit_ms = timeout_ms;
    if (limit_ms == 0) {
        return;
    }
    using namespace std::chrono;
    const auto now = steady_clock::now();
    // Saturates, so a huge limit (a host's INT64_MAX for "forever") cannot overflow the deadline.
    const auto room = duration_cast<milliseconds>(steady_clock::time_point::max() - now).count();
    deadline = limit_ms < room ? now + milliseconds(limit_ms) : steady_clock::time_point::max();
    message = "Cannot run: script exceeded its time limit of " + std::to_string(limit_ms) + " ms";
    // Coroutines copy the hook when created (lua_newthread), so a coroutine the script makes is limited too.
    lua_sethook(L, &time_limit_hook, LUA_MASKCOUNT, kTimeLimitCheckEvery);
    sqlite3_progress_handler(connection, kTimeLimitCheckEvery, &interrupt_if_past_deadline, this);
}

// Idempotent. Keeps timed_out for run()'s error. A coroutine created during the run keeps its hook,
// which does nothing once limit_ms is 0.
void TimeLimit::stop(lua_State* L) {
    if (limit_ms == 0) {
        return;
    }
    limit_ms = 0;
    lua_sethook(L, nullptr, 0, 0);
    sqlite3_progress_handler(connection, 0, nullptr, nullptr);
}

// Every Lua writer opens its files with truncate, so containment alone is not enough: the database
// file and its journals sit inside the sandbox root, and a rollback cannot restore a truncated one.
std::string RunHandles::resolve_write_path(
    const Database& db,
    const std::string& operation,
    const std::string& path,
    std::initializer_list<std::string_view> written
) const {
    auto resolved = resolve_sandbox_path(db, operation, path);
    if (read_only) {
        throw std::runtime_error("Cannot " + operation + ": database is open read-only, so scripts cannot write files");
    }

    const std::filesystem::path candidate(resolved);
    for (const auto& [directory, base] : database_entries) {
        const auto relative = candidate.lexically_relative(directory);
        if (relative.empty() || relative.begin()->string() == "..") {
            continue;
        }
        // The first component is what the write creates in the database's directory: the file
        // itself, or a directory export_csv creates on the way to a deeper one. A directory named
        // like the journal would stop SQLite from ever opening its journal again.
        const auto first = relative.begin()->string();
        const bool is_file = std::next(relative.begin()) == relative.end();
        for (const auto suffix : written) {
            const auto name = fold(is_file ? first + std::string(suffix) : first);
            if (name == base || name == base + "-journal" || name == base + "-wal" || name == base + "-shm") {
                throw std::runtime_error(
                    "Cannot " + operation + ": path '" + path + "' is the database file or one of its journals"
                );
            }
        }
    }
    return resolved;
}

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

struct Sandbox::Impl {
    Database& db;
    // Declared before `lua`: the state, and every closure that captured `handles`, is torn down first.
    lua_internal::RunHandles handles;
    // Before `lua` too: a coroutine's hook can still run during lua_close's finalizers.
    lua_internal::TimeLimit time_limit;
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
        lua_pushlightuserdata(lua.lua_state(), &time_limit);
        lua_rawsetp(lua.lua_state(), LUA_REGISTRYINDEX, &lua_internal::time_limit_key);
        // Scripts may not load Lua source from disk, and load() takes text chunks only: Lua does not
        // verify bytecode, so a crafted binary chunk could read and write host memory. The wrapper
        // forces mode "t" whatever the caller passed; `...` forwards env only when one was given,
        // because stock load tells a missing env (global environment) from an explicit nil.
        // Every Lua call that catches an error re-raises once the time limit has passed, so a script
        // cannot swallow the timeout: pcall, xpcall, coroutine.resume/close, and load, whose reader
        // function runs in a protected call. An xpcall handler checks first, because Lua runs it with
        // hooks off when the hook raised. A resume checks before it starts too: every coroutine has its
        // own hook count, so one that finishes inside it never fires the hook, and nested ones would
        // multiply the work done past the deadline. Otherwise they return exactly what the stock ones do.
        lua["dofile"] = sol::lua_nil;
        lua["loadfile"] = sol::lua_nil;
        sol::protected_function prelude = lua.load(R"lua(
            local load, pcall, xpcall, type, raise_if_past_deadline = load, pcall, xpcall, type, ...
            local resume, close, wrap = coroutine.resume, coroutine.close, coroutine.wrap
            local function checked(ok, ...)
                if not ok then raise_if_past_deadline() end
                return ok, ...
            end
            local function checked_load(f, ...)
                if f == nil then raise_if_past_deadline() end
                return f, ...
            end
            _G.load = function(chunk, chunkname, _, ...) return checked_load(load(chunk, chunkname, "t", ...)) end
            _G.pcall = function(...) return checked(pcall(...)) end
            _G.xpcall = function(f, h, ...)
                if type(h) == "function" then
                    local handler = h
                    h = function(e) raise_if_past_deadline() return handler(e) end
                end
                return checked(xpcall(f, h, ...))
            end
            coroutine.resume = function(...) raise_if_past_deadline() return checked(resume(...)) end
            coroutine.close = function(...) raise_if_past_deadline() return checked(close(...)) end
            coroutine.wrap = function(f)
                local w = wrap(f)
                return function(...) raise_if_past_deadline() return w(...) end
            end
        )lua");
        prelude(&lua_internal::raise_if_past_deadline);
        sol::table ns = lua.create_named_table("quiver");
        // The only Database usertype: registering it again would clear every method bound before.
        auto bind = lua.new_usertype<Database>("Database");
        lua_internal::bind_database(bind, handles);
        lua_internal::bind_create(bind);
        lua_internal::bind_read(bind);
        lua_internal::bind_update(bind);
        lua_internal::bind_delete(bind);
        lua_internal::bind_describe(bind);
        lua_internal::bind_metadata(bind);
        lua_internal::bind_query(bind);
        lua_internal::bind_time_series(bind);
        lua_internal::bind_csv_export(bind, handles);
        lua_internal::bind_csv_import(bind);
        lua_internal::bind_csv(lua, bind, handles);
        lua_internal::bind_json(bind);
        lua_internal::bind_xlsx(bind);
        auto binary_file_type = lua_internal::bind_binary(lua, bind, ns, handles);
        lua_internal::bind_expression(lua, ns, binary_file_type, db, handles);
        lua["db"] = &db;
    }
};

Sandbox::Sandbox(Database& db) : impl_(std::make_unique<Impl>(db)) {
    namespace fs = std::filesystem;
    auto& handles = impl_->handles;
    handles.read_only = db.impl_->read_only;
    // SQLite's absolute name for the file, fixed at open; db.path() may be relative to a working
    // directory that has since changed. Empty for an in-memory database.
    const char* file = sqlite3_db_filename(db.impl_->db, "main");
    if (file != nullptr && *file != '\0') {
        std::error_code ignored;
        const fs::path named(file);
        auto resolved = fs::weakly_canonical(named, ignored);
        for (const auto& entry : {named, resolved.empty() ? named : resolved}) {
            auto directory = fs::weakly_canonical(entry.parent_path(), ignored);
            handles.database_entries.emplace_back(
                directory.empty() ? entry.parent_path() : directory,
                lua_internal::fold(entry.filename().string())
            );
        }
    }
}

Sandbox::~Sandbox() = default;

Sandbox::Sandbox(Sandbox&&) noexcept = default;

Sandbox& Sandbox::operator=(Sandbox&&) noexcept = default;

std::string Sandbox::run(const std::string& script, int64_t timeout_ms) {
    if (timeout_ms < 0) {
        throw std::runtime_error("Cannot run: timeout_ms must not be negative, got " + std::to_string(timeout_ms));
    }
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
            // Back to "every scope is the host's" for the finalizers lua_close runs later.
            impl.handles.host_transaction = true;
            impl.handles.host_dry_run = true;
        }
    } gc_guard{*impl_};
    // Declared after gc_guard, so destroyed before it: the time limit comes off before the collection,
    // whose __gc finalizers are Lua code that must not be interrupted. The normal path takes it off
    // right after the script; this covers an exception out of safe_script.
    struct TimeLimitGuard {
        Impl& impl;
        ~TimeLimitGuard() {
            impl.time_limit.stop(impl.lua.lua_state());
        }
    } time_limit_guard{*impl_};

    impl_->handles.host_dry_run = impl_->db.in_dry_run();
    impl_->handles.host_transaction = impl_->db.in_transaction() && !impl_->handles.host_dry_run;

    // Text only, like load(): a precompiled chunk passed as the script would skip the same check.
    auto& lua = impl_->lua;
    impl_->time_limit.connection = impl_->db.impl_->db;
    impl_->time_limit.start(lua.lua_state(), timeout_ms);
    auto result =
        lua.safe_script(script, sol::script_pass_on_error, sol::detail::default_chunk_name(), sol::load_mode::text);
    // The JSON encoder below and the host's next calls run without the limit.
    impl_->time_limit.stop(lua.lua_state());
    if (!result.valid()) {
        if (impl_->time_limit.timed_out) {
            throw std::runtime_error(impl_->time_limit.message);
        }
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }

    if (result.return_count() == 0) {
        return {};
    }

    return lua_internal::encode_return_json(result.get<sol::object>(0));
}

}  // namespace quiver
