#include "lua_runner/path_policy.h"

#include "quiver/database.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace quiver::lua_internal {

// Resolves a script-supplied path against the database file's directory and enforces that the
// result stays strictly inside it (subdirectories allowed). Returns the resolved absolute path.
// `operation` is the public method name the user called (threaded into Pattern 1 messages).
std::string resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path) {
    namespace fs = std::filesystem;

    const std::string& db_path = db.path();
    if (db_path == ":memory:") {
        throw std::runtime_error("Cannot " + operation + ": database is in-memory, file operations are unavailable");
    }

    // Every filesystem call below uses a throwing overload, and each can fail for an OS reason
    // that is not "does not exist" -- a Windows device name ("NUL", "nul") makes
    // weakly_canonical throw outright, and a permission or I/O error can do the same. Left
    // unwrapped, that std::filesystem_error reaches the script verbatim
    // ("weakly_canonical: The parameter is incorrect.: ..."), violating the rule that no
    // standard-library message may surface unprefixed. This is the one choke point every
    // file-touching Lua operation routes through, so wrapping it here covers all of them.
    fs::path root;
    fs::path candidate;
    try {
        // Same root derivation as create_database_logger: a bare filename has an empty
        // parent_path and resolves against the current working directory.
        root = fs::path(db_path).parent_path();
        if (root.empty()) {
            root = fs::current_path();
        }
        root = fs::weakly_canonical(root);

        candidate = fs::path(path);
        if (candidate.is_relative()) {
            candidate = root / candidate;
        }
        candidate = fs::weakly_canonical(candidate);
    } catch (const fs::filesystem_error& e) {
        // e.code().message() is the bare OS reason; e.what() would repeat the paths already
        // named here and lead with the failing std function's name.
        throw std::runtime_error("Cannot " + operation + ": cannot resolve path '" + path + "': " + e.code().message());
    }

    // Strict containment: candidate == root is rejected too — the binary subsystem appends
    // ".qvr"/".toml" by string concatenation, so the root itself would yield "<root>.qvr"
    // outside the sandbox.
    const auto rel = candidate.lexically_relative(root);
    if (rel.empty() || rel == "." || rel.begin()->string() == "..") {
        throw std::runtime_error(
            "Cannot " + operation + ": path '" + path + "' escapes the database directory '" + root.string() + "'"
        );
    }

    return candidate.string();
}

}  // namespace quiver::lua_internal
