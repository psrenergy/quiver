#include "sandbox/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <string>

namespace quiver::lua_internal {

void bind_csv_import(sol::usertype<Database>& bind) {
    bind.set_function(
        "import_csv",
        [](Database& self,
           const std::string& collection,
           const std::string& group,
           const std::string& path,
           const sol::object& options) {
            const auto resolved = resolve_sandbox_path(self, "import_csv", path);
            self.import_csv(collection, group, resolved, parse_csv_options(options, "import_csv"));
        }
    );
}

}  // namespace quiver::lua_internal
