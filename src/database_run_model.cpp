#include "database_impl.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS / WIFSIGNALED / WTERMSIG
#endif

namespace {

// Hardcoded directory holding the model launcher scripts.
// TODO(rsampaio): set this to the real absolute directory containing CarbSteeler.bat/.sh.
constexpr const char* kModelDir = R"(C:\Models\CarbSteeler)";

// Base name of the model launcher; the platform-specific extension is appended below.
constexpr const char* kModelBaseName = "CarbSteeler";

#ifdef _WIN32
constexpr const char* kModelExtension = ".bat";
#else
constexpr const char* kModelExtension = ".sh";
#endif

// Decode the raw value std::system() returns into a process exit code. On Windows the MSVCRT
// system() returns the child's exit code directly; on POSIX it returns a wait() status that
// must be decoded with the W* macros.
int decode_exit_status(int system_rc) {
#ifdef _WIN32
    return system_rc;
#else
    if (WIFEXITED(system_rc)) {
        return WEXITSTATUS(system_rc);
    }
    if (WIFSIGNALED(system_rc)) {
        // Convention: report a signal-terminated child as 128 + signal number.
        return 128 + WTERMSIG(system_rc);
    }
    return system_rc;
#endif
}

}  // namespace

namespace quiver {

int Database::run_model() {
    namespace fs = std::filesystem;

    // Preconditions (checked before the model directory is touched).
    if (impl_->path == ":memory:") {
        throw std::runtime_error("Cannot run_model: in-memory database has no path to pass to the model");
    }
    if (in_transaction()) {
        throw std::runtime_error("Cannot run_model: commit or roll back the active transaction first");
    }

    // Resolve the database path to an unambiguous absolute path. The cwd is deliberately
    // not changed, so the model receives a fully-qualified argument regardless of where
    // Quiver was launched from. weakly_canonical does not require the file to exist (it
    // does here, since we are connected to it) and also normalizes "." / ".." segments.
    std::error_code ec;
    const fs::path db_abs = fs::weakly_canonical(fs::path(impl_->path), ec);
    if (ec) {
        throw std::runtime_error("Failed to run_model: could not resolve database path '" + impl_->path +
                                 "': " + ec.message());
    }

    // Resolve and verify the model script exists. A missing script is reported precisely
    // here rather than being confused with a real non-zero model exit code (the shell
    // returns ambiguous, platform-divergent codes for "command not found").
    const fs::path model_script = fs::path(kModelDir) / (std::string(kModelBaseName) + kModelExtension);
    if (!fs::exists(model_script)) {
        throw std::runtime_error("Model script not found: " + model_script.string());
    }

    // Build the shell command, quoting both paths so spaces survive. std::system runs this
    // through `cmd /c` on Windows and `/bin/sh -c` on POSIX, inheriting stdio so the model
    // output streams to the console.
    std::string command = "\"" + model_script.string() + "\" \"" + db_abs.string() + "\"";
#ifdef _WIN32
    // cmd.exe /c strips the outer quote pair when more than two quote characters are present,
    // which would mangle space-containing paths. Wrapping the whole command in one extra pair
    // preserves the inner quoting.
    command = "\"" + command + "\"";
#endif

    impl_->logger->info("run_model: launching {} with db {}", model_script.string(), db_abs.string());

    const int system_rc = std::system(command.c_str());
    if (system_rc == -1) {
        // -1 means the shell itself could not be created, not a model exit code.
        throw std::runtime_error("Failed to run_model: could not launch command: " + command);
    }

    const int exit_code = decode_exit_status(system_rc);
    impl_->logger->info("run_model: model exited with code {}", exit_code);
    return exit_code;
}

}  // namespace quiver
