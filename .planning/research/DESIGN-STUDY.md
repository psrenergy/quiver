# lua-2 Design Study (raw findings)

Generated 2026-10-04 from workflow run `wf_21c91395-8d2` (4 readers, 3 designs under the user's abstract-type directive, 2 judges, 1 compiled spike). Every claim carries file:line evidence. The synthesized recommendation is in `SUMMARY.md`; this file is the evidence behind it. Scratch probes live in the session scratchpad (`spike/`, `absprobe/`, `ownprobe/`, `abstract_probe/`, `solprobe/`), not in the repo.

## C++ types and cross-layer treatment

In C++, a BinaryFile already counts as an Expression for every operator and free function, because `Expression(const BinaryFile&)` is implicit (include/quiver/expression/expression.h:19, pinned by test ImplicitConversionFromBinaryFile, tests/test_expression.cpp:1127-1139 `Expression e = a + b;`). It does not count for member calls (aggregate, select_agents, save, ...), since BinaryFile has no such members (binary_file.h:30-46).

The conversion copies only the file's PATH. ExpressionFile reads the TOML again when it is built and keeps its own unopened BinaryFile (expression_file.cpp:10, binary_file.cpp:45-46). That private handle is opened 'r' only inside Expression::save and closed again by a CloseOnExit guard (expression.cpp:49-97). `collect_input_files` exists only for save(): it powers the output-path collision guard and the open/close of the input handles.

Making BinaryFile really derive from Expression is costly and buys nothing in the C API or Julia:
- **Header cycle**: ExpressionFile holds a BinaryFile by value, and expression.h includes binary_file.h.
- **No node at construction**: the node cannot be built when a BinaryFile is constructed, because the TOML does not exist until open('w').
- **Unsafe base**: Expression has a non-virtual destructor, and its friend operators read node_ directly.
- **Opaque handles**: the C API uses separate opaque handles, and Julia uses separate structs.

The only layer where real inheritance would help is sol2, through `sol::base_classes`. But typed sol2 parameters bring back sol2's own "stack index N, expected userdata" errors. The repo rejected those on purpose (src/AGENTS.md:285-293, csv.cpp:343-349), and they would break the Pattern 1 operand tests (test_lua_expression.cpp:555-581).

Julia handles BinaryFile with 97 forwarding methods, in addition to the Expression(file) constructor. A `Union{Expression, Binary.File}` alias plus one conversion helper would collapse them without touching C++.

Lua does not match Julia: the BinaryFile usertype gets the operators but not aggregate, aggregate_agents, select_agents or rename_agents. Julia accepts a Binary.File for all four of those.

| Claim | Evidence |
|---|---|
| Expression is a value type whose only state is a shared_ptr<ExpressionNode>; copies share the node DAG. | include/quiver/expression/expression.h:17-21 (ctors), :91 `std::shared_ptr<ExpressionNode> node_;`; no default ctor, implicit copy. |
| Expression(const BinaryFile&) is implicit and builds a fresh ExpressionFile from the file PATH only; no reference to the source BinaryFile is kept. | include/quiver/expression/expression.h:19 (no `explicit`); src/expression/expression.cpp:18 `node_(std::make_shared<ExpressionFile>(file.get_file_path()))`. |
| ExpressionFile re-reads metadata from the .toml on disk at construction and owns its own BinaryFile, constructed unopened. The source handle's in-memory metadata is ignored. | src/expression/expression_file.cpp:10 `meta_(BinaryMetadata::from_toml_file(path)), file_(path)`; include/quiver/expression/expression_node.h:42-44 (`BinaryMetadata meta_; mutable BinaryFile file_; mutable dim_map_`); src/binary/binary_file.cpp:45-46 (path ctor leaves io null, empty metadata). |
| BinaryFile is a Pimpl, non-copyable, movable, QUIVER_API class. Its header depends only on binary_metadata.h, never on expression headers. | include/quiver/binary/binary_file.h:4-5 (includes), :17 `class QUIVER_API BinaryFile`, :22-23 deleted copy, :26-27 move, :49-50 `struct Impl; std::unique_ptr<Impl> impl_;`; src/binary/binary_file.cpp:48-50 defaulted dtor/move. |
| collect_input_files is used only by Expression::save. It feeds (1) the output-path collision guard (weakly_canonical compare, before anything opens) and (2) the open-'r'/close lifecycle of every input handle through a CloseOnExit guard. Inputs are open only for the duration of save(). | src/expression/expression.cpp:49-58 (collect + collision throw `Cannot save: output path collides with input file`), :61-72 (CloseOnExit guard, then f->open('r')), :76 writer open; leaf impl src/expression/expression_file.cpp:25-27; all other nodes recurse (e.g. expression_binary.cpp:130-132, expression_ternary.cpp:76-79, expression_scalar.cpp:20 empty); doc src/AGENTS.md:1020. |
| Source open in write mode: building the expression succeeds, because open('w') already wrote the TOML. save() then throws `Cannot open_file: file is already open for writing: <canonical>` from the process-global write_registry. This is pinned at the C++, C and Julia layers. | src/binary/binary_file.cpp:17 (static write_registry), :67-69 (registry check runs before the mode switch, so 'r' opens hit it too), :92-94 (TOML written in the 'w' case), :103-104 (register); tests/test_expression.cpp:1104-1122 SaveFailsWhenInputIsOpenForWriting; tests/test_c_api_expression.cpp:1122-1141; bindings/julia/test/test_expression.jl:952,970. |
| Unflushed data cannot be read by save(): a live writer blocks save through the registry, and close() flushes before unregistering. Only once the writer is closed does save re-read the data from disk. | src/binary/binary_file.cpp:114-125 (close: flush, reset io, erase from registry, current_position=-1); :35-42 (Impl dtor flush + unregister). |
| Closing or destroying the source file after building the expression has no effect, because the ExpressionFile keeps its own handle. In Julia/C, `close!`/`quiver_binary_file_close` *deletes* the C handle, and the tests rely on closing it right after Expression(file). | src/c/binary/binary_file.cpp:76-79 `delete binary_file;`; bindings/julia/src/binary/file.jl:30-36; bindings/julia/test/test_expression.jl:140-151 (comment: `ExpressionFile keeps its own handle, so the BinaryFile can close immediately`); tests/test_c_api_expression.cpp:135-141 expr_from_file closes f right after from_file. |
| Metadata is snapshotted when the expression is built, but the handle re-reads the TOML at save time. A file rewritten or deleted in between is therefore read against stale meta_ (and broadcast metadata computed from it). A deleted file throws a non-Pattern `File not found: <path>` (invalid_argument). | src/expression/expression_file.cpp:10 vs src/binary/binary_file.cpp:72-84 (open('r') reloads impl_->metadata at :79; missing file throws at :74-77); compute_row builds dims from meta_ but reads via the file's fresh metadata: expression_file.cpp:18-23. |
| src/AGENTS.md describes ExpressionFile as caching an *open* BinaryFile, which is inaccurate: the handle is unopened except inside save(). | src/AGENTS.md:1021 `Caches an open BinaryFile`; contradicted by src/expression/expression_file.cpp:10 (unopened file_) and src/expression/expression.cpp:61-72 (opened/closed only within save). |
| There are exactly three construction sites that turn a BinaryFile into an Expression: the C++ ctor, the C API, and Lua's to_expression. | src/expression/expression.cpp:18; src/c/expression/expression.cpp:103 `new quiver_expression(quiver::Expression(file->binary_file))`; src/lua_runner/binary.cpp:111-112. |
| In C++, the implicit ctor already makes a BinaryFile usable wherever an Expression is taken by a non-member function. Operators and free functions are namespace-scope in quiver, so ADL plus one user-defined conversion applies. Member functions are not reachable on a BinaryFile. | include/quiver/expression/expression.h:94-151 (namespace-scope declarations taking const Expression&); tests/test_expression.cpp:1127-1139 `Expression e = a + b;` with two BinaryFiles; member-only API expression.h:23-40 vs BinaryFile members binary_file.h:30-46 (no aggregate/select_agents/save). |
| The C API bridges the two types with one constructor. Every other expression function takes quiver_expression_t* only. The handles are opaque structs holding the C++ objects by value, with no relationship between them. | include/quiver/c/expression/expression.h:58 `quiver_expression_from_file(quiver_binary_file_t* file, quiver_expression_t** out)`; :61 close; :64-93 apply/apply_scalar_right/apply_scalar_left/apply_unary/apply_ternary; :96 save; :99-102 get_metadata; :106-119 aggregate/aggregate_agents; :122-136 select_agents/rename_agents (all quiver_expression_t*); src/c/internal.h:32-40 (`struct quiver_binary_file { quiver::BinaryFile binary_file; }`, `struct quiver_expression { quiver::Expression expression; }`). |
| Julia: Expression and Binary.File are unrelated mutable structs wrapping different C pointers. expression.jl defines 97 forwarding methods for Binary.File (plus the Expression(file) ctor), each wrapping the file with Expression(file). | bindings/julia/src/expression.jl:1-15 (Expression struct + Expression(file::Binary.File) via quiver_expression_from_file); bindings/julia/src/binary/file.jl:1-9 (File struct). Counts: arithmetic :71-93 (20), comparisons :107-111 x6 (30), operator sugar :120-124 x4 (20), logical :137-141 x2 (10), `!` :146 (1), unary :148-152 (5), ifelse :163-176 (7), aggregate/aggregate_agents :224-239 (2), select/rename :265-266 (2). |
| Julia modules are ordered Binary first, then expression.jl. A shared abstract supertype would have to be declared in Quiver.jl before the Binary include and imported into Binary. A `Union{Expression, Binary.File}` alias inside expression.jl needs no type-hierarchy change. | bindings/julia/src/Quiver.jl:26-27 (include binary/Binary.jl then expression.jl); bindings/julia/src/binary/Binary.jl:1-9 (`using ..Quiver: C, check, Element, Optional`). |
| Julia has two distinct get_metadata generic functions, one for expressions and one for files, in different modules. | bindings/julia/src/expression.jl:183-187 (Quiver.get_metadata(e::Expression)); bindings/julia/src/binary/file.jl:92-96 (Binary.get_metadata(file::File)). |
| Lua relates the two types through to_expression (sol::object -> Expression, or BinaryFile auto-wrapped, else a Pattern 1 error) and one templated operator table bound on both usertypes. Lua holds BinaryFile as a shared_ptr<BinaryFile> userdata. | src/lua_runner/binary.cpp:107-115 (to_expression), :123-139 (binop), :165-176 (bind_expression_operators<T>), :269 and :319 (bound on BinaryFile and Expression), :321-339 (quiver.* functions take sol::object), :189-199 (open_file returns std::shared_ptr<BinaryFile>, registered in RunHandles). |
| Lua does not match Julia: the Lua BinaryFile usertype has only read/write/close/is_open/get_metadata/get_file_path plus operators. It has no aggregate/aggregate_agents/select_agents/rename_agents, which Julia accepts on a Binary.File. Yet the Lua reference says it mirrors the Julia surface. | src/lua_runner/binary.cpp:241-267 (BinaryFile methods) vs :280-318 (Expression methods); bindings/julia/src/expression.jl:224-239, 265-266; bindings/js/src/lua-api.ts:884 `Mirrors the Julia surface`, :908 `quiver.expression(r)` wrap required for methods; tests/test_lua_expression.cpp:143,294,301 always wrap with quiver.expression(fa) before :aggregate/:select_agents/:rename_agents. |
| The Lua operand Pattern 1 messages from fix C8 are pinned per operation and per argument position. | tests/test_lua_expression.cpp:555-581 (`Cannot add: operand must be an expression or a binary file, got string`, ... `Cannot expression ... got number`, leftmost-bad-operand ordering); message built by lua_type_error src/lua_runner/internal.h:187-197. |
| The repo deliberately avoids typed sol2 parameters where a wrong type would surface sol2's own text. The documented stance is sol::object plus an explicit check that owns the Pattern 1 message. | src/AGENTS.md:285-293 (`a typed sol::protected_function parameter, whose check surfaced sol2's own "stack index 3, expected function" text`); src/lua_runner/csv.cpp:343-349; src/AGENTS.md:823-829 (sol2 safeties are a backstop behind explicit checks); sol2 default format build/_deps/sol2-src/include/sol/error_handler.hpp:88-89 `stack index %d, expected %s, received %s`; no handler override in src/lua_runner (grep found none). |
| sol2 accepts a derived userdata for a base-typed parameter only through a registered inheritance check, which requires real C++ inheritance plus `sol::base_classes`. sol2 never invokes a C++ converting constructor. It does offer ADL customization points (sol_lua_check/sol_lua_get, and pointer-based sol_lua_interop_*) that take precedence over the default getters. | build/_deps/sol2-src/include/sol/stack_check_unqualified.hpp:530-560 (metatable match, then `derive<T>` + base_class_check_key inheritance check, else `value at this index does not properly reflect the desired type`); build/_deps/sol2-src/include/sol/stack_core.hpp:705-724 (sol_lua_get precedence), :727-774 (interop get/check return T*), :976-1003 (sol_lua_check precedence). |
| Real inheritance (BinaryFile : Expression) would create an include cycle: ExpressionFile holds a BinaryFile by value, so expression_node.h needs a complete BinaryFile, while binary_file.h would then need a complete Expression. | include/quiver/expression/expression_node.h:4 (includes binary_file.h), :43 `mutable BinaryFile file_;`; include/quiver/expression/expression.h:4,7 (includes binary_file.h and expression_node.h). |
| Real inheritance has no valid node to hold at BinaryFile construction. The ExpressionFile ctor needs the TOML, which a writer only creates in open('w'), and the path ctor does not open. Expression's friend operators dereference node_ unconditionally, and Expression has no virtual destructor. | src/expression/expression_file.cpp:10 (from_toml_file at ctor); src/binary/binary_metadata.cpp:263-267 (`Metadata file not found` if TOML missing); src/binary/binary_file.cpp:45-46, :86-105; src/expression/expression.cpp:99-109 (operators use lhs.node_/rhs.node_ directly); include/quiver/expression/expression.h:17-92 (no virtual dtor; ExpressionNode at expression_node.h:20 is the only virtual-dtor base). |
| Naming differs across the two types in every layer: Expression uses metadata(), BinaryFile uses get_metadata(). | include/quiver/expression/expression.h:23 vs include/quiver/binary/binary_file.h:45; include/quiver/c/expression/expression.h:99 vs include/quiver/c/binary/binary_file.h:49; src/lua_runner/binary.cpp:285 (`metadata`) vs :263 (`get_metadata`). |
| The Lua sync test only attributes a usertype method when it appears as a quoted name inside a `new_usertype<X>(...)` call. Methods attached through a templated helper (the way bind_expression_operators attaches metamethods) would not be checked for doc coverage. | bindings/js/test/lua-api-sync.test.ts:25 (Pass 1 regex `(bind|ns)\.set_function`), :29-52 (Pass 2 parses quoted names after new_usertype<T>); src/lua_runner/binary.cpp:53-55 (comment on that parser behaviour), :165-176 (helper uses `type[sol::meta_function::...] =`). |

**Risks**

- Typed sol2 Expression parameters would swap the pinned Pattern 1 operand errors (tests/test_lua_expression.cpp:555-581, fix C8) for sol2's own `stack index N, expected userdata, received number` or `value at this index does not properly reflect the desired type`. That text names neither the operation nor the argument, which breaks the root AGENTS.md error-message rule and the documented sol::object-plus-explicit-check stance (src/AGENTS.md:285-293, csv.cpp:343-349). sol2 has no override for this in the repo.
- The arithmetic and comparison operators must accept numbers on either side. A typed signature therefore needs sol::overload over every combination ({Expression, BinaryFile, number}^2 minus number/number = 8 per operator, and 8 for ifelse unless numbers stay rejected). A failed overload gives sol2's `no matching function call` text, which is also not Pattern 1. The current binop (binary.cpp:123-139) exists to avoid this.
- Real C++ inheritance (BinaryFile : Expression) would need: the header cycle broken (ExpressionFile would hold its BinaryFile through a pointer); a node built lazily in open() because the TOML does not exist before open('w'); handling for a null node_ that the friend operators dereference; and a non-virtual base destructor. It also nests one BinaryFile-that-is-an-Expression inside every ExpressionFile.
- If a design reused the caller's own BinaryFile handle as the expression input instead of the private one, save() would open('r') and close() the user's handle (expression.cpp:61-72). That would silently close a reader the user still holds, and it breaks the documented guarantee that the file can be closed immediately (test_expression.jl:140-151). Any design has to keep the separate path-based handle.
- C++ inheritance changes BinaryFile's layout and exported surface. Any C++ consumer must recompile (acceptable for a WIP project). C API consumers are unaffected only because the handles are opaque (src/c/internal.h:32-40). Neither the C API nor Julia gains anything from it: Julia still has to go through quiver_expression_from_file.
- Stale metadata: meta_ is snapshotted when the expression is built (expression_file.cpp:10), but the handle re-reads the TOML at save (binary_file.cpp:79). A file rewritten between the two gives mismatched-dimension errors or wrong reads. A file deleted in between throws the non-Pattern `File not found: <path>` (binary_file.cpp:74-77).
- Each conversion (C++ temporary, every Lua to_expression call, every Julia Expression(file)) re-parses the TOML and allocates a new ExpressionFile, so `f + f` builds two leaves. This is harmless but redundant. A cached node would make a shared leaf appear twice in collect_input_files, so the same BinaryFile would be opened twice in save. That works today, because a second open('r') replaces io and close() is idempotent (binary_file.cpp:82-83, 114-117).
- Any methods added to the Lua BinaryFile usertype through a templated helper would bypass the lua-api-sync doc check (lua-api-sync.test.ts:29-52). Write them as quoted names inside new_usertype<BinaryFile> instead, or extend the parser.
- Calling Expression(moved_from_binary_file) dereferences a null impl_ (binary_file.cpp:49 defaulted move; expression.cpp:18 calls get_file_path). This affects C++ only and cannot be reached from the C API or Lua.
- The documentation is already inaccurate: src/AGENTS.md:1021 says ExpressionFile caches an *open* BinaryFile. Fix it in the same milestone.

**Open questions (at research time; most were answered by the user afterwards — see SUMMARY.md)**

- Does 'BinaryFile should be an expression' mean a real C++ is-a (inheritance), or surface parity: wherever an Expression is accepted, a BinaryFile is too, including member-style calls like file:aggregate(...)? C++ already has parity for operators and free functions through the implicit ctor. What is missing is Lua member methods and Julia's 97-method duplication.
- Is the user willing to accept sol2-native argument error text in place of the Pattern 1 `Cannot <op>: operand must be an expression or a binary file, got <type>` messages? Doing so reverses fix C8 and the documented sol::object stance. If not, can an ADL `sol_lua_check`/`sol_lua_get` customization for Expression (stack_core.hpp:705-724, 976-1003), which accepts a BinaryFile userdata and returns an Expression by value, coexist with Expression being a registered usertype (the `self` argument of its methods)? And can it still produce an operation-named Pattern 1 message? Verify against sol2 v3.5.0 before committing to the approach.
- Should the Lua BinaryFile usertype gain aggregate/aggregate_agents/select_agents/rename_agents to match Julia (expression.jl:224-239, 265-266)? Should it also gain save and metadata, which would duplicate get_metadata under the Expression name? Julia currently has neither save(::Binary.File) nor a shared get_metadata.
- In Julia, should expression.jl collapse to `const ExpressionLike = Union{Expression, Binary.File}` plus `_expr(x)`? That stays local to one file. The alternative is an abstract supertype declared in Quiver.jl before include("binary/Binary.jl"), which changes Binary.File's declaration.
- Should Expression(const BinaryFile&) reuse an open handle's in-memory metadata instead of re-reading the TOML? And should save() detect stale metadata (meta_ vs the TOML re-read at open)?
- Should the C API stay with the single quiver_expression_from_file bridge? C has no inheritance; a file-accepting variant of each quiver_expression_* function would double the surface for no binding benefit.

## Lua surface (src/lua_runner/binary.cpp)

I inventoried every place in src/lua_runner/ where a BinaryFile or an Expression crosses the Lua boundary. Everything is in src/lua_runner/binary.cpp, plus the RunHandles registry in internal.h and lua_runner.cpp. Nothing was modified and no spike was compiled, so all sol2 behaviour below comes from reading the vendored v3.5.0 source.

**How it works today**
- Every expression entry point takes `sol::object`: the eight operator metamethods bound on both usertypes, `quiver.expression`, `abs`, `sqrt`, `log`, `exp`, `ifelse`, and `gt` through `neq`.
- Each one goes through `to_expression(o, op)`. It accepts an Expression, or wraps a BinaryFile through the implicit C++ constructor `Expression(const BinaryFile&)`. Anything else gets the Pattern 1 error `Cannot <op>: operand must be an expression or a binary file, got <lua type>`.
- Numbers are accepted only on one side of a binary operator or comparison (`binop`). They are refused by `expression`/`abs`/`sqrt`/`log`/`exp`/`ifelse`, and number-vs-number is refused too.

**Lifetime**
An Expression built from a file copies only the file's path. It reads the `.toml` once when it is built and opens its own reader only during `save()`. So it does not depend on the Lua handle, which RunHandles closes when `run()` returns. One catch: saving while the source file is still open for writing throws "Cannot open_file: file is already open for writing: <path>".

**Why typed parameters don't work as-is**
- A typed `const Expression&` would reject a BinaryFile. sol2 only checks the exact metatable, or C++ base classes declared with `sol::base_classes`, and BinaryFile and Expression are unrelated types.
- On a mismatch, sol2 raises its own text: `stack index N, expected userdata, received ...: value at this index does not properly reflect the desired type (bad argument into '<demangled signature>')`. That is not Pattern 1, and the signature part depends on the compiler.
- The project's own policy in src/AGENTS.md (lines 284-293 and 822-827) says sol2's safety checks are only a backstop and the explicit checks own every Pattern 1 message. `db:read_csv_stream`'s `on_row` was moved off a typed parameter for exactly this reason.

**Tests that pin current behaviour**
- Two tests pin the operand error text, 13 expectations in all: `LuaExpressionTest.OperandErrorsNameTheOperation` and `OperandErrorsReportTheLeftmostBadOperand`.
- Five tests pin "a file works where an expression does" directly: `FilePlusFile`, `IfElse`, `ComparisonFreeFunctions`, `LogicalOperators`, `OperatorMetamethodsOnFileAndExpression`.

**Gap against Julia**
Julia already lets a `Binary.File` be used for every operator, unary function, `ifelse`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents`. Lua's BinaryFile type has none of the Expression methods, so a script has to write `quiver.expression(f):aggregate(...)`.

**Changelog status**
The per-operation error text is unreleased: it sits under CHANGELOG [0.13.0], and v0.12.9 shipped `Cannot build expression: ...`. So that entry can be rewritten rather than added to.

| Claim | Evidence |
|---|---|
| bind_binary is the only place the binary and expression subsystems are bound to Lua. Its parameter names `bind` and `ns` are load-bearing for the sync test. | src/lua_runner/binary.cpp:184 `void bind_binary(sol::state& state, sol::usertype<Database>& bind, sol::table& ns, Database& db, RunHandles& handles)`. bindings/js/test/lua-api-sync.test.ts:25 regex `\b(bind|ns)\.set_function\(\s*"name"`, and :73 requires every `.set_function(` to match it. Call site: src/lua_runner/lua_runner.cpp:119. |
| db:open_file takes typed strings for path and mode and a sol::object for metadata. It returns a std::shared_ptr<BinaryFile> and registers it in RunHandles. | binary.cpp:187-201. The mode check message is at :192: `Cannot open_file: mode must be "r" or "w"`. Then resolve_sandboxed_path (:194), then optional_from_lua<BinaryMetadata>(metadata, "open_file", "metadata", "a BinaryMetadata") (:196), make_shared (:197), handles.add_binary_file (:198). Pinned at tests/test_lua_binary.cpp:509-518: `Cannot open_file: metadata must be a BinaryMetadata, got table` and `... got userdata`. |
| The BinaryFile usertype has only I/O methods (read, write, close, is_open, get_metadata, get_file_path) plus the shared operator metamethods. It has none of the Expression methods (save, metadata, aggregate, aggregate_agents, select_agents, rename_agents). | binary.cpp:241-267 declares the usertype with sol::no_constructor. read is (BinaryFile&, const sol::object& dims, const sol::object& allow_nulls, this_state) at :245-251. write is (BinaryFile&, const sol::object& data, const sol::object& dims) at :253-258. :269 calls bind_expression_operators(binary_file_type). |
| The BinaryFile methods report Pattern 1 errors through require_table, lua_table_to_dim_map and optional_from_lua. | tests/test_lua_binary.cpp:452 `Cannot read: dims must be a table, got number`. :456 `Cannot write: data must be a table, got number`. :461 `Cannot write: dims must be a table, got string`. :500 `Cannot read: dimension name must be a string, got number`. :178 `dimension 'row' has unsupported Lua type`. :534 `Cannot read: allow_nulls must be a boolean, got string`. |
| The Expression usertype methods mix typed and sol::object parameters. self is always a typed Expression&, and save captures the Database by reference so it can sandbox the output path. | binary.cpp:280-318. save is [&db](Expression&, const std::string& path) calling resolve_sandboxed_path(db, "save", path) at :283-284. metadata at :285-286. aggregate is (Expression&, const std::string& dimension, const std::string& op, const sol::object& parameter) at :287-292. aggregate_agents is (Expression&, const std::string& op, const sol::object& parameter) at :293-298. select_agents takes const sol::object& labels through require_table (:299-304). rename_agents takes const sol::object& mapping through collect_entries and lua_cell_as (:305-317). :319 calls bind_expression_operators(expression_type). |
| The Expression method errors are pinned by tests. | tests/test_lua_expression.cpp:204 `Cannot aggregate: unknown operation 'bogus'` (thrown at binary.cpp:158). :218 `Cannot aggregate_agents: unknown operation 'bogus'`. :317 `Cannot rename_agents: value for 'v1' has unsupported Lua type`. :322 `Cannot rename_agents: key has unsupported Lua type`. :327 and :531 `Cannot rename_agents: mapping must be a table, got number`. :526 `Cannot select_agents: labels must be a table, got number`. :542 `Cannot aggregate: parameter must be a number, got string`. :547 `Cannot aggregate_agents: parameter must be a number, got boolean`. :342 `Cannot save: output path collides with input file`. :372 `Cannot save: path '../out' escapes the database directory`. |
| bind_expression_operators<T> binds the same eight metamethods on both usertypes, and each names its Lua event in errors. The binary ones use binop. Unary minus and bitwise not take (sol::object a, sol::object) because Lua passes the operand twice. | binary.cpp:165-176: addition binop<std::plus<>>("add"), subtraction ("sub"), multiplication ("mul"), division ("div"). unary_minus is `[](sol::object a, sol::object){ return -to_expression(a, "unm"); }`. bitwise_and binop<std::logical_and<>>("band"), bitwise_or ("bor"). bitwise_not is `!to_expression(a, "bnot")`. Lua calls unary metamethods with (rb, rb): build/_deps/lua-src/src/lvm.c:1555 (TM_UNM) and :1566 (TM_BNOT). |
| binop<Op> takes two sol::objects and checks for numbers with get_type()==number. A number goes through the scalar overload via as<double>(). Number vs number falls through to to_expression and throws. With two non-numbers the left operand is decoded first, so the leftmost bad operand is the one reported. | binary.cpp:99-101 is_number. :123-139 binop: `lhs.as<double>()` at :129, `rhs.as<double>()` at :132, and the local a/b decoding at :134-137. |
| to_expression is the single gate for operands. It returns an Expression copy, or wraps a BinaryFile through the implicit constructor, and otherwise throws a Pattern 1 error via lua_type_error. Any usertype other than these two is reported as 'userdata', and a missing argument as 'nil'. | binary.cpp:107-115: `o.is<Expression>()` then `o.is<BinaryFile>()` then `Expression(o.as<BinaryFile&>())`, else `throw lua_type_error(operation, "operand", "an expression or a binary file", o)`. internal.h:187-196 builds `"Cannot " + op + ": " + what + " must be " + expected + ", got " + type`. internal.h:181-184 lua_type_name maps none to "nil". |
| The quiver.* expression functions all take sol::object. quiver.expression, abs, sqrt, log and exp take one argument. ifelse takes three, decoded left to right. gt, lt, gte, lte, eq and neq are binop instances. Only the binop-based ones accept a number. | binary.cpp:321-325 (expression, abs, sqrt, log, exp, each `[](sol::object o)`). :326-331 ifelse decodes cond, then_value, else_value into locals in that order. :334-339 `ns.set_function("gt", binop<std::greater<>>("gt"))` through neq with std::not_equal_to<>. |
| An Expression built from a BinaryFile keeps only the file's path. It reads the .toml eagerly and owns its own unopened BinaryFile, which save() opens for reading and closes on exit. So the Expression does not depend on the Lua handle or on RunHandles. | include/quiver/expression/expression.h:19 `Expression(const BinaryFile& file);` (implicit). src/expression/expression.cpp:18 `node_(std::make_shared<ExpressionFile>(file.get_file_path()))`. src/expression/expression_file.cpp:10 reads BinaryMetadata::from_toml_file(path) and constructs file_(path). include/quiver/expression/expression_node.h:43 `mutable BinaryFile file_;`. src/binary/binary_file.cpp:45-46 builds it with a null stream (closed). src/expression/expression.cpp:49-74 opens inputs with 'r' under a CloseOnExit guard. |
| Saving an Expression whose input path is still open for writing (for example a live Lua writer) throws from the process-wide write registry. | src/binary/binary_file.cpp:67-68 `Cannot open_file: file is already open for writing: <canonical>`. The registry entry is added for 'w' at :103 and removed in close() at :120-122. |
| BinaryFile lifetime is managed by RunHandles. It holds a weak_ptr per open_file handle, prunes expired entries on insert, and closes every handle still alive at run() exit (GcGuard) before collect_garbage. Expressions are not registered. | src/lua_runner/internal.h:44-48 (open_binary_files is std::vector<std::weak_ptr<BinaryFile>>). src/lua_runner/lua_runner.cpp:47-50 add_binary_file. :57-79 close_open_handles, which swallows exceptions. :149-155 the GcGuard destructor calls close_open_handles() then collect_garbage(). Pinned by tests/test_lua_binary.cpp:128-147 WriterHeldInAGlobalIsClosedWhenRunReturns and :149-159 HandleFromAnEarlierRunIsClosed. tests/test_lua_runner_lifecycle.cpp:43-47 builds quiver.expression(r) * 2.0 after a runner move. |
| There is already precedent for 'sol2 type check, but Pattern 1 message' on a usertype argument: optional_from_lua uses o.as<sol::optional<T>>() (a checked getter) and converts a failure into lua_type_error. | src/lua_runner/internal.h:225-243, specifically `if (auto value = o.as<sol::optional<T>>())` at :238 and `throw lua_type_error(...)` at :241. Used for BinaryMetadata at binary.cpp:196, and pinned by test_lua_binary.cpp:509-518. |
| sol2's check for a typed usertype parameter accepts only that type's own metatables (T, T*, unique T, container T), or a C++ base declared via derive<T>. Since BinaryFile is not related to Expression in C++, a typed Expression parameter would reject a BinaryFile. | build/_deps/sol2-src/include/sol/stack_check_unqualified.hpp:522-559: check_metatable<U>, <U*>, <d::u<U>>, <as_container_t<U>>, then the derive<T> base-class check, else `"value at this index does not properly reflect the desired type"` at :557. A non-userdata gives `"value is not a valid userdata"` at :525. include/quiver/binary/binary_file.h:17 and include/quiver/expression/expression.h:17 show the two classes are unrelated. |
| sol2's argument-mismatch text is a luaL_error string in the form `stack index %d, expected %s, received %s: <msg> (bad argument into '<demangled signature>')`. A failed sol::overload gives a fixed text that does not name the operation. Neither is Pattern 1. | sol2-src/include/sol/error_handler.hpp:86-95 (push_type_panic_string format) and :143-157 (argument_handler<types<R, Args...>> appends detail::demangle<R>() and the argument types). sol2-src/include/sol/call.hpp:157 `"sol: no matching function call takes this number of arguments and the specified types"`. |
| sol2's two hooks for accepting another type are limited here. The interop hook is off by default and not enabled by this project. The ADL customization points sol_lua_check and sol_lua_get are keyed on the unqualified type. | sol2-src/include/sol/version.hpp:519 `#define SOL_USE_INTEROP_I_ SOL_DEFAULT_OFF`. The interop_check call site is stack_check_unqualified.hpp:513-517. ADL dispatch is at sol2-src/include/sol/stack_core.hpp:704-723 (get) and :975-1003 (check, using `meta::unqualified_t<T>`). src/CMakeLists.txt:79-84 sets no interop define. |
| Project policy deliberately avoids typed sol2 parameters for tables and functions, because sol2's raw text is not Pattern 1. sol2's safety checks are documented as a backstop only, with the explicit checks owning every Pattern 1 message. Typed std::string parameters are still used in many places. | src/AGENTS.md:284-293 (tables are `sol::object` + `require_table`. `on_row` uses an explicit function check rather than a typed sol::protected_function, "whose check surfaced sol2's own 'stack index 3, expected function' text"). src/AGENTS.md:822-827 ("the backstop behind the explicit checks, which own every Pattern 1 message"). src/lua_runner/csv.cpp:345-353 and :168-172. tests/test_lua_runner_read_csv.cpp:1463-1467. Typed strings at binary.cpp:189, 272, 284, 288, 294. Root AGENTS.md 'Error Messages' principle. |
| The sol2 build flags are SOL_ALL_SAFETIES_ON=1, SOL_PRINT_ERRORS=0, SOL_SAFE_NUMERICS=1, SOL_NO_NIL=1, SOL_SAFE_GETTER=0 and SOL_SAFE_STACK_CHECK=0. | src/CMakeLists.txt:79-84 |
| Two tests pin the operand error text, 13 expectations in all. | tests/test_lua_expression.cpp:556-573 OperandErrorsNameTheOperation, with tail `: operand must be an expression or a binary file, got `: `e + 'x'` gives `Cannot add...string`, `e - 'x'` sub string, `e * {}` mul table, `e / 'x'` div string, `e & 'x'` band string, `e | 'x'` bor string, `quiver.gt(1, 2)` gt number, `quiver.eq(e, 'x')` eq string, `quiver.abs('x')` abs string, `quiver.ifelse(e, e, 'x')` ifelse string, `quiver.expression(5)` expression number. :575-582 OperandErrorsReportTheLeftmostBadOperand: `quiver.gt('a', {})` gives gt string, `quiver.ifelse(5, {}, 'x')` gives ifelse number. No test reaches the unm or bnot text. |
| Several tests pin that a raw BinaryFile works wherever an Expression does, through the operators, quiver.ifelse and the comparison functions. | tests/test_lua_expression.cpp:83-98 FilePlusFile (`fa + fb`). :116-135 IfElse (`quiver.ifelse(fa, fb, fc)`). :376-398 ComparisonFreeFunctions (`quiver.gt(fa, fb)`). :440-463 LogicalOperators (`fa & fb`, `fa | fb`, `~fa`). :489-517 OperatorMetamethodsOnFileAndExpression loops over {file = fa, expression = quiver.expression(fa)} for + - * / unary- & | ~. Most other tests wrap with quiver.expression(fa) before calling a method (for example :143, :161, :258, :294). |
| The released v0.12.9 text was the generic `Cannot build expression: ...`. The per-operation text is unreleased, under CHANGELOG [0.13.0]. | `git show v0.12.9:src/lua_runner.cpp` line 1142: `throw std::runtime_error("Cannot build expression: operand must be an expression or a binary file")`. CHANGELOG.md:8 `## [0.13.0] — unreleased`, entry at :81-84. CMakeLists.txt:4 VERSION 0.13.0. Latest tag is v0.12.9. |
| The Lua reference documents the expression surface in one code block, says files auto-wrap, and does not mention the operand error. The sync test checks names only and is receiver-agnostic for usertype methods. | bindings/js/src/lua-api.ts:879-919, with line 908 `-- files auto-wrap; scalars either side`. bindings/js/test/lua-api-sync.test.ts: :34-53 Pass 2 (bare `"name",` lines after `new_usertype<X>`, reset per file and at `.set_function(` lines). :74-83 require BinaryFile, BinaryMetadata, Expression and CsvWriter to parse to at least one method. :96-107 method coverage is `:name(` regardless of receiver. :109-118 'no documented name removed'. binary.cpp:53-55 notes the quiver.metadata key list must stay above bind_binary because of Pass 2. |
| The root AGENTS.md overstates the Lua surface: it says scripts can 'operate on files directly', which is true only for the operators and quiver.* functions, not for the Expression methods. src/AGENTS.md also misdescribes ExpressionFile as caching an open BinaryFile. | AGENTS.md:821-822 "build from a file with `quiver.expression(file)` (or operate on files directly)", but binary.cpp:241-267 gives BinaryFile no aggregate/select_agents/save/etc. src/AGENTS.md:1021 "Caches an open `BinaryFile`", but expression_node.h:43 and binary_file.cpp:45-46 show it is constructed closed and opened only inside save() (expression.cpp:67-73). src/AGENTS.md:682-686 describes binop and to_expression. src/AGENTS.md:1009 describes the implicit constructor. |
| Julia treats a Binary.File as an expression everywhere: all operators, abs/sqrt/log/exp, every mixed ifelse combination, aggregate, aggregate_agents, select_agents and rename_agents. Julia also calls the Expression metadata accessor get_metadata, while Lua uses expr:metadata() and file:get_metadata(). | bindings/julia/src/expression.jl:11-15 (Expression(file::Binary.File) via quiver_expression_from_file). :71-93 arithmetic. :98-141 comparisons and logical ops. :146-152 unary. :163-175 ifelse. :224-239 aggregate and aggregate_agents(f::Binary.File, ...). :265-266 select_agents and rename_agents(f::Binary.File, ...). :183 get_metadata(e::Expression). Lua names: binary.cpp:263 (get_metadata) and :285 (metadata). |
| Returning an Expression or a BinaryFile from a script is rejected by the JSON encoder. | src/lua_runner/return_json.cpp:197-215: a userdata matches no branch and hits `throw std::runtime_error("Cannot run: script returned an unsupported Lua type")`. The comment at :209-210 mentions BinaryFile. |

**Risks**

- Switching the quiver.* functions and metamethods to a typed `const Expression&` with no conversion mechanism rejects BinaryFile (sol2 checks the exact metatable, and there is no inheritance between the two types). That breaks FilePlusFile, IfElse, ComparisonFreeFunctions, LogicalOperators and OperatorMetamethodsOnFileAndExpression. It is the opposite of 'BinaryFile should be an expression'.
- Letting sol2 do the type check swaps the Pattern 1 text `Cannot <op>: operand must be ..., got <type>` for sol2's raw `stack index N, expected userdata, received ...: ... (bad argument into '<demangled signature>')`. The demangled signature differs between MSVC and GCC/Clang. This breaks the 13 pinned expectations and contradicts the root error-message principle and the src/AGENTS.md:284-293 / 822-827 policy (a typed protected_function was already reverted for this reason). The user's request and the project rules conflict here, so the user must decide.
- sol::overload over {Expression, BinaryFile, number} needs 8 overloads per binary operator or comparison (12 of them) and 8 for ifelse. A failed match gives 'sol: no matching function call takes this number of arguments and the specified types', which names no operation.
- An ADL `sol_lua_check`/`sol_lua_get` customization for Expression is keyed on the unqualified type (stack_core.hpp:975-1003). It would probably also apply to the `Expression& self` of every Expression method, to `o.is<Expression>()`, and to pushes. A getter that returns by value may not bind to `Expression&`. Unverified; this needs a compiled spike under these SOL_* flags.
- A Lua-side wrapper (for example a struct deriving from Expression and holding shared_ptr<BinaryFile>, registered with `sol::base_classes`) changes what db:open_file returns. RunHandles keeps a weak_ptr<BinaryFile> (internal.h:48), so the wrapper must keep the shared_ptr alive. It would also parse the .toml on every open_file (ExpressionFile's constructor) and would have to merge the BinaryFile and Expression method tables (get_metadata vs metadata).
- Making BinaryFile inherit Expression in the C++ core would affect the C API (quiver_expression_from_file) and Julia. That is wider than the request and not something to change without the user.
- Behaviour changes hidden behind typing: today a missing argument reads as 'got nil' (internal.h:181-184), while a typed parameter gives sol2's 'received no value'. A typed unary metamethod must tolerate the duplicated second operand Lua passes (lvm.c:1555/1566).
- Sync-test traps: registering anything through a receiver not named `bind` or `ns` (for example `binary_file_type.set_function`) fails the count check at lua-api-sync.test.ts:73. Methods added by index assignment (`type["aggregate"] = ...`, the same style as bind_expression_operators) are invisible to Pass 2. And because the coverage check is receiver-agnostic, adding aggregate/save to BinaryFile would pass even if the doc never shows them on a file.
- If BinaryFile gains the Expression methods, `file:save()` on a still-open writer will throw the write-registry error (binary_file.cpp:67-68), because save reopens its inputs for reading. That needs documenting or a test.
- Changing the operand text again before 0.13.0 ships means rewriting the CHANGELOG.md:81-84 entry, not adding a new one. Against released v0.12.9 the user-visible change is still from 'Cannot build expression: ...'.
- Doc drift to fix in the same pass: AGENTS.md:821-822 'operate on files directly', src/AGENTS.md:1021 'Caches an open BinaryFile', src/AGENTS.md:682-686 (binop/to_expression description), and lua-api.ts:908.

**Open questions (at research time; most were answered by the user afterwards — see SUMMARY.md)**

- Should wrong-type errors stay Pattern 1 (root principle, 13 pinned expectations), or does the user now accept sol2's raw argument text? Their request ('let sol2 do the type checking') conflicts with the documented policy in src/AGENTS.md:284-293 and 822-827.
- Does 'BinaryFile should be an expression' mean operands only (already true at runtime via to_expression), or also the methods, so that `file:aggregate`, `file:aggregate_agents`, `file:select_agents`, `file:rename_agents`, `file:save` and `file:metadata` work as they do in Julia?
- Is this a Lua-binding change only (a wrapper type with `sol::base_classes`, or a sol2 customization point), or a core C++ change (BinaryFile deriving from Expression) that would ripple into the C API and Julia?
- Once a file is an expression, should `quiver.expression` stay? It mirrors Julia's `Expression(file)` and the C `quiver_expression_from_file`, and removing it would also require updating lua-api.ts, which the sync test's 'no documented name removed' check enforces.
- Metadata accessor names: BinaryFile uses get_metadata and Expression uses metadata in Lua, while Julia uses get_metadata for both. Unify them?
- Should a number be accepted where an Expression is (`quiver.abs(5)`, `quiver.ifelse(cond, 1.0, 0.0)`)? Today it is rejected on purpose (binary.cpp:103-106). Julia's ifelse also takes only Expression/File.
- Is a compiled spike in the scratchpad wanted before planning, to confirm how `sol_lua_check`/`sol_lua_get` for Expression and a `sol::base_classes` wrapper behave under SOL_ALL_SAFETIES_ON=1 / SOL_SAFE_GETTER=0 (self-argument binding, the exact error text on MSVC)?

## sol2 v3.5.0 mechanics

Checked against the sol2 v3.5.0 source and two MSVC probe programs built with Quiver's exact SOL_* defines (in scratchpad/solprobe/; no repo files changed). Answers in order:

1. **sol::bases can't make a BinaryFile an Expression.** It works by `static_cast<Base*>` from the derived pointer, so it needs real C++ inheritance. BinaryFile and Expression are unrelated classes.
2. **sol2 never applies C++ implicit conversions between usertypes.** A typed `const Expression&` or by-value `Expression` parameter rejects a BinaryFile, even though `Expression(const BinaryFile&)` is implicit.
3. **The working route is the ADL customization pair `sol_lua_check` / `sol_lua_get` for `types<Expression>`.** It takes priority over sol2's own usertype checker, and sol2 ships an example that does exactly this for a type that is also a usertype (`customization_convert_on_get.cpp`). In the probe, `quiver.abs(file)`, a by-value parameter, `quiver.gt(file, 2)` through `sol::overload`, and `Expr.show(file)` all worked. Cost: every lambda taking `Expression& self` must become `const Expression&` (otherwise compile error C2664). All Expression methods are already const, so that is a safe change.
4. **Error propagation.** A typed mismatch raises sol2's own text through `luaL_error`. It is a Lua error, not a C++ exception crossing Lua: `run()`'s `safe_script` catches it and rethrows "Failed to run Lua script: ...". The text looks like `"[string ...]:1: stack index 1, expected userdata, received number: <msg> (bad argument into 'quiver::Expression(const quiver::Expression&)')"`. It is not Pattern 1, does not name the operation, and the signature part depends on the compiler.
5. **Pattern 1 can't come from sol2's typed check itself.** Two real ways to keep it:
   - **Fallback overload** (recommended if Pattern 1 stays): a last `sol::variadic_args` overload per function, sol2's documented "overloading with fallback" pattern, which knows its operation name and throws the Pattern 1 message.
   - **`sol::argument_handler` specialization**: keyed by the full signature, so abs/sqrt/log/exp/expression share one handler. The operation name is only available from `lua_getinfo`, which reported "Cannot f" for a local alias in the probe.
6. **Unusable options.** Policies run after the call. The interop hooks are off by default and can only return a pointer to an existing object, so they can't build a new Expression. `sol::filters` does not exist in v3.5.

Laziest version that keeps every current test passing: add the check/get pair, and shrink `to_expression` to an `is<Expression>` test plus the existing Pattern 1 throw. A fully typed signature needs the decision in the open questions below.

| Claim | Evidence |
|---|---|
| sol::base_classes works only for a real C++ derived-to-base relationship. Registering it sets a runtime weak_derive<Base> flag and stores class_check/class_cast function pointers in the derived metatable; the cast is static_cast<Base*>(static_cast<T*>(p)), which does not compile for unrelated types. | usertype_storage.hpp:376-392 (update_bases, weak_derive<Bases>::value = true at :386), :267-268 (class_check/class_cast stored), :689-691 (base_classes_tag key); inheritance.hpp:91-107 (type_cast_bases/type_cast_with: static_cast<void*>(static_cast<Base*>(data))); forward.hpp:216-265 (derive/base/weak_derive traits, SOL_BASE_CLASSES/SOL_DERIVED_CLASSES macros) |
| A derived usertype does satisfy a `const Base&` or by-value `Base` parameter. The checker falls back to class_check when the metatable is not Base's; the getter adjusts the pointer through class_cast; a by-value parameter copies from the returned T&. | stack_check_unqualified.hpp:521-559 (check_metatable for U, U*, d::u<U>, as_container_t<U>, then has_derived -> class_check); stack_get_unqualified.hpp:887-925 (get_no_lua_nil_from: class_cast, then static_cast<T*>), :180-182 and :927-929 (unqualified_getter<T> -> as_value_tag<T> returns T&) |
| sol::bases<Expression> on BinaryFile is impossible. The two classes have no base relationship, and Expression's only state is a shared_ptr<ExpressionNode>, so even a forced cast would be undefined behaviour. | include/quiver/expression/expression.h:17 (class QUIVER_API Expression {, no base), :91 (std::shared_ptr<ExpressionNode> node_); include/quiver/binary/binary_file.h:17 (class QUIVER_API BinaryFile {, no base) |
| sol2 never applies C++ implicit conversions between usertypes, even though C++ already has the implicit ctor Expression(const BinaryFile&). Verified at runtime: with plain sol2, both `const Expr&` and by-value `Expr` parameters rejected a shared_ptr<File> userdata. | expression.h:19 (non-explicit Expression(const BinaryFile& file)); stack_check_unqualified.hpp:521-559 and stack_get_unqualified.hpp:917-925 (no construction path). Probe plain.exe: "stack index 1, expected userdata, received sol.sol::d::u<q::File>: value at this index does not properly reflect the desired type (bad argument into 'q::Expr(const q::Expr&)')", and the same for 'q::Expr(q::Expr)' |
| v3.5 customization-point signatures, as detected by ADL. Check: `template <class H> bool sol_lua_check(sol::types<T>, lua_State*, int index, H&& handler, sol::stack::record&)`. Get: `T sol_lua_get(sol::types<T>, lua_State*, int, sol::stack::record&)`. Optional extras: `sol_lua_check_get(sol::types<T>, lua_State*, int, H&&, record&)` returning an optional, `sol_lua_interop_check(sol::types<T>, lua_State*, int, sol::type, H&&, record&)`, and `std::pair<bool, T*> sol_lua_interop_get(sol::types<T>, lua_State*, int, void*, record&)`. | stack_core.hpp:616-659 (adl_*_test_t detection aliases); docs api/stack.rst:223-310 |
| A customization for types<Expression> takes priority over sol2's usertype checker for every `Expression`, `const Expression&` and `Expression&` parameter. The qualified checker strips cv/ref and unqualified_check consults ADL before unqualified_checker; the getter path reaches sol_lua_get the same way. | stack_check_qualified.hpp:31-85 (falls to stack::unqualified_check<X>); stack_core.hpp:975-1008 (unqualified_check/check: if constexpr is_adl_sol_lua_check_v<Tu> -> sol_lua_check); stack_get_unqualified.hpp:186-258 (qualified_getter -> unchecked_unqualified_get<Tu>); stack_core.hpp:704-713 (unchecked_unqualified_get -> sol_lua_get) |
| Customizing check/get for a type that is also a registered usertype is an officially supported pattern. The backdoors check_usertype<T>/get_usertype<T> route through as_value_tag<T>, which never re-enters the types<T> customization, so there is no recursion. Push and usertype registration are unaffected. | examples/source/customization_convert_on_get.cpp:11-41; docs api/stack.rst:95-115 ('backdoor... while at the same time providing your own customization'); stack_core.hpp:1006-1017 (check_usertype -> check<as_value_tag<Tu>>), :1186-1190 (get_usertype -> as_value_tag) |
| Probe with custom sol_lua_check/sol_lua_get for Expr (check_usertype<Expr> || check_usertype<File>; get builds Expr(File) when it is a file): every BinaryFile-as-Expression call works through typed parameters. | Probe custom.exe: quiver.abs(file) -> abs(file:f); quiver.byval(file) -> abs(file:f); quiver.gt(file, 2) via sol::overload -> (file:f>2.000000); Expr.show(file) with a `const Expr& self` lambda -> file:f |
| A value-returning sol_lua_get breaks every lambda that takes `Expression& self`; the same code compiles without the customization. All Expression methods are const, so changing those lambdas to `const Expression& self` is safe. Today they take `Expression& self`. | Probe /DCUSTOM /DSELF_REF: sol/wrapper.hpp(70): error C2664 "cannot convert argument 1 from '_Ty' to 'q::Expr &'" (plain build exit 0); expression.h:23-40 (metadata/save/aggregate/aggregate_agents/select_agents/rename_agents all const); src/lua_runner/binary.cpp:284-317 (Expression& self lambdas) |
| The customization does not affect Expression* parameters or member-function-pointer `self`, which use the pointer path. It does affect sol::object::is/as<Expression>, sol::optional<Expression>, lambda self parameters, variant alternatives, and sol2's automatic __eq/__lt wrappers. | call.hpp:485-487 and :578-580 (check_get_arg<Ta*>); stack_check_unqualified.hpp:566-574 (as_pointer_tag -> check_usertype); stack_check_get_unqualified.hpp:129-135 (check_getter composes the custom check and get); stack_core.hpp:1388 (comparsion_operator_wrap uses unqualified_check_get<T>) |
| The interop hooks cannot implement BinaryFile-as-Expression. They compile in only under SOL_USE_INTEROP (default off), and interop_get must return a pointer to an existing T, so it cannot produce a fresh Expression from a BinaryFile. | version.hpp:512-520 (SOL_USE_INTEROP_I_ SOL_DEFAULT_OFF); stack_check_unqualified.hpp:516-520; stack_get_unqualified.hpp:891-896; stack_core.hpp:620-621, 724-737 |
| ADL placement: the associated namespaces of sol::types<quiver::Expression> are sol and quiver. The customization must be declared in namespace quiver (not in lua_internal's anonymous namespace) and be visible before the binding templates instantiate. Among the Lua runner files, only binary.cpp binds Expression. | stack_core.hpp:647 (is_adl_sol_lua_check_v variable template evaluated at instantiation); probe declared it after #include <sol/sol.hpp> in Expr's namespace and it was found; grep: Expression is used in sol bindings only in src/lua_runner/binary.cpp (other matches in csv.cpp/lua_runner.cpp/return_json.cpp are BinaryFile comments or handle registry) |
| With Quiver's defines, every non-overloaded bound function runs multi_check with argument_handler<types<R, Args...>> before calling the function, and ignores its return value, so a handler must never return. SOL_SAFE_FUNCTION_CALLS comes from ALL_SAFETIES, and PROPAGATE_EXCEPTIONS defaults off for C Lua. There is no arity check: extra arguments are ignored and a missing one reads as 'no value'. | stack.hpp:186-205; version.hpp:400-407; forward_detail.hpp:34-39; compatibility/lua_version.hpp:150-157 (SOL_DEFAULT_OFF). Probe: quiver.abs(e, 99) succeeds; quiver.abs() -> 'received no value' |
| Typed-mismatch text format is 'stack index N, expected <type>, received <actual>[: <message>] (bad argument into '<R>(<Args...>)')'. A userdata's actual name is its metatable __name, i.e. 'sol.' + demangled type; BinaryFile, held by shared_ptr, shows as 'sol.sol::d::u<quiver::BinaryFile>'. The signature is demangled per compiler (__FUNCSIG__ on MSVC, __PRETTY_FUNCTION__ on GCC/Clang), so the text differs across compilers. | error_handler.hpp:61-84 (associated_type_name reads __name), :86-96 (format strings), :142-158 (argument_handler aux message); usertype_traits.hpp:41-44; lauxlib.c:314-324 (luaL_newmetatable sets __name); demangle.hpp:51-84 vs :87-129; binary.cpp:190-199 (open_file returns shared_ptr<BinaryFile>) |
| MSVC probe texts with plain typed `const Expr&`: number -> 'value is not a valid userdata'; table -> 'received table: value is not a valid userdata'; other usertype -> 'received sol.q::Meta: value at this index does not properly reflect the desired type'; std::string return demangles as 'std::basic_string<char,std::char_traits<char>,std::allocator<char> >(const q::Expr&)'. With a custom check that passes a message, the message lands inside the same frame: '...received number: operand must be an expression or a binary file (bad argument into 'q::Expr(const q::Expr&)')'. | Probe plain.exe and custom.exe output |
| A typed-argument error propagates as a Lua error, not a C++ exception. luaL_error prefixes the Lua caller's position, because level 1 is the chunk that called the C function. Lua is compiled as C, so it unwinds with longjmp to safe_script's pcall; sol2's default traceback handler appends a traceback; LuaRunner::run rethrows as std::runtime_error('Failed to run Lua script: ' + what). | lauxlib.c:217-227 (luaL_where); build/build.ninja:856 (ldo.c built with C_COMPILER__lua_library); ldo.c:70-75 (longjmp); state_handling.hpp:75-100 (default_traceback_error_handler set as default handler); src/lua_runner/lua_runner.cpp:158-164. Probe: '[string "return quiver.abs(5)"]:1: stack index 1, ...' plus 'stack traceback:' |
| A C++ exception thrown inside a bound function, including from sol_lua_get or from a custom argument handler, is caught by sol2's trampoline. Its what() is pushed verbatim and raised with lua_error, so there is no position prefix. | trampoline.hpp:146-170 (catch std::exception -> call_exception_handler -> lua_error), :48-73 (default handler pushes what); probe handler.exe: 'Cannot abs: operand #1 must be an expression or a binary file, got number' |
| sol::overload tries candidates in declaration order. Arity must match exactly, except for runtime-variadic candidates. Each candidate is type-checked with no_panic; the first match is called through the normal checked path. If nothing matches, the error is 'sol: no matching function call takes this number of arguments and the specified types', with no operation name or operand index. | call.hpp:154-158, :160-204, :254-296, :733-747; function_types_overloaded.hpp:47-61 (call_wrapped with default checked); probe: quiver.gt(1, 2) and quiver.gt(e) both give that text |
| A custom sol_lua_check must report failure through the handler and return false, never throw. Overload resolution, object::is and the variant checker all call it with no_panic and rely on a plain false. | call.hpp:192 and :288 (check_types(..., &no_panic, ...)); stack_check_unqualified.hpp:720-727 (variant uses &no_panic); docs api/stack.rst:257-269 |
| Pattern 1 option A, argument_handler specialization: it is real, an explicit specialization of sol::argument_handler<sol::types<R, Args...>>, and may throw to get exact text. It is keyed by the whole signature, not the function: abs/sqrt/log/exp/expression all share types<Expression, const Expression&>. The operation name is only available from lua_getinfo('n'), which reports the call-site name. The overload no-match path bypasses it. | error_handler.hpp:132-158; probe handler.exe: 'Cannot abs: ...' but 'local f = quiver.abs; f(5)' -> 'Cannot f: ...'; call.hpp:157 (overload failure uses luaL_error directly) |
| Pattern 1 option B, fallback overload: a final sol::variadic_args candidate matches anything because variadic_args always type-checks and runtime-variadic candidates skip the arity filter. A per-operation fallback lambda that captures its name can throw the exact current Pattern 1 text. This is sol2's documented 'overloading with fallback' pattern. | stack_check_unqualified.hpp:212-217 (variadic_args -> true); call.hpp:178-190 (!runtime_variadics_t guards the arity filter); examples/source/overloading_with_fallback.cpp:14-41 |
| Options that do not work: passing a message into the handler is embedded in sol2's frame, not Pattern 1. Policies run after the wrapped call, so they cannot intercept argument errors. There is no sol::filters in v3.5. set_exception_handler only changes how C++ exceptions are converted, not argument checks. | error_handler.hpp:86-96; call.hpp:858-866 (handle_policy after call); policies.hpp:33-85; grep 'filters' in include/sol: no matches; state_view.hpp:747 |
| Repo precedent and tests both point toward keeping Pattern 1. A typed sol::protected_function parameter was already replaced by sol::object plus an explicit check, because sol2's text is not Pattern 1. Two tests pin the per-operation Pattern 1 operand text. | src/lua_runner/csv.cpp:345-349; tests/test_lua_runner_read_csv.cpp:1463-1466; tests/test_lua_expression.cpp:556-584 (OperandErrorsNameTheOperation, OperandErrorsReportTheLeftmostBadOperand); tests/test_lua_runner.h:48-55 (substring match) |
| Expression(const BinaryFile&) captures only the file path, so a temporary Expression built inside sol_lua_get does not depend on the userdata's lifetime. BinaryFile is non-copyable, so a by-value std::variant<Expression, BinaryFile> parameter is impossible. | src/expression/expression.cpp:18; binary_file.h:23-24 |
| Pre-existing quirk, made broader by the customization. Expression's operator< and operator== return Expression, so sol2 auto-registers __lt/__le/__eq, and Lua coerces the returned userdata to true. With the customization, `expr == file` changes from false to true. | expression.h:64-81; usertype_core.hpp:129-150; traits.hpp:478-497; probe2: e1<e2, e2<e1, e1==e2 all true in both builds; e1==f false in plain, true in custom |
| Julia already lets a File stand in for an Expression in aggregate, aggregate_agents, select_agents and rename_agents. Lua's BinaryFile usertype has none of those methods; it only gets the operators, through bind_expression_operators. | bindings/julia/src/expression.jl:224-240, 265-266; src/lua_runner/binary.cpp:241-269, 165-176 |

**Risks**

- Using sol2's typed-argument text replaces 'Cannot <op>: operand must be an expression or a binary file, got <type>' with '[string ...]:1: stack index N, expected userdata, received ... (bad argument into '<demangled signature>')'. That breaks the C8 Pattern 1 decision and tests/test_lua_expression.cpp:556-584, and contradicts the csv.cpp:345-349 precedent. The demangled signature also differs between compilers (demangle.hpp:51-129).
- sol::overload makes arity strict (call.hpp:178-190). Calls that succeed today with extra arguments, such as quiver.abs(e, 99), would fail or drop to the fallback. Lua's __unm passes the operand twice, so unary overloads need an extra placeholder parameter.
- A value-returning sol_lua_get forces every `Expression& self` lambda to become `const Expression&`; otherwise the build fails with C2664 in sol/wrapper.hpp(70).
- ODR and visibility: the customization must be in namespace quiver and seen by every translation unit that instantiates sol2 checks or gets for Expression (only binary.cpp today). A future translation unit that doesn't see it would silently use different template instances (ill-formed, no diagnostic required).
- The customization reaches sol2's automatic comparison wrappers: `expr == file` flips from false to true. Lua `<`/`==` between expressions is already always true (pre-existing footgun).
- A custom argument_handler or sol_lua_check must never return normally after a failure: multi_check's result is ignored (stack.hpp:194-198) and the arguments are then read unchecked, because SOL_SAFE_GETTER=0.
- If the customization is placed after a new_usertype<...> line in a binding file, any line consisting of a bare lowercase quoted string followed by a comma inside it would be picked up as a usertype method by lua-api-sync.test.ts pass 2 (binary.cpp:53-55 already warns about this).

**Open questions (at research time; most were answered by the user afterwards — see SUMMARY.md)**

- Error text: should the typed signatures accept sol2's own message (giving up Pattern 1 and the operation name), or keep Pattern 1 through a per-operation sol::variadic_args fallback overload? This is the real decision for the user.
- Binary operators: should they become sol::overload of (E,E), (double,E), (E,double) plus a Pattern 1 fallback, or keep taking sol::object for the number-or-expression operands, using is<Expression>/as<Expression> now that the customization makes a BinaryFile pass as an Expression?
- Does 'BinaryFile should be an expression' also mean Expression methods on BinaryFile in Lua (file:aggregate/aggregate_agents/select_agents/rename_agents, as Julia allows)? With `const Expression& self` lambdas plus the customization, the same lambdas can be registered on both usertypes.
- Is the stricter arity under sol::overload acceptable (extra arguments become errors)?
- Should the fix for `e1 < e2` / `e1 == e2` always returning true in Lua be in scope? It means disabling sol2's automatic __lt/__le/__eq for Expression.

## File layout mapping

Read-only investigation; nothing in the repo was changed. The only scratch files are under the scratchpad (floor.cpp, binary_part.cpp, expression_part.cpp, time.bat, time2.bat, obj/).

Proposal: mirror the core `database_*.cpp` split for the Database binders, and give each core subsystem folder its own Lua file. That means 13 binder files: database, database_create, database_read, database_update, database_delete, database_metadata, database_time_series, database_query, database_csv_export, database_csv_import, csv, binary and expression. lua_runner.cpp, internal.h, return_json.cpp and path_policy.{h,cpp} stay as they are.

There are three folds, each with a precedent in an existing layer:
- **Transactions and dry runs** go in database.cpp, because the core puts them there (src/database.cpp:319-386). The C API's separate file exists only because each C function carries about 12 lines of try/catch (src/c/database_transaction.cpp: 80 lines for 7 functions).
- **describe, describe_collection and summarize_collection** also go in database.cpp, because the C API puts them there (src/c/database.cpp:135-163) and has no database_describe.cpp.
- **get_time_series_metadata and list_time_series_groups** stay in database_metadata.cpp, even though the core and C API put them in database_time_series (src/database_time_series.cpp:67,77; src/c/database_time_series.cpp:16,33). The reason is that metadata_to_lua, list_metadata_lua and get_metadata_lua (db_metadata.cpp:22-74) serve all eight metadata functions.

**Name overrides and registration order:** no binder overrides a name another one sets. All 86 set_function names (71 `bind`, 15 `ns`) are each registered exactly once (checked with grep | uniq -c). sol2 looks up usertypes when a function is called, not when it is registered: csv.cpp registers the CsvWriter usertype at line 405, after write_csv at line 384 already returns unique_ptr<CsvWriter>. So the order between binders does not matter, except for these fixed constraints:
- open_libraries, then nil dofile/loadfile, then the load wrapper, then the quiver table (lua_runner.cpp:92-110).
- Exactly one Database usertype, created before every binder (lua_runner.cpp:111-112).
- lua["db"] set last (lua_runner.cpp:120).
- One new constraint: bind_binary must run before bind_expression, because it returns the BinaryFile usertype that bind_expression adds the operators to. The compiler enforces this through the data dependency.

Proposed constructor body (follows the core order in src/CMakeLists.txt:3-12, then the subsystems):
```cpp
sol::table ns = lua.create_named_table("quiver");
auto bind = lua.new_usertype<Database>("Database");
lua_internal::bind_database(bind);
lua_internal::bind_create(bind);
lua_internal::bind_read(bind);
lua_internal::bind_update(bind);
lua_internal::bind_delete(bind);
lua_internal::bind_metadata(bind);
lua_internal::bind_time_series(bind);
lua_internal::bind_query(bind);
lua_internal::bind_csv_export(bind);
lua_internal::bind_csv_import(bind);
lua_internal::bind_csv(lua, bind, handles);
auto binary_file_type = lua_internal::bind_binary(lua, bind, ns, handles);
lua_internal::bind_expression(lua, ns, binary_file_type, db);
lua["db"] = &db;
```

**Compile cost:** sol2 TUs go from 9 to 15. Isolated, each extra TU costs about 4-5 s of CPU; in a full build about 10 s. Wall time should fall, because the binary/expression split removes the current long pole (binary.cpp, 42.1 s in the last full build). Details are in sol2_tu_cost.

**Behaviour:** this is a pure move. No Lua name, usertype name or error text changes, so no CHANGELOG entry is needed.

| Lua name | Current file | Core file | Proposed file |
|---|---|---|---|
| `db:is_healthy` | src/lua_runner/db_core.cpp:134 | src/database.cpp:129 | src/lua_runner/database.cpp |
| `db:current_version` | src/lua_runner/db_core.cpp:135 | src/database.cpp:239 | src/lua_runner/database.cpp |
| `db:path` | src/lua_runner/db_core.cpp:136 | src/database.cpp:243 | src/lua_runner/database.cpp |
| `db:begin_transaction` | src/lua_runner/db_core.cpp:137 | src/database.cpp:319 (C API src/c/database_transaction.cpp:6) | src/lua_runner/database.cpp |
| `db:commit` | src/lua_runner/db_core.cpp:138 | src/database.cpp:334 (C API src/c/database_transaction.cpp:18) | src/lua_runner/database.cpp |
| `db:rollback` | src/lua_runner/db_core.cpp:139 | src/database.cpp:345 (C API src/c/database_transaction.cpp:30) | src/lua_runner/database.cpp |
| `db:in_transaction` | src/lua_runner/db_core.cpp:140 | src/database.cpp:330 (C API src/c/database_transaction.cpp:42) | src/lua_runner/database.cpp |
| `db:transaction` | src/lua_runner/db_core.cpp:141-150 (run_in_scope) | Lua-only composite over src/database.cpp:319/334/345 | src/lua_runner/database.cpp |
| `db:begin_dry_run` | src/lua_runner/db_core.cpp:151 | src/database.cpp:356 (C API src/c/database_transaction.cpp:49) | src/lua_runner/database.cpp |
| `db:end_dry_run` | src/lua_runner/db_core.cpp:152 | src/database.cpp:368 (C API src/c/database_transaction.cpp:61) | src/lua_runner/database.cpp |
| `db:in_dry_run` | src/lua_runner/db_core.cpp:153 | src/database.cpp:386 (C API src/c/database_transaction.cpp:73) | src/lua_runner/database.cpp |
| `db:dry_run` | src/lua_runner/db_core.cpp:154-163 (run_in_scope) | Lua-only composite over src/database.cpp:356/368 | src/lua_runner/database.cpp |
| `db:validate_migrations` | src/lua_runner/db_core.cpp:199-201 | src/database.cpp:268 | src/lua_runner/database.cpp |
| `db:describe` | src/lua_runner/db_core.cpp:190 | src/database_describe.cpp:232 (C API folds it into src/c/database.cpp:135) | src/lua_runner/database.cpp |
| `db:describe_collection` | src/lua_runner/db_core.cpp:191 | src/database_describe.cpp:254 (C API src/c/database.cpp:147) | src/lua_runner/database.cpp |
| `db:summarize_collection` | src/lua_runner/db_core.cpp:192 | src/database_describe.cpp:262 (C API src/c/database.cpp:163) | src/lua_runner/database.cpp |
| `db:export_csv` | src/lua_runner/db_core.cpp:164-175 | src/database_csv_export.cpp:123 | src/lua_runner/database_csv_export.cpp |
| `db:import_csv` | src/lua_runner/db_core.cpp:176-186 | src/database_csv_import.cpp:330 | src/lua_runner/database_csv_import.cpp |
| `db:number_of_elements` | src/lua_runner/db_core.cpp:188 | src/database_read.cpp:270 | src/lua_runner/database_read.cpp |
| `db:query_string` | src/lua_runner/db_core.cpp:194 | src/database_query.cpp:6 | src/lua_runner/database_query.cpp |
| `db:query_integer` | src/lua_runner/db_core.cpp:195 | src/database_query.cpp:10 | src/lua_runner/database_query.cpp |
| `db:query_float` | src/lua_runner/db_core.cpp:196 | src/database_query.cpp:14 | src/lua_runner/database_query.cpp |
| `db:read_element_ids` | src/lua_runner/db_read.cpp:109 | src/database_read.cpp:264 | src/lua_runner/database_read.cpp |
| `db:read_scalar_strings` | src/lua_runner/db_read.cpp:111 | src/database_read.cpp:26 | src/lua_runner/database_read.cpp |
| `db:read_scalar_integers` | src/lua_runner/db_read.cpp:112 | src/database_read.cpp:6 | src/lua_runner/database_read.cpp |
| `db:read_scalar_floats` | src/lua_runner/db_read.cpp:113 | src/database_read.cpp:16 | src/lua_runner/database_read.cpp |
| `db:read_vector_integers` | src/lua_runner/db_read.cpp:115 | src/database_read.cpp:69 | src/lua_runner/database_read.cpp |
| `db:read_vector_floats` | src/lua_runner/db_read.cpp:116 | src/database_read.cpp:80 | src/lua_runner/database_read.cpp |
| `db:read_vector_strings` | src/lua_runner/db_read.cpp:117 | src/database_read.cpp:91 | src/lua_runner/database_read.cpp |
| `db:read_set_integers` | src/lua_runner/db_read.cpp:119 | src/database_read.cpp:138 | src/lua_runner/database_read.cpp |
| `db:read_set_floats` | src/lua_runner/db_read.cpp:120 | src/database_read.cpp:149 | src/lua_runner/database_read.cpp |
| `db:read_set_strings` | src/lua_runner/db_read.cpp:121 | src/database_read.cpp:160 | src/lua_runner/database_read.cpp |
| `db:read_scalars_by_id` | src/lua_runner/db_read.cpp:123 | composite: src/database_metadata.cpp:55 + src/database_read.cpp:36/47/58 | src/lua_runner/database_read.cpp |
| `db:read_vectors_by_id` | src/lua_runner/db_read.cpp:124 | composite: src/database_metadata.cpp:67 + src/database_read.cpp:102/114/126 | src/lua_runner/database_read.cpp |
| `db:read_sets_by_id` | src/lua_runner/db_read.cpp:125 | composite: src/database_metadata.cpp:77 + src/database_read.cpp:171/183/195 | src/lua_runner/database_read.cpp |
| `db:read_element_by_id` | src/lua_runner/db_read.cpp:126 | composite of the three *_by_id composites above | src/lua_runner/database_read.cpp |
| `db:get_scalar_metadata` | src/lua_runner/db_metadata.cpp:80 | src/database_metadata.cpp:6 | src/lua_runner/database_metadata.cpp |
| `db:get_vector_metadata` | src/lua_runner/db_metadata.cpp:81 | src/database_metadata.cpp:18 | src/lua_runner/database_metadata.cpp |
| `db:get_set_metadata` | src/lua_runner/db_metadata.cpp:82 | src/database_metadata.cpp:37 | src/lua_runner/database_metadata.cpp |
| `db:get_time_series_metadata` | src/lua_runner/db_metadata.cpp:83 | src/database_time_series.cpp:77 (C API src/c/database_time_series.cpp:16) | src/lua_runner/database_metadata.cpp (deliberate deviation: shares get_metadata_lua/metadata_to_lua) |
| `db:list_scalar_attributes` | src/lua_runner/db_metadata.cpp:85 | src/database_metadata.cpp:55 | src/lua_runner/database_metadata.cpp |
| `db:list_vector_groups` | src/lua_runner/db_metadata.cpp:86 | src/database_metadata.cpp:67 | src/lua_runner/database_metadata.cpp |
| `db:list_set_groups` | src/lua_runner/db_metadata.cpp:87 | src/database_metadata.cpp:77 | src/lua_runner/database_metadata.cpp |
| `db:list_time_series_groups` | src/lua_runner/db_metadata.cpp:88 | src/database_time_series.cpp:67 (C API src/c/database_time_series.cpp:33) | src/lua_runner/database_metadata.cpp (deliberate deviation: shares list_metadata_lua/metadata_to_lua) |
| `db:create_element` | src/lua_runner/db_write.cpp:298 | src/database_create.cpp:5 | src/lua_runner/database_create.cpp |
| `db:update_element` | src/lua_runner/db_write.cpp:300 | src/database_update.cpp:9 | src/lua_runner/database_update.cpp |
| `db:update_element_by_label` | src/lua_runner/db_write.cpp:301 | src/database_update.cpp:63 | src/lua_runner/database_update.cpp |
| `db:update_relation` | src/lua_runner/db_write.cpp:302 | src/database_update.cpp:71 | src/lua_runner/database_update.cpp |
| `db:update_relation_by_label` | src/lua_runner/db_write.cpp:303 | src/database_update.cpp:119 | src/lua_runner/database_update.cpp |
| `db:update_vector_group` | src/lua_runner/db_write.cpp:304 | src/database_update.cpp:216 | src/lua_runner/database_update.cpp |
| `db:update_vector_group_by_label` | src/lua_runner/db_write.cpp:305 | src/database_update.cpp:227 | src/lua_runner/database_update.cpp |
| `db:update_set_group` | src/lua_runner/db_write.cpp:306 | src/database_update.cpp:241 | src/lua_runner/database_update.cpp |
| `db:update_set_group_by_label` | src/lua_runner/db_write.cpp:307 | src/database_update.cpp:252 | src/lua_runner/database_update.cpp |
| `db:delete_element` | src/lua_runner/db_write.cpp:295 | src/database_delete.cpp:5 | src/lua_runner/database_delete.cpp |
| `db:delete_element_by_label` | src/lua_runner/db_write.cpp:296 | src/database_delete.cpp:17 | src/lua_runner/database_delete.cpp |
| `db:has_time_series_files` | src/lua_runner/db_time_series.cpp:262 | src/database_time_series.cpp:349 | src/lua_runner/database_time_series.cpp |
| `db:read_time_series_group` | src/lua_runner/db_time_series.cpp:264 | src/database_time_series.cpp:97 | src/lua_runner/database_time_series.cpp |
| `db:read_time_series_row` | src/lua_runner/db_time_series.cpp:265 | src/database_time_series.cpp:289 | src/lua_runner/database_time_series.cpp |
| `db:read_time_series_files` | src/lua_runner/db_time_series.cpp:266 | src/database_time_series.cpp:368 | src/lua_runner/database_time_series.cpp |
| `db:update_time_series_group` | src/lua_runner/db_time_series.cpp:268 | src/database_time_series.cpp:141 | src/lua_runner/database_time_series.cpp |
| `db:update_time_series_group_by_label` | src/lua_runner/db_time_series.cpp:269 | src/database_time_series.cpp:215 | src/lua_runner/database_time_series.cpp |
| `db:upsert_time_series_row` | src/lua_runner/db_time_series.cpp:270 | src/database_time_series.cpp:229 | src/lua_runner/database_time_series.cpp |
| `db:upsert_time_series_row_by_label` | src/lua_runner/db_time_series.cpp:271 | src/database_time_series.cpp:275 | src/lua_runner/database_time_series.cpp |
| `db:update_time_series_files` | src/lua_runner/db_time_series.cpp:272 | src/database_time_series.cpp:415 | src/lua_runner/database_time_series.cpp |
| `db:list_time_series_files_columns` | src/lua_runner/db_time_series.cpp:274-276 | src/database_time_series.cpp:355 | src/lua_runner/database_time_series.cpp |
| `db:read_csv` | src/lua_runner/csv.cpp:314-339 | src/csv/csv_read.cpp:72,154 (Reader; no Database method) | src/lua_runner/csv.cpp (unchanged) |
| `db:read_csv_stream` | src/lua_runner/csv.cpp:340-383 | src/csv/csv_read.cpp:72,154 | src/lua_runner/csv.cpp (unchanged) |
| `db:write_csv` | src/lua_runner/csv.cpp:384-401 | src/csv/csv_write.cpp:103 | src/lua_runner/csv.cpp (unchanged) |
| `CsvWriter:write_row` | src/lua_runner/csv.cpp:408 (body csv.cpp:265-301) | src/csv/csv_write.cpp:149 | src/lua_runner/csv.cpp (unchanged) |
| `CsvWriter:close` | src/lua_runner/csv.cpp:410 (body csv.cpp:303-305) | src/csv/csv_write.cpp:164 | src/lua_runner/csv.cpp (unchanged) |
| `db:open_file` | src/lua_runner/binary.cpp:187-201 | src/binary/binary_file.cpp:52 | src/lua_runner/binary.cpp |
| `db:bin_to_csv` | src/lua_runner/binary.cpp:202-207 | src/binary/csv_converter.cpp:128 | src/lua_runner/binary.cpp |
| `db:csv_to_bin` | src/lua_runner/binary.cpp:208-210 | src/binary/csv_converter.cpp:87 | src/lua_runner/binary.cpp |
| `BinaryMetadata:get_unit` | src/lua_runner/binary.cpp:215 | BinaryMetadata::unit field (include/quiver/binary/binary_metadata.h) | src/lua_runner/binary.cpp |
| `BinaryMetadata:get_version` | src/lua_runner/binary.cpp:217 | BinaryMetadata::version field | src/lua_runner/binary.cpp |
| `BinaryMetadata:get_initial_datetime` | src/lua_runner/binary.cpp:219 | src/utils/datetime.h:100 (format_utc) | src/lua_runner/binary.cpp |
| `BinaryMetadata:get_labels` | src/lua_runner/binary.cpp:221 | BinaryMetadata::labels field | src/lua_runner/binary.cpp |
| `BinaryMetadata:get_dimensions` | src/lua_runner/binary.cpp:226 | BinaryMetadata::dimensions field | src/lua_runner/binary.cpp |
| `BinaryMetadata:get_number_of_time_dimensions` | src/lua_runner/binary.cpp:235 | src/binary/binary_metadata.cpp:154 | src/lua_runner/binary.cpp |
| `BinaryMetadata:to_toml` | src/lua_runner/binary.cpp:237 | src/binary/binary_metadata.cpp:298 | src/lua_runner/binary.cpp |
| `BinaryFile:read` | src/lua_runner/binary.cpp:244 | src/binary/binary_file.cpp:131 | src/lua_runner/binary.cpp |
| `BinaryFile:write` | src/lua_runner/binary.cpp:252 | src/binary/binary_file.cpp:160 | src/lua_runner/binary.cpp |
| `BinaryFile:close` | src/lua_runner/binary.cpp:259 | src/binary/binary_file.cpp:114 | src/lua_runner/binary.cpp |
| `BinaryFile:is_open` | src/lua_runner/binary.cpp:261 | src/binary/binary_file.cpp:127 | src/lua_runner/binary.cpp |
| `BinaryFile:get_metadata` | src/lua_runner/binary.cpp:263 | src/binary/binary_file.cpp:310 | src/lua_runner/binary.cpp |
| `BinaryFile:get_file_path` | src/lua_runner/binary.cpp:265 | src/binary/binary_file.cpp:314 | src/lua_runner/binary.cpp |
| `BinaryFile metamethods __add/__sub/__mul/__div/__unm/__band/__bor/__bnot` | src/lua_runner/binary.cpp:269 (bind_expression_operators, binary.cpp:165-176) | src/expression/expression.cpp:18 (Expression(const BinaryFile&)) + operators :98-146, :245-269 | src/lua_runner/expression.cpp (bind_expression registers them on the usertype bind_binary returns) |
| `quiver.metadata` | src/lua_runner/binary.cpp:271 | src/binary/binary_metadata.cpp:177 (from_element) | src/lua_runner/binary.cpp |
| `quiver.metadata_from_toml` | src/lua_runner/binary.cpp:272 | src/binary/binary_metadata.cpp:273 | src/lua_runner/binary.cpp |
| `quiver.metadata_from_element` | src/lua_runner/binary.cpp:275 | src/binary/binary_metadata.cpp:177 | src/lua_runner/binary.cpp |
| `Expression:save` | src/lua_runner/binary.cpp:283 | src/expression/expression.cpp:49 | src/lua_runner/expression.cpp |
| `Expression:metadata` | src/lua_runner/binary.cpp:285 | src/expression/expression.cpp:22 | src/lua_runner/expression.cpp |
| `Expression:aggregate` | src/lua_runner/binary.cpp:287 | src/expression/expression.cpp:26 | src/lua_runner/expression.cpp |
| `Expression:aggregate_agents` | src/lua_runner/binary.cpp:293 | src/expression/expression.cpp:34 | src/lua_runner/expression.cpp |
| `Expression:select_agents` | src/lua_runner/binary.cpp:299 | src/expression/expression.cpp:41 | src/lua_runner/expression.cpp |
| `Expression:rename_agents` | src/lua_runner/binary.cpp:305 | src/expression/expression.cpp:45 | src/lua_runner/expression.cpp |
| `Expression metamethods __add/__sub/__mul/__div/__unm/__band/__bor/__bnot` | src/lua_runner/binary.cpp:319 | src/expression/expression.cpp:98-146, 245-269 | src/lua_runner/expression.cpp |
| `quiver.expression` | src/lua_runner/binary.cpp:321 | src/expression/expression.cpp:18 (C API src/c/expression/expression.cpp:99 quiver_expression_from_file) | src/lua_runner/expression.cpp |
| `quiver.abs` | src/lua_runner/binary.cpp:322 | src/expression/expression.cpp:149 | src/lua_runner/expression.cpp |
| `quiver.sqrt` | src/lua_runner/binary.cpp:323 | src/expression/expression.cpp:152 | src/lua_runner/expression.cpp |
| `quiver.log` | src/lua_runner/binary.cpp:324 | src/expression/expression.cpp:155 | src/lua_runner/expression.cpp |
| `quiver.exp` | src/lua_runner/binary.cpp:325 | src/expression/expression.cpp:158 | src/lua_runner/expression.cpp |
| `quiver.ifelse` | src/lua_runner/binary.cpp:326-331 | src/expression/expression.cpp:162 | src/lua_runner/expression.cpp |
| `quiver.gt` | src/lua_runner/binary.cpp:334 | src/expression/expression.cpp:173 | src/lua_runner/expression.cpp |
| `quiver.lt` | src/lua_runner/binary.cpp:335 | src/expression/expression.cpp:185 | src/lua_runner/expression.cpp |
| `quiver.gte` | src/lua_runner/binary.cpp:336 | src/expression/expression.cpp:197 | src/lua_runner/expression.cpp |
| `quiver.lte` | src/lua_runner/binary.cpp:337 | src/expression/expression.cpp:209 | src/lua_runner/expression.cpp |
| `quiver.eq` | src/lua_runner/binary.cpp:338 | src/expression/expression.cpp:221 | src/lua_runner/expression.cpp |
| `quiver.neq` | src/lua_runner/binary.cpp:339 | src/expression/expression.cpp:233 | src/lua_runner/expression.cpp |

### Helpers

| Helper | Current file | Used by | Proposed home |
|---|---|---|---|
| `RunHandles (struct + path_has_open_writer/add_writer/add_binary_file/close_open_handles)` | src/lua_runner/internal.h:38-54; bodies src/lua_runner/lua_runner.cpp:22-79 | csv.cpp:391,398 (write_csv); binary.cpp:198 (open_file); lua_runner.cpp GcGuard :149-155 | unchanged: internal.h + lua_runner.cpp (users: csv.cpp, binary.cpp, lua_runner.cpp) |
| `to_lua_table (3 overloads)` | src/lua_runner/internal.h:56-87 | db_read.cpp (read_groups_by_id), csv.cpp (read_csv, header_object), binary.cpp (get_labels, file:read), internal.h adapters | internal.h (database_read, csv, binary) |
| `bulk_read_lua<Read>` | src/lua_runner/internal.h:93-97 | db_read.cpp only (9 registrations, db_read.cpp:111-121) | database_read.cpp anonymous namespace (single-file; optional move, src/AGENTS.md:663-665 documents it next to collection_read_lua) |
| `collection_read_lua<Read>` | src/lua_runner/internal.h:99-103 | db_read.cpp:109 (read_element_ids), db_time_series.cpp:274-276 (list_time_series_files_columns) | internal.h (database_read, database_time_series) |
| `is_lua_boolean` | src/lua_runner/internal.h:109-111 | internal.h (lua_cell_as, lua_to_value), db_write.cpp:73 (table_to_element) | internal.h (database_create + internal.h) |
| `lua_cell_as<T>` | src/lua_runner/internal.h:122-134 | binary.cpp (dims, metadata_string, rename_agents), csv.cpp, db_core.cpp:58 (parse_csv_options codes), db_time_series.cpp (update_time_series_files) | internal.h (binary, expression, csv, database_csv_export, database_time_series) |
| `lua_to_value` | src/lua_runner/internal.h:139-156 | csv.cpp (csv_cell_to_string), db_core.cpp:109 (lua_table_to_values), db_time_series.cpp (lua_table_to_value_map), db_write.cpp (table_to_element, columns_to_cpp_rows) | internal.h (csv, database_query, database_time_series, database_create, database_update) |
| `lua_table_to_vector<T>` | src/lua_runner/internal.h:160-169 | binary.cpp (metadata_array, file:write, select_agents), db_write.cpp (table_to_element) | internal.h (binary, expression, database_create) |
| `collect_entries` | src/lua_runner/internal.h:173-177 | db_core.cpp:46,51,56 (parse_csv_options), binary.cpp:311 (rename_agents), option_entries | internal.h (database_csv_export, expression) |
| `lua_type_name` | src/lua_runner/internal.h:181-184 | internal.h only (lua_type_error) | internal.h |
| `lua_type_error` | src/lua_runner/internal.h:187-196 | db_core.cpp:81 (run_in_scope), binary.cpp:114 (to_expression), db_write.cpp:60 (table_to_element), require_table/optional_from_lua/lua_string_key | internal.h (database, expression, database_create) |
| `require_table` | src/lua_runner/internal.h:201-211 | binary.cpp, csv.cpp, db_time_series.cpp, db_write.cpp (table_to_element, collect_group_columns) | internal.h |
| `lua_string_key` | src/lua_runner/internal.h:215-220 | binary.cpp:30, db_time_series.cpp (row map, files), db_write.cpp:56 | internal.h (binary, database_time_series, database_create) |
| `optional_from_lua<T>` | src/lua_runner/internal.h:225-243 | db_core.cpp:117,122,127 (query_*), binary.cpp:196,205,248 (open_file/bin_to_csv/read), binary.cpp:290,296 (aggregate/aggregate_agents) | internal.h (database_query, binary, expression) |
| `option_table` | src/lua_runner/internal.h:247-249 | csv.cpp:208, db_core.cpp:45,50,55 | internal.h (csv, database_csv_export) |
| `option_entries<N>` | src/lua_runner/internal.h:258-281 | binary.cpp:57 (build_metadata_from_lua), csv.cpp:201,229, db_core.cpp:35 | internal.h (binary, csv, database_csv_export) |
| `GroupColumn` | src/lua_runner/internal.h:288-293 | db_write.cpp, db_time_series.cpp | internal.h (database_update, database_time_series) |
| `encode_return_json` | decl src/lua_runner/internal.h:303; def return_json.cpp | lua_runner.cpp (run) | unchanged |
| `table_to_element` | decl src/lua_runner/internal.h:305; def src/lua_runner/db_write.cpp:51-99 | db_write.cpp:164,169,179 (create/update/update_by_label), binary.cpp:276 (quiver.metadata_from_element) | def in database_create.cpp, decl stays in internal.h (users: database_create, database_update, binary) |
| `collect_group_columns` | decl src/lua_runner/internal.h:306; def src/lua_runner/db_write.cpp:101-126 | db_write.cpp:233 (group_rows_from_lua), db_time_series.cpp (time_series_rows_from_lua) | def in database_update.cpp, decl stays in internal.h (users: database_update, database_time_series) |
| `columns_to_cpp_rows` | decl src/lua_runner/internal.h:307-311; def src/lua_runner/db_write.cpp:136-159 | db_write.cpp:242, db_time_series.cpp (time_series_rows_from_lua) | def in database_update.cpp, decl stays in internal.h |
| `resolve_sandboxed_path` | src/lua_runner/path_policy.h:13 / path_policy.cpp:12 | binary.cpp (open_file, bin_to_csv, csv_to_bin, expr:save), csv.cpp (3), db_core.cpp (export/import_csv, validate_migrations); tests/test_sandboxed_path.cpp:1 | unchanged (users: binary, expression, csv, database, database_csv_export, database_csv_import) |
| `string_key (enum_labels key check, pinned text)` | src/lua_runner/db_core.cpp:19-24 (anon) | parse_csv_options only (db_core.cpp:47,52,57) | database_csv_export.cpp anonymous namespace |
| `parse_csv_options` | src/lua_runner/db_core.cpp:29-64 (anon) | db:export_csv (db_core.cpp:173) and db:import_csv (db_core.cpp:184) | def in database_csv_export.cpp, NEW decl in internal.h (shared by database_csv_export + database_csv_import; CSVOptions already visible via quiver/database.h:7 -> options.h) |
| `run_in_scope` | src/lua_runner/db_core.cpp:72-103 (anon) | db:transaction (141-150), db:dry_run (154-163) | database.cpp anonymous namespace |
| `lua_table_to_values` | src/lua_runner/db_core.cpp:105-112 (anon) | query_string_lua/query_integer_lua/query_float_lua | database_query.cpp anonymous namespace |
| `query_string_lua / query_integer_lua / query_float_lua` | src/lua_runner/db_core.cpp:116-129 (anon) | db:query_* registrations db_core.cpp:194-196 | database_query.cpp anonymous namespace |
| `read_scalars_by_id_lua / read_groups_by_id / read_vectors_by_id_lua / read_sets_by_id_lua / read_element_by_id_lua` | src/lua_runner/db_read.cpp:14-104 (anon) | db_read.cpp:123-126 | database_read.cpp anonymous namespace (unchanged) |
| `require_dense_array` | src/lua_runner/db_write.cpp:26-38 (anon) | table_to_element only (db_write.cpp:64) | database_create.cpp anonymous namespace |
| `join_column_names` | src/lua_runner/db_write.cpp:41-47 (anon) | columns_to_cpp_rows only (db_write.cpp:143) | database_update.cpp anonymous namespace |
| `create_element_lua` | src/lua_runner/db_write.cpp:163-166 (anon) | db:create_element (db_write.cpp:298) | database_create.cpp anonymous namespace |
| `update_element_lua / update_element_by_label_lua` | src/lua_runner/db_write.cpp:168-181 (anon) | db_write.cpp:300-301 | database_update.cpp anonymous namespace |
| `relation_target_from_lua / update_relation_lua / update_relation_by_label_lua` | src/lua_runner/db_write.cpp:184-226 (anon) | db_write.cpp:302-303 | database_update.cpp anonymous namespace |
| `group_rows_from_lua + update_{vector,set}_group(_by_label)_lua` | src/lua_runner/db_write.cpp:232-290 (anon) | db_write.cpp:304-307 | database_update.cpp anonymous namespace |
| `lua_data_type_name / metadata_to_lua (Scalar, Group) / list_metadata_lua / get_metadata_lua` | src/lua_runner/db_metadata.cpp:12-74 (anon) | all eight registrations db_metadata.cpp:80-88 | database_metadata.cpp anonymous namespace (unchanged; the reason the two time-series metadata functions stay here) |
| `value_to_lua_object / length_mismatch / lua_table_to_value_map / time_series_rows_from_lua / read_* / update_* / upsert_* time-series wrappers` | src/lua_runner/db_time_series.cpp:19-257 (anon) | db_time_series.cpp:262-276 | database_time_series.cpp anonymous namespace (unchanged) |
| `CsvWriter (named, usertype) + csv_max_integer_key / csv_cell_to_string / csv_row_cells_from_lua / csv_header_from_lua / csv_separator_from_lua / write_csv_options_from_lua / read_csv_options_from_lua / header_object` | src/lua_runner/csv.cpp:26-50, 58-261 (anon), 265-305 | csv.cpp bindings 314-412 (csv_separator_from_lua shared by both decoders, csv.cpp:204,232) | csv.cpp (unchanged) |
| `lua_table_to_dim_map / metadata_string / metadata_array / build_metadata_from_lua / dimension_to_lua` | src/lua_runner/binary.cpp:27-93 (anon) | file:read/write (247,256), quiver.metadata (271), BinaryMetadata:get_dimensions (231) | binary.cpp anonymous namespace; build_metadata_from_lua must stay ABOVE the usertype registrations (binary.cpp:53-55, sync-test pass 2) |
| `is_number / to_expression / binop<Op> / bind_expression_operators<T>` | src/lua_runner/binary.cpp:99-139, 165-176 (anon) | BinaryFile metamethods (269), Expression metamethods (319), quiver.expression/abs/sqrt/log/exp/ifelse/gt..neq (321-339) | expression.cpp anonymous namespace (every user moves there because bind_expression also registers the BinaryFile operators); nothing goes to internal.h |
| `parse_aggregate_op` | src/lua_runner/binary.cpp:142-159 (anon) | Expression:aggregate (289), Expression:aggregate_agents (295) | expression.cpp anonymous namespace |
| `GcGuard (local struct in run())` | src/lua_runner/lua_runner.cpp:149-155 | LuaRunner::run | unchanged |

### Proposed files

| File | Est. lines | Contents |
|---|---|---|
| src/lua_runner/lua_runner.cpp | 179 | Unchanged except that the 7 binder calls (lua_runner.cpp:113-119) become the 13 calls shown in the summary. bind_binary now returns sol::usertype<BinaryFile>, which is passed to bind_expression. RunHandles bodies and run()/GcGuard are untouched. |
| src/lua_runner/internal.h | 320 | Same helpers. The binder declarations (internal.h:295-301) are replaced by bind_database, bind_create, bind_read, bind_update, bind_delete, bind_metadata, bind_time_series, bind_query, bind_csv_export, bind_csv_import (all taking sol::usertype<Database>& bind); bind_csv(sol::state&, sol::usertype<Database>& bind, RunHandles&); sol::usertype<BinaryFile> bind_binary(sol::state&, sol::usertype<Database>& bind, sol::table& ns, RunHandles&) (drops Database&, since only expr:save used it); bind_expression(sol::state&, sol::table& ns, sol::usertype<BinaryFile>&, Database&). Adds one declaration, CSVOptions parse_csv_options(const sol::object&, const std::string&). Optional: bulk_read_lua moves out to database_read.cpp. |
| src/lua_runner/return_json.cpp | 225 | Unchanged (run()'s JSON encoder). |
| src/lua_runner/path_policy.h | 18 | Unchanged (sol2-free; included by tests/test_sandboxed_path.cpp:1). |
| src/lua_runner/path_policy.cpp | 64 | Unchanged (resolve_sandboxed_path; also compiled into quiver_tests, tests/CMakeLists.txt:58). |
| src/lua_runner/database.cpp | 95 | bind_database: is_healthy, current_version, path, validate_migrations (sandboxed); begin_transaction, commit, rollback, in_transaction, transaction; begin_dry_run, end_dry_run, in_dry_run, dry_run; describe, describe_collection, summarize_collection. Anonymous namespace: run_in_scope (from db_core.cpp:72-103). Mirrors src/database.cpp (transactions) plus the C API's src/c/database.cpp (describe). The by-value lambdas need one NOLINT pair. |
| src/lua_runner/database_create.cpp | 95 | bind_create: create_element. Defines table_to_element (declared in internal.h, also used by database_update.cpp and binary.cpp). Anonymous namespace: require_dense_array, create_element_lua. |
| src/lua_runner/database_read.cpp | 135 | bind_read: db_read.cpp as it is today (14 names), plus number_of_elements (moved from db_core.cpp:188). Anonymous namespace: read_scalars_by_id_lua, read_groups_by_id, read_vectors_by_id_lua, read_sets_by_id_lua, read_element_by_id_lua (plus bulk_read_lua if it moves). |
| src/lua_runner/database_update.cpp | 225 | bind_update: update_element(+_by_label), update_relation(+_by_label), update_vector_group(+_by_label), update_set_group(+_by_label). Defines collect_group_columns and columns_to_cpp_rows (declared in internal.h, shared with database_time_series.cpp). Anonymous namespace: join_column_names, update_element*_lua, relation_target_from_lua, update_relation*_lua, group_rows_from_lua, update_{vector,set}_group*_lua. One NOLINT pair. |
| src/lua_runner/database_delete.cpp | 15 | bind_delete: delete_element and delete_element_by_label as &Database:: member pointers. No helpers and no NOLINT. |
| src/lua_runner/database_metadata.cpp | 91 | Rename of db_metadata.cpp (git mv), with bind_metadata unchanged: the 4 get_*_metadata and 4 list_* functions, including the time-series pair. |
| src/lua_runner/database_time_series.cpp | 281 | Rename of db_time_series.cpp (git mv), with bind_time_series unchanged (10 names). |
| src/lua_runner/database_query.cpp | 50 | bind_query: query_string, query_integer, query_float. Anonymous namespace: lua_table_to_values, query_*_lua (from db_core.cpp:105-129). One NOLINT pair. |
| src/lua_runner/database_csv_export.cpp | 75 | bind_csv_export: export_csv (sandbox, then options; db_core.cpp:164-175). Defines parse_csv_options, which is declared in internal.h. Anonymous namespace: string_key. |
| src/lua_runner/database_csv_import.cpp | 25 | bind_csv_import: import_csv (db_core.cpp:176-186), calling parse_csv_options through internal.h. |
| src/lua_runner/csv.cpp | 416 | Unchanged: bind_csv with read_csv, read_csv_stream, write_csv and the CsvWriter usertype. It already mirrors src/csv/; keeping the name means no edits to cmake/Platform.cmake:4, the Dart hook comment or the src/csv/* comments. |
| src/lua_runner/binary.cpp | 180 | bind_binary: open_file, bin_to_csv, csv_to_bin; the BinaryMetadata usertype (7 methods); the BinaryFile usertype (6 methods, no operators); quiver.metadata, metadata_from_toml, metadata_from_element. Returns the BinaryFile usertype. Anonymous namespace: lua_table_to_dim_map, metadata_string, metadata_array, build_metadata_from_lua (kept above the usertypes), dimension_to_lua. Scratch split compiled at 185 lines. |
| src/lua_runner/expression.cpp | 175 | bind_expression: the Expression usertype (save, metadata, aggregate, aggregate_agents, select_agents, rename_agents); bind_expression_operators on both the Expression usertype and the BinaryFile usertype that was passed in; quiver.expression, abs, sqrt, log, exp, ifelse, gt, lt, gte, lte, eq, neq. Anonymous namespace: is_number, to_expression, binop<Op>, parse_aggregate_op, bind_expression_operators<T>. This is where 'BinaryFile should be an expression' gets implemented. Scratch split compiled at 176 lines. |

### Transactions placement

Put them in src/lua_runner/database.cpp, inside bind_database, as the core does (src/database.cpp:319-386 holds begin_transaction, in_transaction, commit, rollback, begin_dry_run, end_dry_run and in_dry_run). Do not create a database_transaction.cpp as the C API does.

**Why the C API splits them out:** every C function is a QUIVER_REQUIRE plus a try/catch wrapper of about 12 lines, so 7 functions fill 80 lines (src/c/database_transaction.cpp:1-80).

**Why Lua does not need to:** in Lua they are 7 member-pointer one-liners (db_core.cpp:137-140, 151-153) plus the transaction and dry_run lambdas over run_in_scope (db_core.cpp:72-103, 141-163), about 45 lines in total. A separate sol2 TU would pay the 4-5 s isolated (about 10 s under load) fixed cost for that.

**Same reasoning for describe:** describe, describe_collection and summarize_collection also go into database.cpp. That follows the C API (src/c/database.cpp:135-163; there is no src/c/database_describe.cpp) rather than the core's database_describe.cpp.

**If an exact one-file-per-core-file mirror is wanted anyway:** database_transaction.cpp and database_describe.cpp would add 2 sol2 TUs, with no other consequence.

### binary.cpp / expression.cpp split

Yes, split binary.cpp into binary.cpp and expression.cpp.

Reasons:
- **It matches the other two layers.** The core has src/binary/*.cpp and src/expression/*.cpp (src/CMakeLists.txt:37-51); the C API has src/c/binary/{binary_file,csv_converter,binary_metadata}.cpp and src/c/expression/expression.cpp (src/CMakeLists.txt:151-154).
- **The C API already wraps files on the expression side:** quiver_expression_from_file is at src/c/expression/expression.cpp:99.
- **It is the only split here that improves wall time.** binary.cpp is the measured long pole: 42.1 s in the last full build against 20.8 s for the next TU, and 15.1-17.6 s isolated. The scratch split compiles in 9.2-9.7 s plus 9.5-10.9 s.

What goes where:
- **binary.cpp:** db:open_file, bin_to_csv, csv_to_bin; the BinaryMetadata usertype (binary.cpp:212-239); the BinaryFile usertype without operators (241-267); quiver.metadata, metadata_from_toml, metadata_from_element (271-277); helpers lua_table_to_dim_map, metadata_string, metadata_array, build_metadata_from_lua and dimension_to_lua (27-93). It returns the BinaryFile usertype.
- **expression.cpp:** the Expression usertype (280-318); the operators for Expression and for BinaryFile (binary.cpp:269 and 319); quiver.expression, abs, sqrt, log, exp, ifelse and the six comparisons (321-339).

**Shared operator helpers:** is_number, to_expression, binop<Op>, parse_aggregate_op and bind_expression_operators<T> (binary.cpp:99-176) all go into expression.cpp's anonymous namespace. None of them goes into internal.h, because once bind_expression registers the BinaryFile metamethods (on the usertype bind_binary returns), every caller is in expression.cpp.

**Why that matters for direction 1:** all sol2 code that touches Expression then sits in one TU. If 'BinaryFile is an expression' is implemented through a sol2 customization point or a converter, it stays local to expression.cpp. A customization visible in some TUs but not others would be a silent ODR violation, and binary.cpp has no Expression push or get (its methods return BinaryMetadata, vectors and strings; binary.cpp:244-266).

**Binary metadata from an element:** quiver.metadata_from_element stays in binary.cpp and reaches table_to_element through internal.h.

### sol2 TU cost

Measured on this machine: MSVC 14.51 Debug with the repo's exact flags from build/compile_commands.json, objects written to the scratchpad. Isolated compiles were run sequentially, 3 runs each.
- **Fixed cost per TU:** a TU containing only `#include "lua_runner/internal.h"` costs 4.1-5.4 s.
- **Existing files:**
  - db_metadata.cpp: 3.5-7.2 s
  - db_read.cpp: 5.0-8.1 s
  - db_core.cpp: 5.6-7.2 s
  - binary.cpp: 15.1-17.6 s
- **Scratch split of binary.cpp:** binary_part 9.2-9.7 s plus expression_part 9.5-10.9 s (both compiled cleanly, objects 26 MB and 25 MB against binary.obj's 41 MB).

From build/.ninja_log, the last full build on 14 cores, with all 9 sol2 TUs starting within 0.3 s of each other:
- lua_runner 12.2 s, return_json 10.3 s, db_metadata 10.5 s, db_time_series 11.1 s, csv 13.0 s, db_read 17.7 s, db_write 18.2 s, db_core 20.8 s, binary 42.1 s.
- Total about 156 s CPU; wall time 42.1 s, set entirely by binary.cpp.
- The near-empty TUs (return_json, db_metadata) show the fixed sol2 cost is about 10 s per TU under a full build's load.

Estimate for the proposal (9 to 15 sol2 TUs, +6):
- **CPU:** about +25-30 s isolated, or about +60 s under a full build's load, so roughly 156 s to 215 s per clean Debug build. This matches the previous milestone's slope (61 s to 223 s CPU going from 1 to 9 TUs, about 20 s per extra TU under load).
- **Wall time:** should drop rather than rise. The long pole was binary.cpp at 42 s; after the split it is about half that (isolated: 17.6 s against max(9.7, 10.9) s). The next longest would be database_update or database_read at about 18 s under load. With 14 cores the 15 Lua TUs mostly run at once, so the Lua layer's wall time should be about 20-25 s.
- **Incremental builds** get cheaper: editing one binder recompiles a 15-225 line TU, not db_core (20.8 s) or binary (42.1 s).
- **Cheapest TUs to cut** if the CPU matters more than exact naming: database_delete (about 15 lines), database_csv_import (about 25), database_query (about 50). Each costs the 4-5 s fixed cost for almost no content.

### References to update

- src/CMakeLists.txt:17-28: replace lua_runner/binary.cpp, csv.cpp and db_*.cpp in QUIVER_SOURCES with database.cpp, database_create.cpp, database_read.cpp, database_update.cpp, database_delete.cpp, database_metadata.cpp, database_time_series.cpp, database_query.cpp, database_csv_export.cpp, database_csv_import.cpp, csv.cpp, binary.cpp, expression.cpp. Keep internal.h, lua_runner.cpp, path_policy.{cpp,h}, return_json.cpp.
- src/lua_runner/internal.h:295-301: binder declarations (13 binders, bind_binary returns sol::usertype<BinaryFile>, new bind_expression, new parse_csv_options declaration).
- src/lua_runner/lua_runner.cpp:113-119: binder calls (13 calls; binary_file_type passed from bind_binary to bind_expression).
- src/AGENTS.md:46-58: lua_runner/ layout block (db_core/db_read/db_write/db_metadata/db_time_series/csv/binary lines, plus the 'one file per domain' wording at :46).
- src/AGENTS.md:265: 'csv.cpp, internal.h, db_write.cpp' becomes database_update.cpp (the collect_group_columns key guard).
- src/AGENTS.md:648-649: 'hands it to the seven binders' becomes thirteen; document the BinaryFile usertype handed from bind_binary to bind_expression.
- src/AGENTS.md:663-665: bulk_read_lua location if it moves; :665 read_groups_by_id (db_read.cpp -> database_read.cpp).
- src/AGENTS.md:667: get_metadata_lua (db_metadata.cpp -> database_metadata.cpp).
- src/AGENTS.md:670: run_in_scope (db_core.cpp -> database.cpp).
- src/AGENTS.md:682: binop<Op> (binary.cpp -> expression.cpp).
- src/AGENTS.md:687: length_mismatch (db_time_series.cpp -> database_time_series.cpp).
- src/AGENTS.md:697: option 'date_time_format' / keys-of-option texts (db_core.cpp -> database_csv_export.cpp).
- src/AGENTS.md:699: target_label text (db_write.cpp -> database_update.cpp).
- src/AGENTS.md:758: parse_csv_options section; optionally add its new home (database_csv_export.cpp, declared in internal.h).
- src/AGENTS.md:999: Expression section 'src/lua_runner/binary.cpp' becomes src/lua_runner/expression.cpp (:929, the Binary section, keeps binary.cpp).
- AGENTS.md:74: 'via sol2 (src/lua_runner/binary.cpp)' becomes binary.cpp + expression.cpp.
- AGENTS.md:743: 'one file per domain' (optional rewording: one file per core database_*.cpp / subsystem).
- bindings/js/src/lua-api.ts:3: maintainer header 'bind_core through bind_binary' becomes 'bind_database through bind_expression'. The sync test does not check this, so it will not fail if missed.
- In-file comments that travel with the code: binary.cpp:53-55 ('stays above bind_binary', still binary.cpp); db_core.cpp:198 ('like the file I/O in binary.cpp', moves to database.cpp and is still true); csv.cpp:310 (same wording, still true).
- Verified unchanged under this proposal (names kept): cmake/Platform.cmake:4, bindings/dart/hook/build.dart:55, bindings/dart/AGENTS.md:65, AGENTS.md:449-450 (return_json.cpp, csv.cpp), AGENTS.md:90/157/223/904, src/csv/csv_read.h:5,12, src/csv/csv_write.h:4,12,18,22,55, src/csv/csv_write.cpp:30-31,56, tests/CMakeLists.txt:58,69, tests/test_sandboxed_path.cpp:1, tests/test_database_ui_metadata.cpp:59, tests/AGENTS.md:61,192, bindings/julia/AGENTS.md:160, bindings/js/AGENTS.md:37.
- bindings/js/test/lua-api-sync.test.ts:10-16 globs src/lua_runner/ recursively and names no files, so it needs no edit. It does constrain the code: see risks.
- No matches in .github/, docs/, scripts/ or CHANGELOG.md (git grep excluding .planning/.gsd). scripts/tidy.bat:19 only excludes src/binary/, so the new src/lua_runner/expression.cpp is still linted.

### Risks

- Sync-test pass 1 (bindings/js/test/lua-api-sync.test.ts:25-27,73): every binder must keep naming its parameters `bind` and `ns`. bind_expression's quiver-table parameter must be called `ns`. The BinaryFile usertype passed into it may only take `[sol::meta_function::...] =` assignments; a `binary_file_type.set_function(` call would trip the set_function count assertion.
- Sync-test pass 2 (lua-api-sync.test.ts:36-53) resets per file and treats any bare `"name",` line after a new_usertype as one of that usertype's methods. build_metadata_from_lua and its wrapped key list must stay above the usertype registrations in binary.cpp (binary.cpp:53-55). No new file may put bare quoted lines after a new_usertype.
- Binder order becomes partly load-bearing in a new way: bind_expression needs the BinaryFile usertype from bind_binary. Passing it as a return value makes the compiler enforce the order. Looking it up from the state (e.g. state["BinaryFile"]) would hide the dependency and is untested in this sol2 version.
- Coupling with direction 1: a sol2 customization (check/get/interop) that lets a BinaryFile stand in for an Expression has to be visible in every TU that instantiates sol2 stack code for Expression. Keep all Expression-touching sol2 code in expression.cpp, including the BinaryFile metamethods. Also, typed sol2 parameters surface sol2's own argument text. That is the exact thing src/AGENTS.md:283-293 and csv.cpp:345-349 avoid, and it would replace the C8 Pattern 1 message ('Cannot <op>: operand must be an expression or a binary file, got <type>', binary.cpp:114), so direction 1 needs an explicit decision on the error text.
- Places where the proposal deliberately does not mirror the core exactly: (1) get_time_series_metadata and list_time_series_groups stay in database_metadata.cpp, while the core and C API have them in database_time_series (src/database_time_series.cpp:67,77; src/c/database_time_series.cpp:16,33). Moving them would mean exporting metadata_to_lua, list_metadata_lua and get_metadata_lua through internal.h (about 45 lines). (2) describe is folded into database.cpp, following the C API. (3) Transactions stay in database.cpp, following the core. If the user wants the literal list they wrote, that adds up to 2 TUs and those helpers move into internal.h.
- Compile cost: +6 sol2 TUs, about +25-30 s CPU isolated or about +60 s under a full build's load per clean Debug build. The smallest TUs (database_delete about 15 lines, database_csv_import about 25, database_query about 50) are almost all fixed cost. Wall time should still fall, from 42 s to about 20-25 s, because of the binary/expression split.
- Basename ambiguity: database_read.cpp (and the others) would exist in src/, src/c/ and src/lua_runner/. CMake/Ninja object paths are per-directory, so the build is unaffected (quiver.dir/database_read.cpp.obj vs quiver.dir/lua_runner/database_read.cpp.obj), but docs and grep must use full paths.
- Blame history: use git mv for the rename-only files (db_metadata -> database_metadata, db_time_series -> database_time_series, db_read -> database_read). The split files (db_core, db_write, binary) lose straight blame continuity; `git log -C`/`--follow` recovers part of it.
- Docs can drift silently: about 12 file-name mentions in src/AGENTS.md and the lua-api.ts:3 header are not checked by the sync test, which compares binding names only, so stale file names will not fail CI.
- Must stay a pure move: every Pattern 1 string the tests pin stays byte-identical. Each new TU with by-value sol2 parameters needs its own NOLINTBEGIN/END pair (src/AGENTS.md:659-660). Every file stays at or under about 450 lines (src/AGENTS.md:660); the largest is csv.cpp at 416. New files must be added to QUIVER_SOURCES so they inherit the target-wide SOL_* defines and /bigobj (src/CMakeLists.txt:78-93); a sol2 TU built with different SOL_* defines would be an ODR hazard.

## Designs (all under the directive: one abstract expression type as the only parameter of every op)

### abstract-base-lifetime: Stateless AbstractExpression base: a BinaryFile stands for the file at its path, never for the open handle

- **Level:** cpp-and-lua
- **Keeps Pattern 1:** false
- **LOC:** About +170 / -140 overall: - src, roughly +20 net:   - new 50-line abstract_expression.h   - expression.h loses its 47-line friend block   - expression.cpp roughly flat: the four make_* helpers offset the shorter operator bodies   - binary.cpp roughly flat: 6 trait lines and 6 duplicated method name pairs replace is_number, to_expression and the sol::object binop - tests: about +70 / -25 - docs (AGENTS.md, src/AGENTS.md, lua-api.ts, CHANGELOG): about +50 / -15

**Mechanism**

1. NEW TYPE. A new abstract base, quiver::AbstractExpression, in include/quiver/expression/abstract_expression.h.
   - It holds no state and has a public virtual destructor.
   - Its copy and move operations are protected. It is abstract, so it can never be a by-value object, and it cannot be sliced or assigned through a base reference.
   - It has one pure virtual: `std::shared_ptr<ExpressionNode> node() const`.
   - Everything else is non-virtual on the base and built on node(): metadata(), save(), aggregate(), aggregate_agents(), select_agents(), rename_agents(). metadata() returns by value.

2. THE TWO KINDS. Exactly two final classes derive from it.
   - Expression keeps its shared_ptr node_ and returns it from node().
   - BinaryFile returns a new `std::make_shared<ExpressionFile>(get_file_path())` from node(). That is word for word today's `Expression(const BinaryFile&)` body (src/expression/expression.cpp:18).

3. FREE FUNCTIONS. Every operator and free function takes `const AbstractExpression&` and returns Expression. They use the public node() instead of friend access to node_, so the friend block (expression.h:42-89) is deleted.
   - The implicit `Expression(const BinaryFile&)` becomes `explicit Expression(const AbstractExpression&)`, a snapshot conversion.
   - The C API (src/c/expression/expression.cpp:103) and the tests use direct-init, so they still compile.

4. INCLUDE CYCLE. expression_node.h:4 includes binary_file.h, and binary_file.h must now include the base header. To break the cycle, hoist `ExpressionAggregate::Operation` to a namespace-scope `enum class AggregateOperation`. Keep `using Operation = AggregateOperation;` inside ExpressionAggregate, so existing callers such as src/c/expression/expression.cpp:77-88 still compile.

5. OWNERSHIP: THE NODE NEVER REFERENCES THE CALLER'S HANDLE. This is deliberate. A node that points at the user's handle breaks in four ways:
   - (a) Write mode. A writer's stream is ios::out only (binary_file.cpp:99-100). save() also opens every collected input with 'r' and closes all of them in CloseOnExit (expression.cpp:61-73). So it would silently close the user's writer, or reader, and later `write`/`read` calls would throw "File is not open".
   - (b) Closed handles. save() would toggle the user's is_open() state.
   - (c) Lifetime. The C API's `quiver_binary_file_close` deletes the handle (src/c/binary/binary_file.cpp:76-79). Julia closes right after wrapping (test_expression.jl:140-151). Lua's RunHandles closes every handle at run() exit while expressions survive in globals (lua_runner.cpp:57-79, 151). A raw pointer would dangle. A shared_ptr pin would force every BinaryFile onto the heap, and the C API holds it by value (src/c/internal.h:32).
   - (d) Moved-from files. A moved-from file has impl_ == nullptr (binary_file.cpp:49).
   Building the leaf from the path keeps every existing guarantee: an expression outlives its handle, save() uses private readers, and a live writer still fails at save() through the write registry (binary_file.cpp:67-69), which is the pinned text.

6. NO CACHED LEAF. Caching would go stale after a reopen with 'w' and new metadata, and it would duplicate entries in collect_input_files. To parse each TOML once, call node() once per operand into locals (helpers make_binary/make_unary). That also keeps errors left-first.

7. BINARYFILE AS A DERIVED CLASS IS SOUND. The base has no data, so the defaulted moves (binary_file.cpp:49-50) and the ~Impl cleanup (:32-42) are unaffected. The vptr is not moved, so a moved-from file keeps its dynamic type. `final` closes the hierarchy. sol2 never stores the base: Expression is stored by value with its own destructor, and BinaryFile through shared_ptr as a unique usertype (usertype_storage.hpp:1074-1088). The base is reached only through a pointer cast (stack_get_unqualified.hpp:898-916).

8. LUA (the directive's sol2 type check).
   - Use sol2's compile-time bases traits: SOL_BASE_CLASSES(BinaryFile, AbstractExpression), SOL_BASE_CLASSES(Expression, AbstractExpression) and SOL_DERIVED_CLASSES(AbstractExpression, BinaryFile, Expression) (forward.hpp:248-265). With these, sol2's checker accepts either userdata for a `const AbstractExpression&` parameter (stack_check_unqualified.hpp:540-559).
   - Do not use the runtime `sol::base_classes` tag. update_bases calls change_indexing (usertype_storage.hpp:376-400, 559-584), which replaces each metatable's fast `__index` table (:1137) with a C closure. That adds a cost to every f:read/f:write. The probe confirmed this: __index is `table` with the traits and `function` with the tag.
   - Specialize `sol::is_automagical` to false for both kinds (usertype.rst:295-305). Otherwise sol2 auto-registers __eq/__lt/__le from the C++ comparison operators (usertype_core.hpp:127-150), which return an Expression. Lua coerces that to true, so `f1 == f2` would become true; the probe showed exactly that.
   - Operators: `sol::overload` of (AE, AE), (AE, double) and (double, AE).
   - Unary metamethods take one typed parameter. Lua passes the operand twice and sol2 reads only the first.
   - quiver.* functions take typed `const AbstractExpression&`.
   - The six expression methods are lambdas with `const AbstractExpression& self`, listed in both new_usertype calls. sol2 calls base-member lookup "undocumented, unsupported" (usertype.rst:288), and the sync test reads method names per usertype.
   - Delete is_number, to_expression and the sol::object binop (binary.cpp:99-139).
   - RunHandles and db:open_file are unchanged: it still returns shared_ptr<BinaryFile>, and RunHandles still holds a weak_ptr.

9. VERIFIED IN A STANDALONE MOCK. Built with MSVC and Quiver's exact SOL_* defines: scratchpad/ownprobe/probe.cpp → traits.exe, tag.exe, magic.exe.

**Code sketch**

```cpp
// ============ include/quiver/expression/abstract_expression.h (NEW) ============
#ifndef QUIVER_ABSTRACT_EXPRESSION_H
#define QUIVER_ABSTRACT_EXPRESSION_H

#include "../binary/binary_metadata.h"
#include "../export.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

class Expression;
class ExpressionNode;

// Hoisted out of ExpressionAggregate: this header cannot include expression_node.h (which includes
// binary_file.h, which includes this one). ExpressionAggregate::Operation is now an alias of it.
enum class AggregateOperation { Sum, Mean, Min, Max, Percentile };

// The one expression type. Two kinds, both final: Expression (a lazy DAG) and BinaryFile (a .qvr on
// disk). Stateless; a kind only supplies its DAG root. Every operation, free or member, takes this.
class QUIVER_API AbstractExpression {
public:
    virtual ~AbstractExpression() = default;

    // The DAG root. Expression: its own shared node. BinaryFile: a new leaf over its path.
    virtual std::shared_ptr<ExpressionNode> node() const = 0;

    // By value: a BinaryFile's root is a temporary, so a reference into it would dangle.
    BinaryMetadata metadata() const;
    void save(const std::string& path) const;
    Expression aggregate(
        const std::string& dimension,
        AggregateOperation operation,
        std::optional<double> parameter = std::nullopt
    ) const;
    Expression aggregate_agents(AggregateOperation operation, std::optional<double> parameter = std::nullopt) const;
    Expression select_agents(const std::vector<std::string>& labels) const;
    Expression rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const;

protected:
    // Protected: no slicing assignment through a base reference; the base is never an object.
    AbstractExpression() = default;
    AbstractExpression(const AbstractExpression&) = default;
    AbstractExpression(AbstractExpression&&) noexcept = default;
    AbstractExpression& operator=(const AbstractExpression&) = default;
    AbstractExpression& operator=(AbstractExpression&&) noexcept = default;
};

}  // namespace quiver

#endif  // QUIVER_ABSTRACT_EXPRESSION_H

// ============ include/quiver/binary/binary_file.h (changed part) ============
#include "../expression/abstract_expression.h"

class QUIVER_API BinaryFile final : public AbstractExpression {
public:
    explicit BinaryFile(const std::string& file_path);
    ~BinaryFile() override;
    BinaryFile(const BinaryFile&) = delete;
    BinaryFile& operator=(const BinaryFile&) = delete;
    BinaryFile(BinaryFile&& other) noexcept;             // still `= default` in binary_file.cpp
    BinaryFile& operator=(BinaryFile&& other) noexcept;  // ~Impl still flushes + unregisters

    // As an expression this is the file at get_file_path(), not this handle: a new ExpressionFile
    // with its own reader, opened only inside save(). An expression therefore never opens, closes or
    // reads through this handle, and it outlives it (close(), C API delete, Lua run() exit). A live
    // writer on the same path still fails save() through the write registry. Not on a moved-from file.
    std::shared_ptr<ExpressionNode> node() const override;

    // File-specific; not on the base.
    void open(char mode, const std::optional<BinaryMetadata>& metadata = {});
    void close();
    bool is_open() const;
    std::vector<double> read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls = false);
    void write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);
    const BinaryMetadata& get_metadata() const;  // the handle's metadata (empty until opened)
    const std::string& get_file_path() const;
    // ... private part unchanged
};

// ============ src/expression/expression_file.cpp (added; keeps binary_file.cpp expression-free) ============
std::shared_ptr<ExpressionNode> BinaryFile::node() const {
    return std::make_shared<ExpressionFile>(get_file_path());  // today's Expression(const BinaryFile&) body
}

// ============ include/quiver/expression/expression_node.h ============
class QUIVER_API ExpressionAggregate final : public ExpressionNode {
public:
    using Operation = AggregateOperation;  // was a nested enum; src/c/expression/expression.cpp:77-88 still compiles
    // ...

// ============ include/quiver/expression/expression.h ============
class QUIVER_API Expression final : public AbstractExpression {
public:
    explicit Expression(std::shared_ptr<ExpressionNode> node);
    // A concrete snapshot of any expression (quiver.expression, Julia Expression(file),
    // quiver_expression_from_file). For a BinaryFile it parses the TOML now.
    explicit Expression(const AbstractExpression& expression);

    std::shared_ptr<ExpressionNode> node() const override;

private:
    std::shared_ptr<ExpressionNode> node_;  // the 47-line friend block is gone: operators use node()
};

QUIVER_API Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator+(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const AbstractExpression& rhs);
// - * / > < >= <= == != && || : the same three shapes each
QUIVER_API Expression operator-(const AbstractExpression& operand);
QUIVER_API Expression operator!(const AbstractExpression& operand);
QUIVER_API Expression abs(const AbstractExpression& operand);  // sqrt, log, exp alike
QUIVER_API Expression ifelse(
    const AbstractExpression& condition,
    const AbstractExpression& then_value,
    const AbstractExpression& else_value
);

// ============ src/expression/expression.cpp ============
namespace {

// One node() per operand: a BinaryFile builds a new leaf (and parses its TOML) on every call.
// Locals make the left operand's error surface first.
Expression make_binary(ExpressionBinary::Operation op, const AbstractExpression& lhs, const AbstractExpression& rhs) {
    auto l = lhs.node();
    auto r = rhs.node();
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(l), std::move(r)));
}

Expression make_binary(ExpressionBinary::Operation op, const AbstractExpression& lhs, double rhs) {
    auto l = lhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(rhs, l->metadata());
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(l), std::move(scalar)));
}

Expression make_binary(ExpressionBinary::Operation op, double lhs, const AbstractExpression& rhs) {
    auto r = rhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(lhs, r->metadata());
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(scalar), std::move(r)));
}

Expression make_unary(ExpressionUnary::Operation op, const AbstractExpression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(op, operand.node()));
}

}  // namespace

Expression::Expression(std::shared_ptr<ExpressionNode> node) : node_(std::move(node)) {}

Expression::Expression(const AbstractExpression& expression) : node_(expression.node()) {}

std::shared_ptr<ExpressionNode> Expression::node() const {
    return node_;
}

BinaryMetadata AbstractExpression::metadata() const {
    return node()->metadata();
}

void AbstractExpression::save(const std::string& path) const {
    const auto root = node();  // owns a BinaryFile's leaf, and so the readers below, for the whole save
    std::vector<BinaryFile*> input_files;  // the leaves' private readers, never a caller's handle
    root->collect_input_files(input_files);
    // ... today's body (collision guard, CloseOnExit, open('r'), writer loop) with node_-> -> root->
}

Expression AbstractExpression::aggregate(
    const std::string& dimension,
    AggregateOperation operation,
    std::optional<double> parameter
) const {
    return Expression(std::make_shared<ExpressionAggregate>(operation, node(), dimension, parameter));
}

Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return make_binary(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(const AbstractExpression& lhs, double rhs) {
    return make_binary(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(double lhs, const AbstractExpression& rhs) {
    return make_binary(ExpressionBinary::Operation::Add, lhs, rhs);
}

Expression abs(const AbstractExpression& operand) {
    return make_unary(ExpressionUnary::Operation::Abs, operand);
}

Expression ifelse(
    const AbstractExpression& condition,
    const AbstractExpression& then_value,
    const AbstractExpression& else_value
) {
    auto c = condition.node();
    auto t = then_value.node();
    auto e = else_value.node();
    return Expression(std::make_shared<ExpressionTernary>(
        ExpressionTernary::Operation::IfElse, std::move(c), std::move(t), std::move(e)
    ));
}

// ============ src/lua_runner/binary.cpp ============
#include <sol/sol.hpp>

// sol2 accepts a BinaryFile or an Expression userdata for a `const AbstractExpression&` parameter.
// This is the trait form of sol2's bases, not the sol::base_classes tag: the tag swaps every backing
// metatable's __index table for a C closure (usertype_storage.hpp change_indexing), which costs
// something on every f:read / f:write. Every TU that binds these types must see these (only this one does).
SOL_BASE_CLASSES(quiver::BinaryFile, quiver::AbstractExpression);
SOL_BASE_CLASSES(quiver::Expression, quiver::AbstractExpression);
SOL_DERIVED_CLASSES(quiver::AbstractExpression, quiver::BinaryFile, quiver::Expression);

// No automatic __eq/__lt/__le. The C++ comparisons return an Expression, which Lua coerces to true,
// so `f1 == f2` would build an expression and answer true. Keep userdata identity, as BinaryFile had.
namespace sol {
template <>
struct is_automagical<quiver::BinaryFile> : std::false_type {};
template <>
struct is_automagical<quiver::Expression> : std::false_type {};
}  // namespace sol

namespace quiver::lua_internal {
namespace {

// A number may sit on either side; sol2 picks the candidate whose parameter types match.
template <typename Op>
auto binop() {
    return sol::overload(
        [](const AbstractExpression& lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        [](const AbstractExpression& lhs, double rhs) { return Op{}(lhs, rhs); },
        [](double lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); }
    );
}

template <typename T>
void bind_expression_operators(sol::usertype<T>& type) {
    type[sol::meta_function::addition] = binop<std::plus<>>();
    type[sol::meta_function::subtraction] = binop<std::minus<>>();
    type[sol::meta_function::multiplication] = binop<std::multiplies<>>();
    type[sol::meta_function::division] = binop<std::divides<>>();
    // Lua passes a unary operand twice (lvm.c OP_UNM / OP_BNOT); sol2 reads only the first.
    type[sol::meta_function::unary_minus] = [](const AbstractExpression& operand) { return -operand; };
    type[sol::meta_function::bitwise_and] = binop<std::logical_and<>>();
    type[sol::meta_function::bitwise_or] = binop<std::logical_or<>>();
    type[sol::meta_function::bitwise_not] = [](const AbstractExpression& operand) { return !operand; };
}

}  // namespace

void bind_binary(sol::state& state, sol::usertype<Database>& bind, sol::table& ns, Database& db, RunHandles& handles) {
    // db:open_file / bin_to_csv / csv_to_bin unchanged; open_file still returns std::shared_ptr<BinaryFile>
    // and registers it in RunHandles.

    // The expression methods; self is the abstract kind. Defined here, after the last bind.set_function
    // and before the first new_usertype, so the sync test cannot attribute a wrapped argument line to a
    // usertype. Listed in both usertypes below, because sol2's base-member lookup is "unsupported".
    auto save = [&db](const AbstractExpression& self, const std::string& path) {
        self.save(resolve_sandboxed_path(db, "save", path));
    };
    auto metadata = [](const AbstractExpression& self) { return self.metadata(); };
    auto aggregate = [](const AbstractExpression& self,
                        const std::string& dimension,
                        const std::string& op,
                        const sol::object& parameter) {
        const auto operation = parse_aggregate_op(op, "aggregate");
        const auto value = optional_from_lua<double>(parameter, "aggregate", "parameter", "a number");
        return self.aggregate(dimension, operation, value);
    };
    // aggregate_agents, select_agents, rename_agents: today's bodies with `const AbstractExpression& self`.

    // state.new_usertype<BinaryMetadata>(...) unchanged

    auto binary_file_type = state.new_usertype<BinaryFile>(
        "BinaryFile",
        sol::no_constructor,
        // File-specific: self stays BinaryFile&, so expr:read is nil.
        "read",
        [](BinaryFile& self, const sol::object& dims, const sol::object& allow_nulls, sol::this_state s) {
            sol::state_view lua(s);
            const auto coordinates = lua_table_to_dim_map(dims, "read");
            const bool nulls = optional_from_lua<bool>(allow_nulls, "read", "allow_nulls", "a boolean").value_or(false);
            return to_lua_table(lua, self.read(coordinates, nulls));
        },
        // "write", "close", "is_open", "get_metadata", "get_file_path": unchanged
        "save",
        save,
        "metadata",
        metadata,
        "aggregate",
        aggregate,
        "aggregate_agents",
        aggregate_agents,
        "select_agents",
        select_agents,
        "rename_agents",
        rename_agents
    );
    bind_expression_operators(binary_file_type);

    auto expression_type = state.new_usertype<Expression>(
        "Expression",
        sol::no_constructor,
        "save",
        save,
        "metadata",
        metadata,
        "aggregate",
        aggregate,
        "aggregate_agents",
        aggregate_agents,
        "select_agents",
        select_agents,
        "rename_agents",
        rename_agents
    );
    bind_expression_operators(expression_type);

    ns.set_function("expression", [](const AbstractExpression& operand) { return Expression(operand); });
    ns.set_function("abs", [](const AbstractExpression& operand) { return quiver::abs(operand); });
    ns.set_function("sqrt", [](const AbstractExpression& operand) { return quiver::sqrt(operand); });
    ns.set_function("log", [](const AbstractExpression& operand) { return quiver::log(operand); });
    ns.set_function("exp", [](const AbstractExpression& operand) { return quiver::exp(operand); });
    ns.set_function(
        "ifelse",
        [](const AbstractExpression& condition, const AbstractExpression& then_value, const AbstractExpression& else_value) {
            return quiver::ifelse(condition, then_value, else_value);
        }
    );
    ns.set_function("gt", binop<std::greater<>>());
    // lt, gte, lte, eq, neq alike
}

}  // namespace quiver::lua_internal
// Deleted: is_number, to_expression and the sol::object binop (binary.cpp:99-139).
```

**Wrong-type error text**

The host's LuaRunner::run throws these (lua_runner.cpp:130-135 adds the prefix; sol2's traceback handler appends "\nstack traceback:..."). The texts were checked in the MSVC mock, whose namespace is `q`; the real code says `quiver`. GCC and Clang spell the demangled signature their own way.

quiver.abs(42):
`Failed to run Lua script: [string "return quiver.abs(42)"]:1: stack index 1, expected userdata, received number: value is not a valid userdata (bad argument into 'quiver::Expression(const quiver::AbstractExpression&)')`

Related cases:
- `quiver.abs()` gives `... received no value: value is not a valid userdata ...`
- `quiver.abs(md)` gives `... received sol.quiver::BinaryMetadata: value at this index does not properly reflect the desired type ...`
- `quiver.ifelse(f, f, 'x')` gives `stack index 3, expected userdata, received string: ... (bad argument into 'quiver::Expression(const quiver::AbstractExpression&, const quiver::AbstractExpression&, const quiver::AbstractExpression&)')`

file + "x":
`Failed to run Lua script: [string "<first line of the script>"]:1: sol: no matching function call takes this number of arguments and the specified types`

`"x" + file` gives the same text with no `[string ...]:N:` prefix. In that case the string library's C `__add` is the caller, so luaL_where has no Lua position to report. `quiver.gt(1, 2)` gives the same no-match text.

**Lua behaviour**

What changes for scripts:

- **Files take the expression methods directly.** A BinaryFile handle accepts every expression method without `quiver.expression`: `f:aggregate('row','sum')`, `f:aggregate_agents('mean')`, `f:select_agents{...}`, `f:rename_agents{...}`, `f:save('copy')` and `f:metadata()`. That makes "operate on files directly" (AGENTS.md:821-822) and lua-api.ts:884 ("Mirrors the Julia surface") true. Julia already allows the same on Binary.File (expression.jl:224-239, 265-266).
- **Operators and quiver.* functions accept a file or an expression, as before.** This covers `+ - * /`, unary `-`, `& | ~`, `quiver.abs/sqrt/log/exp/ifelse/gt..neq`. Numbers on either side of a binary operator or comparison also work as before.
- **`quiver.expression(x)` stays.** It returns a concrete Expression snapshot, mirroring Julia's `Expression(file)` and the C function `quiver_expression_from_file`.
- **Lifetime is exactly today's: an expression never touches the handle it came from.**
  - `local r = db:open_file('a','r'); (r*2):save('b')` leaves `r` open and readable.
  - An expression kept in a global still saves in a later `run()`, after RunHandles has closed its source handle.
  - `w:save('x')` or `(w*2):save('x')` on a live writer throws `Cannot open_file: file is already open for writing: <canonical>`.
  - `f:save(<its own path>)` throws `Cannot save: output path collides with input file '<path>'`.
- **Wrong-type operands now produce sol2's own error text** (see `wrong_type_error_text`).
- **Comparing two expressions with `==` or `<` no longer silently returns true.**
  - Today `==` builds an Expression, which Lua reads as true. Now `e1 == e2` compares userdata identity.
  - `e1 < e2` now raises `attempt to compare two sol.quiver::Expression values` (mock showed `sol.q::Expression`).
  - `f1 == f2` stays an identity comparison, as today.
- **Binary operators and comparisons now reject extra arguments; unary functions still ignore them.**
  - `quiver.gt(f, 1, 99)` now errors, because `sol::overload` requires the exact number of arguments.
  - `quiver.abs(f, 99)` still works.

**Cross-layer impact**

**C++ core:** this is where the abstract type lives, along with every signature change.

**C API:** no source changes; it only needs a rebuild.
- The handles stay opaque structs that hold the concrete objects by value (src/c/internal.h:32-40).
- `quiver_expression_from_file` remains the single bridge. `quiver::Expression(file->binary_file)` now resolves to the explicit `Expression(const AbstractExpression&)` (src/c/expression/expression.cpp:103).
- `from_c` keeps compiling because `ExpressionAggregate::Operation` is now an alias for the hoisted enum.
- C has no inheritance, so adding a file-taking variant of every `quiver_expression_*` function would double the surface for no gain.

**Julia:** unchanged. It goes through the C API, and `close!` after `Expression(file)` stays safe because the expression never references the handle.
- Optional follow-up, local to Julia: declare `abstract type AbstractExpression end` in Quiver.jl before `include("binary/Binary.jl")`, make `File <: AbstractExpression` and `Expression <: AbstractExpression`, and add `_expr(f::File) = Expression(f)`.
- That would collapse the 97 forwarding methods in expression.jl and give Julia the same one-type surface.
- It is not required for correctness.

**Dart, Python, JS:** unaffected; they do not expose binary or expression (AGENTS.md design decision).

**Lua:** the second half of the change, covered in `lua_behavior` and `mechanism`.

**Interaction with direction 2 (splitting the lua_runner/ files):**
- The SOL_BASE_CLASSES / SOL_DERIVED_CLASSES / is_automagical specializations must be visible in every translation unit that instantiates sol2 for these types. Today that is only binary.cpp.
- If the split puts BinaryFile and Expression in separate files, move those six lines into a small lua_runner header and include it from both files. Otherwise the program is ill-formed (ODR violation), with no diagnostic required.

**Tests and docs impact**

Tests:
- **tests/test_lua_expression.cpp:555-582.** Rewrite OperandErrorsNameTheOperation and OperandErrorsReportTheLeftmostBadOperand (13 expectations) as one test. Assert on substrings that do not depend on the compiler:
  - "stack index 1, expected userdata, received number" (abs / expression)
  - "stack index 3, expected userdata, received string" (ifelse)
  - "sol: no matching function call takes this number of arguments" (operators, gt)
  - Never assert on the demangled signature.
- **New Lua tests:**
  - FileMethodsWithoutWrapping: fa:aggregate, aggregate_agents, select_agents, rename_agents, save, metadata.
  - SaveLeavesTheSourceHandleOpen: `(r*2):save(..)`, then `r:is_open()` and `r:read{}` still work.
  - ExpressionOutlivesRunHandles: run 1 sets `e = db:open_file(..)*2`; run 2 calls `e:save(..)`.
  - WriterSaveHitsWriteRegistry: `w:save(..)` throws "Cannot open_file: file is already open for writing".
  - ComparisonIsIdentity: `e1 == e2` is false; `e1 < e2` raises.
  - Arity: `quiver.gt(f, 1, 99)` raises.
- **Existing tests that already pin "a file works where an expression does" stay as they are:** FilePlusFile, IfElse, ComparisonFreeFunctions, LogicalOperators, OperatorMetamethodsOnFileAndExpression.
- **tests/test_expression.cpp:1127.** Rename ImplicitConversionFromBinaryFile to BinaryFileIsAnExpression and add:
  - a.aggregate(..).save, abs(a), a.save(copy)
  - `(a * 2.0).save` leaves `a.is_open()` true
  - `a.save(path_a)` collision
- **Unchanged:** the direct-init `Expression e(a)` / `Expression(a)` uses and SaveFailsWhenInputIsOpenForWriting (:1104-1122) compile and pass as they are. The C API and Julia suites are also unchanged.
- **bindings/js/test/lua-api-sync.test.ts:** passes with no edit. The six names appear as bare lines in both new_usertype lists, Expression keeps at least one method, and coverage is receiver-agnostic.

Docs:
- **bindings/js/src/lua-api.ts:879-919:** show `f:aggregate(...)` etc. on a file; fix the :908 comment; mention that operand type errors are sol2's text.
- **AGENTS.md:**
  - Add a Design Decision covering: one abstract expression type with two final kinds; a BinaryFile is the file at its path, never the handle; and the documented exception to "Error Messages" for Lua expression operands.
  - Fix the expression paragraph at :821-822.
  - Update the cross-layer binary table.
- **src/AGENTS.md:**
  - :682-686 (binop / to_expression)
  - :1009 (the implicit ctor)
  - :1021 (it says ExpressionFile "Caches an open BinaryFile", which is false)
  - :284-293 (add the expression exception to the typed-parameter stance)
- **CHANGELOG.md [0.13.0]:**
  - Rewrite the :81-84 entry. It is unreleased; v0.12.9 shipped "Cannot build expression: ...".
  - Add BREAKING C++ entries: explicit ctor, metadata() by value, ABI.
  - Add the Lua additions (file methods) and the ==/< change.

**Breaking changes**

- C++ (BREAKING): Expression(const BinaryFile&) is replaced by explicit Expression(const AbstractExpression&). Copy-initialization (`Expression e = file;`) no longer compiles. Write `Expression e(file)`, or pass the file straight to any operation.
- C++ (BREAKING): Expression::metadata() now returns BinaryMetadata by value, not `const BinaryMetadata&`. The reason is that a BinaryFile's node is a temporary. Code that kept a reference into a temporary must hold the value instead.
- C++ (BREAKING, ABI): BinaryFile and Expression gain a vptr, and every free operator and function now takes `const AbstractExpression&`, which changes the exported symbols. C++ consumers must recompile. C API consumers are unaffected because its handles are opaque.
- C++ (source-compatible): ExpressionAggregate::Operation is now an alias of the namespace-scope AggregateOperation.
- C++ (new meaning): `binary_file == other`, `!binary_file` and `file < x` now compile and build element-wise Expressions. Before, they did not compile.
- C++: calling `file.aggregate(...)`, `file.save(...)` or another base method requires including quiver/expression/expression.h, because Expression is incomplete in binary_file.h.
- Lua (BREAKING): a wrong-type expression operand now raises sol2's text (`stack index N, expected userdata, received <type> ...` or `sol: no matching function call ...`). The previous per-operation text, `Cannot <op>: operand must be an expression or a binary file, got <type>`, is gone. That text was never released: v0.12.9 shipped `Cannot build expression: ...`, so the CHANGELOG [0.13.0] entry is rewritten rather than appended. A missing argument now reads `received no value` instead of `got nil`.
- Lua (BREAKING): binary operators and comparisons now reject extra arguments (`quiver.gt(f, 1, 99)` errors). Unary functions still ignore extras.
- Lua (BREAKING): `e1 == e2` between Expressions now compares identity, where it used to build an Expression that Lua read as true. `e1 < e2` / `e1 <= e2` now raise 'attempt to compare two ... values', where they used to be always true.
- Lua (additive): BinaryFile gains save, metadata, aggregate, aggregate_agents, select_agents and rename_agents.

**Risks**

- The directive conflicts with a project rule. Root AGENTS.md says every error message is Pattern 1/2/3 from the C++/C layer, and src/AGENTS.md:284-293 reverted a typed sol::protected_function for exactly this reason. The new errors name neither the operation nor the operand, and the signature part depends on the compiler. This needs a recorded Design Decision. Upgrade path if Pattern 1 must stay: add a final `sol::variadic_args` candidate to each overload set that throws the old text. That is sol2's documented 'overloading with fallback' pattern. sol2 still does the type check, but every unary function becomes arity-strict.
- If the trait form fails in the real tree (dllexport classes, Pimpl), the `sol::base_classes, sol::bases<AbstractExpression>()` tag also works; the mock passes all checks with it. The cost is a C closure in place of the __index table on both metatables (usertype_storage.hpp:559-584), so every f:read / f:write method lookup pays a C call plus a hash find. Benchmark a per-cell Lua write loop before accepting that.
- ODR: the sol2 trait specializations (base, derive, is_automagical) must be seen by every translation unit that instantiates sol2 checks or usertypes for these types. Today that is only binary.cpp. The direction-2 file re-split must move them into a shared lua_runner header if the bindings land in more than one file.
- Sync-test parse traps (lua-api-sync.test.ts:34-53). A bound name line must be bare: `"read",  // comment` would not parse. The shared method lambdas must be defined where Pass 2's `current` is empty (after a `.set_function(` line, before the first new_usertype). Otherwise clang-format can wrap an argument such as `"parameter",` onto its own line, it gets attributed to BinaryMetadata, and the coverage test fails. binary.cpp:53-55 already warns about this.
- TOML parsing is repeated on purpose. Each node() call on a file parses its TOML, so `f + f` builds two leaves and `f:metadata()` reads disk, the same as today's per-conversion behaviour. A cached leaf is rejected: it goes stale when the handle is reopened with 'w' and new metadata, and it puts the same leaf in collect_input_files twice.
- metadata() and get_metadata() can disagree. On a handle that was never opened (C++ path ctor, or C quiver_binary_file_create), get_metadata() returns empty metadata (binary_file.cpp:45-46) while metadata() reads the TOML. They also differ for a file rewritten after the handle was opened. Unifying the names is a separate decision; Julia uses get_metadata for both.
- Calling node() on a moved-from BinaryFile dereferences a null impl_ (binary_file.cpp:49). This was already true of Expression(moved_from) and is unreachable from C or Lua. Document it as a precondition rather than guarding it, per the 'clean over defensive' principle.
- Stale metadata is a pre-existing problem and still present: meta_ is snapshotted when the node is built (expression_file.cpp:10), but the file is re-read at save (binary_file.cpp:79). A file deleted in between throws the non-Pattern `File not found: <path>`.
- C++20 reversed operator== / operator!= candidates for mixed kinds (`expr == file`) compiled cleanly on MSVC /permissive- /W4. Not yet verified on the GCC and Clang CI builds.
- Julia keeps 97 forwarding methods, so the 'one abstract type' surface exists only in C++ and Lua until the optional Julia follow-up lands (a homogeneity gap, not a correctness one).

### abstract-base-full: AbstractExpression: a real C++ base class shared by BinaryFile and Expression, mirrored in sol2 (base_classes), Julia (abstract type) and C (one abstract handle)

- **Level:** cpp-julia-capi-lua
- **Keeps Pattern 1:** true
- **LOC:** About -110 production lines net: - C++ core about -90: friend block -47, operator bodies collapse about -45, new header +55, moved member declarations about -20, BinaryFile +8. - C API about +35. - Julia about -100: 97 forwarding methods deleted, about +12 for the abstract type and unsafe_convert. - Lua about +45: fallback +30, overload wrapping +15, file method registrations +12, to_expression deleted -10.  Plus about +160 new test lines and about 30 renamed (`metadata` to `get_metadata`), and about 60 documentation lines changed across AGENTS.md, src/AGENTS.md, src/c/AGENTS.md, Julia's AGENTS.md, lua-api.ts and the CHANGELOG.

**Mechanism**

NAMING: add a new abstract class `quiver::AbstractExpression` and keep `Expression` as the concrete lazy DAG value. The reverse (an abstract `Expression` plus a renamed concrete class) is worse in every layer:
- C++: an abstract `Expression` cannot be returned by value, so every operator would return the renamed class. All `Expression e = a + b;` lines (100+ in tests/test_expression.cpp) and the 101 `Expression(a)` calls would break.
- Julia: `AbstractExpression`/`Expression` follows the stdlib convention (AbstractArray/Array, AbstractString/String).
- Lua: scripts never name either type.
- C: C has no subtyping, so it keeps one handle type, `quiver_expression_t`, and that handle is the abstract one.
- `ExpressionBase` has no Julia precedent and reads like an implementation detail.

HIERARCHY (new header include/quiver/expression/abstract_expression.h):
- Two pure virtuals:
  - `std::shared_ptr<ExpressionNode> node() const`
  - `const BinaryMetadata& get_metadata() const`
- `save`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents` become non-virtual members of the base, implemented once on top of `node()`. Each returns `Expression`, which is incomplete in that header; that is legal for a declaration.
- `class BinaryFile : public AbstractExpression` (binary_file.h:17). `BinaryFile::node()` returns a fresh `std::make_shared<ExpressionFile>(impl_->file_path)` on every call, which keeps today's semantics exactly (expression.cpp:18, expression_file.cpp:10):
  - only the path is copied;
  - the leaf owns its own handle, so `save()` never opens or closes the user's handle;
  - the expression outlives the file.
- `BinaryFile::get_metadata()` (binary_file.h:45) becomes the override, still returning the in-memory metadata. It is virtual rather than `node()->metadata()` because for a file that would return a reference into a temporary leaf.
- The node is built lazily, so none of these problems arise:
  - no node is needed at construction (the TOML only exists after open('w'));
  - no null `node_` member;
  - no header cycle: abstract_expression.h only forward-declares `ExpressionNode` and `Expression`, and the include order is abstract_expression.h <- binary_file.h <- expression_node.h <- expression.h.
- `class Expression final : public AbstractExpression` keeps its `node_`.
- The implicit `Expression(const BinaryFile&)` (expression.h:19) becomes `explicit Expression(const AbstractExpression&)`, so all 101 `Expression(a)` test calls still compile.
- `node()` is public, so the 47-line friend block (expression.h:43-89) is deleted.
- Every operator and free function changes from `const Expression&` to `const AbstractExpression&`. All 36 operator bodies (expression.cpp:98-271) collapse onto three `binary()` helpers and one `unary()` helper. Each helper calls `node()` once per operand, into a local, so a file's TOML is parsed once and the left operand's error is the one reported. The scalar broadcast uses `node()->metadata()`, never `get_metadata()`, so it matches the leaf.
- `Expression::metadata()` is renamed `get_metadata()` to give the base one name. That matches BinaryFile, the C function `quiver_expression_get_metadata` and Julia.
- The aggregation enum moves to namespace scope as `AggregateOperation`, because abstract_expression.h cannot include expression_node.h. `ExpressionAggregate::Operation` becomes `using Operation = AggregateOperation;`, so every `ExpressionAggregate::Operation::Sum` spelling still compiles.

LUA (sol2 does the type check):
- The BinaryFile and Expression usertypes both declare `sol::base_classes, sol::bases<AbstractExpression>()`. `update_bases` (sol2-src/include/sol/usertype_storage.hpp:377-399) writes the class_check/class_cast keys into every sub-metatable, including the `shared_ptr<BinaryFile>` one. A typed `const AbstractExpression&` parameter then passes through the derived check (stack_check_unqualified.hpp:541-557).
- AbstractExpression itself is not registered as a usertype. The six expression methods are registered by name on both concrete usertypes, as free functions taking `const AbstractExpression& self` plus one `save` lambda that captures the db. Two reasons:
  - sol2's docs call automatic base-method lookup "an undocumented, unsupported feature" (documentation/source/api/usertype.rst:288);
  - lua-api-sync Pass 2 only sees quoted names inside `new_usertype<X>`, and it requires `Expression` to have at least one method (lua-api-sync.test.ts:77-83).
- Binary operators become `sol::overload` sets of typed candidates: (AE, AE), (AE, double), (double, AE).
- Unary metamethods are plain typed lambdas taking one parameter. sol2 ignores the duplicated operand Lua passes (lvm.c:1555/1566).
- Pattern 1 survives through sol2's documented "overloading with fallback": a last `sol::variadic_args` candidate that sol2 reaches only after every typed check failed. It reports the leftmost operand that is not an expression; a number counts as valid only next to an expression.
- `to_expression` and the `sol::object` plumbing (binary.cpp:99-139, 321-339) are deleted.

C API:
- The C struct hierarchy mirrors the C++ one. In src/c/internal.h, `struct quiver_expression` becomes a polymorphic base with `virtual const quiver::AbstractExpression& get() const`.
- Two structs derive from it:
  - `quiver_binary_file`;
  - `quiver_expression_value`, which holds a `quiver::Expression` and is what every operation's `out` returns, owned by the caller.
- Every `quiver_expression_*` function keeps its signature text, so the 141 test call sites are unchanged. `quiver_expression_t*` now means "any expression".
- New `quiver_binary_file_as_expression(file, &out)` borrows a file handle as an expression (`*out = file`).
- `quiver_expression_close` refuses a file view with a Pattern 1 error, through a `dynamic_cast` guard.
- `quiver_expression_from_file` stays as the C form of the C++ `explicit Expression(const AbstractExpression&)`: an owned snapshot.

JULIA:
- `abstract type AbstractExpression end` is declared in Quiver.jl before `include("binary/Binary.jl")` (Quiver.jl:26-27).
- `Binary.File <: AbstractExpression` and `Expression <: AbstractExpression`.
- Two `Base.unsafe_convert(::Type{Ptr{C.quiver_expression}}, x)` methods, one returning `e.ptr` and one calling `quiver_binary_file_as_expression`, let every generated @ccall wrapper take the AbstractExpression directly. @ccall also keeps the object rooted for the call.
- The 97 Binary.File forwarding methods (expression.jl:71-93, 107-111, 120-124, 137-141, 146-152, 163-176, 224-239, 265-266) are deleted.

PROBE: MSVC /O2 with Quiver's exact SOL_* defines, using mock types with this exact hierarchy, at scratchpad\abstract_probe\probe.cpp. These all worked:
- `fa+fb`, `fa+2`, `2+fa`, `-fa`, `~fa`, `fa&fb`;
- `quiver.abs(fa)`, `quiver.gt(2,fa)`, `quiver.ifelse(fa,fb,e)`;
- `fa:aggregate(...)`, `fa:get_metadata()`, `fa:save(...)`;
- the C++ operators, including `fa==fb`, `2.0==e`, `e!=fa` and `!fa` under C++20 rewritten-candidate rules.

The probe also reproduced all 13 Pattern 1 strings pinned in tests/test_lua_expression.cpp:556-584.

**Code sketch**

```cpp
// ===================== include/quiver/expression/abstract_expression.h (new) =====================
#ifndef QUIVER_ABSTRACT_EXPRESSION_H
#define QUIVER_ABSTRACT_EXPRESSION_H

#include "../binary/binary_metadata.h"
#include "../export.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

class Expression;
class ExpressionNode;

// The one aggregation enum, at namespace scope because aggregate() below names it and this header
// cannot include expression_node.h (it includes binary_file.h, which includes this file).
// ExpressionAggregate::Operation and ExpressionAggregateAgents::Operation are aliases of it.
enum class AggregateOperation { Sum, Mean, Min, Max, Percentile };

// Anything that is an expression: a BinaryFile or a lazy Expression DAG. Every operator, free
// function and method of the expression subsystem takes this type, so a file is accepted wherever an
// expression is. The methods are implemented once, on node(), and return a new Expression.
class QUIVER_API AbstractExpression {
public:
    virtual ~AbstractExpression() = default;

    virtual std::shared_ptr<ExpressionNode> node() const = 0;
    virtual const BinaryMetadata& get_metadata() const = 0;

    void save(const std::string& path) const;
    Expression aggregate(
        const std::string& dimension,
        AggregateOperation operation,
        std::optional<double> parameter = std::nullopt
    ) const;
    Expression aggregate_agents(AggregateOperation operation, std::optional<double> parameter = std::nullopt) const;
    Expression select_agents(const std::vector<std::string>& labels) const;
    Expression rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const;

protected:
    AbstractExpression() = default;
    AbstractExpression(const AbstractExpression&) = default;
    AbstractExpression(AbstractExpression&&) noexcept = default;
    AbstractExpression& operator=(const AbstractExpression&) = default;
    AbstractExpression& operator=(AbstractExpression&&) noexcept = default;
};

}  // namespace quiver

#endif  // QUIVER_ABSTRACT_EXPRESSION_H

// ===================== include/quiver/binary/binary_file.h (delta) =====================
#include "../expression/abstract_expression.h"

class QUIVER_API BinaryFile : public AbstractExpression {
public:
    explicit BinaryFile(const std::string& file_path);
    ~BinaryFile() override;
    BinaryFile(const BinaryFile&) = delete;
    BinaryFile& operator=(const BinaryFile&) = delete;
    BinaryFile(BinaryFile&& other) noexcept;
    BinaryFile& operator=(BinaryFile&& other) noexcept;

    // A fresh ExpressionFile over this file's path on every call: it re-reads the .toml and owns its
    // own handle, so an expression built from this file outlives it and save() never touches *this.
    std::shared_ptr<ExpressionNode> node() const override;
    const BinaryMetadata& get_metadata() const override;  // body unchanged: the in-memory metadata

    // File-specific: stays on BinaryFile only (no Expression counterpart).
    std::vector<double> read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls = false);
    void write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);
    // open_file / open / close / is_open / get_file_path unchanged
    // ...
};

// ===================== src/binary/binary_file.cpp (addition) =====================
#include "quiver/expression/expression_node.h"

std::shared_ptr<ExpressionNode> BinaryFile::node() const {
    return std::make_shared<ExpressionFile>(impl_->file_path);
}

// ===================== include/quiver/expression/expression.h (replaces the class + friend block) =====================
#include "abstract_expression.h"
#include "expression_node.h"

class QUIVER_API Expression final : public AbstractExpression {
public:
    explicit Expression(std::shared_ptr<ExpressionNode> node);
    // Freezes any expression (a file included) into a DAG value. Was the implicit Expression(const BinaryFile&).
    explicit Expression(const AbstractExpression& expression);

    std::shared_ptr<ExpressionNode> node() const override;
    const BinaryMetadata& get_metadata() const override;

private:
    std::shared_ptr<ExpressionNode> node_;
};

QUIVER_API Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator+(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const AbstractExpression& rhs);
// ... the same substitution for - * / > < >= <= == != && ||
QUIVER_API Expression operator-(const AbstractExpression& operand);
QUIVER_API Expression operator!(const AbstractExpression& operand);
QUIVER_API Expression abs(const AbstractExpression& operand);
QUIVER_API Expression sqrt(const AbstractExpression& operand);
QUIVER_API Expression log(const AbstractExpression& operand);
QUIVER_API Expression exp(const AbstractExpression& operand);
QUIVER_API Expression ifelse(
    const AbstractExpression& condition,
    const AbstractExpression& then_value,
    const AbstractExpression& else_value
);

// expression_node.h: class ExpressionAggregate { public: using Operation = AggregateOperation; ... };

// ===================== src/expression/expression.cpp (shape) =====================
Expression::Expression(std::shared_ptr<ExpressionNode> node) : node_(std::move(node)) {}
Expression::Expression(const AbstractExpression& expression) : node_(expression.node()) {}

std::shared_ptr<ExpressionNode> Expression::node() const {
    return node_;
}
const BinaryMetadata& Expression::get_metadata() const {
    return node_->metadata();
}

Expression AbstractExpression::aggregate(
    const std::string& dimension,
    AggregateOperation operation,
    std::optional<double> parameter
) const {
    return Expression(std::make_shared<ExpressionAggregate>(operation, node(), dimension, parameter));
}

void AbstractExpression::save(const std::string& path) const {
    const auto root = node();  // once: on a BinaryFile every call builds a new leaf
    std::vector<BinaryFile*> input_files;
    root->collect_input_files(input_files);
    // ... body of today's Expression::save (expression.cpp:53-95) with node_-> spelled root->
}

namespace {

// node() once per operand, into locals: on a BinaryFile each call parses the .toml, and locals fix
// the order (the left operand's error is the one reported).
Expression binary(ExpressionBinary::Operation operation, const AbstractExpression& lhs, const AbstractExpression& rhs) {
    auto l = lhs.node();
    auto r = rhs.node();
    return Expression(std::make_shared<ExpressionBinary>(operation, std::move(l), std::move(r)));
}

// The scalar broadcasts over the node's metadata, not get_metadata(): for a file that is the .toml
// the leaf just read, which is what the leaf computes against.
Expression binary(ExpressionBinary::Operation operation, const AbstractExpression& lhs, double rhs) {
    auto l = lhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(rhs, l->metadata());
    return Expression(std::make_shared<ExpressionBinary>(operation, std::move(l), std::move(scalar)));
}

Expression binary(ExpressionBinary::Operation operation, double lhs, const AbstractExpression& rhs) {
    auto r = rhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(lhs, r->metadata());
    return Expression(std::make_shared<ExpressionBinary>(operation, std::move(scalar), std::move(r)));
}

Expression unary(ExpressionUnary::Operation operation, const AbstractExpression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(operation, operand.node()));
}

}  // namespace

Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(const AbstractExpression& lhs, double rhs) {
    return binary(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(double lhs, const AbstractExpression& rhs) {
    return binary(ExpressionBinary::Operation::Add, lhs, rhs);
}
// ... 33 more one-liners
Expression abs(const AbstractExpression& operand) {
    return unary(ExpressionUnary::Operation::Abs, operand);
}
Expression ifelse(
    const AbstractExpression& condition,
    const AbstractExpression& then_value,
    const AbstractExpression& else_value
) {
    auto c = condition.node();
    auto t = then_value.node();
    auto e = else_value.node();
    return Expression(
        std::make_shared<ExpressionTernary>(ExpressionTernary::Operation::IfElse, std::move(c), std::move(t), std::move(e))
    );
}

// ===================== src/lua_runner/binary.cpp (anonymous namespace, above bind_binary) =====================
// The last candidate of every expression overload set. sol2 reaches it only after every typed
// candidate failed its check, so it owns the Pattern 1 message: sol2's own "no matching function
// call" names neither the operation nor the operand. It reports the leftmost operand that is not
// an expression; a number is acceptable only beside an expression (`numbers`). When every operand
// is valid, the call had too many arguments.
Expression report_operand_error(const char* operation, int arity, bool numbers, const sol::variadic_args& args) {
    const auto operand = [&](int i) {
        return i < static_cast<int>(args.size()) ? sol::object(args[i])
                                                  : sol::make_object(args.lua_state(), sol::lua_nil);
    };
    bool beside_expression = false;
    for (int i = 0; i < arity; ++i) {
        beside_expression = beside_expression || operand(i).is<AbstractExpression>();
    }
    for (int i = 0; i < arity; ++i) {
        const auto o = operand(i);
        if (o.is<AbstractExpression>() || (numbers && beside_expression && o.get_type() == sol::type::number)) {
            continue;
        }
        throw lua_type_error(operation, "operand", "an expression or a binary file", o);
    }
    throw std::runtime_error(
        "Cannot " + std::string(operation) + ": too many arguments (expected " + std::to_string(arity) + ", got " +
        std::to_string(args.size()) + ")"
    );
}

auto operand_error(const char* operation, int arity, bool numbers) {
    return [=](sol::variadic_args args) -> Expression { return report_operand_error(operation, arity, numbers, args); };
}

// One overload set per binary operator. sol2 type-checks each operand as an AbstractExpression
// (BinaryFile and Expression both declare it through sol::base_classes) or as a number.
template <typename Op>
auto binop(const char* operation) {
    return sol::overload(
        [](const AbstractExpression& lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        [](const AbstractExpression& lhs, double rhs) { return Op{}(lhs, rhs); },
        [](double lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        operand_error(operation, 2, true)
    );
}

template <typename T>
void bind_expression_operators(sol::usertype<T>& type) {
    type[sol::meta_function::addition] = binop<std::plus<>>("add");  // file + 2, 2 + file, file + expr
    type[sol::meta_function::subtraction] = binop<std::minus<>>("sub");
    type[sol::meta_function::multiplication] = binop<std::multiplies<>>("mul");
    type[sol::meta_function::division] = binop<std::divides<>>("div");
    type[sol::meta_function::bitwise_and] = binop<std::logical_and<>>("band");
    type[sol::meta_function::bitwise_or] = binop<std::logical_or<>>("bor");
    // Lua passes a unary metamethod its operand twice. Without an overload set sol2 checks no
    // arity, so the copy is ignored, and only an expression's metatable carries the metamethod.
    type[sol::meta_function::unary_minus] = [](const AbstractExpression& operand) { return -operand; };
    type[sol::meta_function::bitwise_not] = [](const AbstractExpression& operand) { return !operand; };
}

// The methods of every expression, registered by name on both usertypes (see bind_binary).
BinaryMetadata expression_get_metadata(const AbstractExpression& self) {
    return self.get_metadata();
}
Expression expression_aggregate(
    const AbstractExpression& self,
    const std::string& dimension,
    const std::string& op,
    const sol::object& parameter
) {
    const auto operation = parse_aggregate_op(op, "aggregate");
    const auto value = optional_from_lua<double>(parameter, "aggregate", "parameter", "a number");
    return self.aggregate(dimension, operation, value);
}
// expression_aggregate_agents / expression_select_agents / expression_rename_agents: today's
// lambda bodies (binary.cpp:293-317) with `const AbstractExpression& self`.

// ===================== bind_binary (registration) =====================
const auto save = [&db](const AbstractExpression& self, const std::string& path) {
    self.save(resolve_sandboxed_path(db, "save", path));
};

auto binary_file_type = state.new_usertype<BinaryFile>(
    "BinaryFile",
    sol::no_constructor,
    sol::base_classes,
    sol::bases<AbstractExpression>(),
    "read",  // file-specific: self is a BinaryFile, so e:read is nil on an Expression
    [](BinaryFile& self, const sol::object& dims, const sol::object& allow_nulls, sol::this_state s) {
        sol::state_view lua(s);
        const auto coordinates = lua_table_to_dim_map(dims, "read");
        const bool nulls = optional_from_lua<bool>(allow_nulls, "read", "allow_nulls", "a boolean").value_or(false);
        return to_lua_table(lua, self.read(coordinates, nulls));
    },
    // "write", "close", "is_open", "get_file_path" unchanged ...
    "get_metadata",
    &expression_get_metadata,
    "save",
    save,
    "aggregate",
    &expression_aggregate,
    "aggregate_agents",
    &expression_aggregate_agents,
    "select_agents",
    &expression_select_agents,
    "rename_agents",
    &expression_rename_agents
);
bind_expression_operators(binary_file_type);

auto expression_type = state.new_usertype<Expression>(
    "Expression",
    sol::no_constructor,
    sol::base_classes,
    sol::bases<AbstractExpression>(),
    "get_metadata",
    &expression_get_metadata,
    "save",
    save,
    "aggregate",
    &expression_aggregate,
    "aggregate_agents",
    &expression_aggregate_agents,
    "select_agents",
    &expression_select_agents,
    "rename_agents",
    &expression_rename_agents
);
bind_expression_operators(expression_type);

ns.set_function(
    "expression",
    sol::overload([](const AbstractExpression& operand) { return Expression(operand); }, operand_error("expression", 1, false))
);
ns.set_function(
    "abs",
    sol::overload([](const AbstractExpression& operand) { return quiver::abs(operand); }, operand_error("abs", 1, false))
);
// sqrt / log / exp identical
ns.set_function(
    "ifelse",
    sol::overload(
        [](const AbstractExpression& condition, const AbstractExpression& then_value, const AbstractExpression& else_value) {
            return quiver::ifelse(condition, then_value, else_value);
        },
        operand_error("ifelse", 3, false)
    )
);
ns.set_function("gt", binop<std::greater<>>("gt"));  // ... lt gte lte eq neq

// ===================== src/c/internal.h (C handle hierarchy) =====================
// quiver_expression_t is the handle of an AbstractExpression; every quiver_expression_* operation
// takes one. A caller owns what an operation returns (a quiver_expression_value). A binary file
// handle is one too, borrowed through quiver_binary_file_as_expression and closed only as a file.
struct quiver_expression {
    virtual ~quiver_expression() = default;
    virtual const quiver::AbstractExpression& get() const = 0;
};

struct quiver_binary_file final : quiver_expression {
    quiver::BinaryFile binary_file;
    explicit quiver_binary_file(quiver::BinaryFile&& bf) : binary_file(std::move(bf)) {}
    const quiver::AbstractExpression& get() const override { return binary_file; }
};

struct quiver_expression_value final : quiver_expression {
    quiver::Expression expression;
    explicit quiver_expression_value(quiver::Expression&& e) : expression(std::move(e)) {}
    const quiver::AbstractExpression& get() const override { return expression; }
};

// ===================== src/c/expression/expression.cpp (delta) =====================
QUIVER_C_API quiver_error_t quiver_binary_file_as_expression(quiver_binary_file_t* file, quiver_expression_t** out) {
    QUIVER_REQUIRE(file, out);
    *out = file;  // derived-to-base: the view is the file handle itself
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_expression_close(quiver_expression_t* expression) {
    if (dynamic_cast<quiver_binary_file*>(expression) != nullptr) {
        quiver_set_last_error("Cannot close: expression is a binary file handle; close it with quiver_binary_file_close");
        return QUIVER_ERROR;
    }
    delete expression;
    return QUIVER_OK;
}

// every op: lhs->expression becomes lhs->get(); new quiver_expression(...) becomes new quiver_expression_value(...)
*out = new quiver_expression_value(dispatch(operation, lhs->get(), rhs->get()));

// ===================== Julia =====================
// Quiver.jl, before include("binary/Binary.jl"):
//     abstract type AbstractExpression end
// binary/Binary.jl: using ..Quiver: C, check, Element, Optional, AbstractExpression
// binary/file.jl:   mutable struct File <: AbstractExpression
// expression.jl:    mutable struct Expression <: AbstractExpression
//
// Base.unsafe_convert(::Type{Ptr{C.quiver_expression}}, e::Expression) = e.ptr
// function Base.unsafe_convert(::Type{Ptr{C.quiver_expression}}, f::Binary.File)
//     out = Ref{Ptr{C.quiver_expression}}(C_NULL)
//     check(C.quiver_binary_file_as_expression(f.ptr, out))
//     return out[]
// end
// function _binop(operation, lhs::AbstractExpression, rhs::AbstractExpression)
//     out = Ref{Ptr{C.quiver_expression}}(C_NULL)
//     check(C.quiver_expression_apply(operation, lhs, rhs, out))  # @ccall roots lhs/rhs, unsafe_convert gives the handle
//     return Expression(out[])
// end
// Base.abs(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_ABS, a)
// Base.ifelse(c::AbstractExpression, t::AbstractExpression, e::AbstractExpression) = ...
// save(e::AbstractExpression, path::String) = (check(C.quiver_expression_save(e, path)); nothing)
```

**Wrong-type error text**

`quiver.abs(42)` gives `Cannot abs: operand must be an expression or a binary file, got number`.

`file + "x"` (Lua calls the file's `__add`) gives `Cannot add: operand must be an expression or a binary file, got string`. `"x" + file` gives the same message, because Lua 5.4's string `__add` hands off to the file's metamethod.

`LuaRunner::run` surfaces both as `Failed to run Lua script: <message>` followed by sol2's traceback (lua_runner.cpp:158-161). There is no position prefix, because the message is a C++ exception thrown from the fallback overload and pushed verbatim.

Both texts are byte-identical to today's pinned strings (tests/test_lua_expression.cpp:556-584), and the probe reproduced all 13 of them.

**Lua behaviour**

What scripts can now do:
- **A file is an expression everywhere.** The new file methods are `f:aggregate(dim, op[, p])`, `f:aggregate_agents(op[, p])`, `f:select_agents{...}`, `f:rename_agents{...}` and `f:save(path)`; `f:get_metadata()` already existed. Today scripts must write `quiver.expression(f)` first (binary.cpp:241-267 has none of these methods).
- **Operators are unchanged:** `f + g`, `f + 2`, `2 + f`, `-f`, `~f`, `f & g` and `quiver.abs/sqrt/log/exp/gt/.../ifelse(f, ...)` behave as before, now through typed `const AbstractExpression&` parameters checked by sol2.
- **`quiver.expression(x)` stays.** It takes any expression and returns an `Expression`, so the 31 test calls and existing scripts keep working; it is now just redundant.
- **One accessor name for metadata:** `get_metadata()` on files and expressions alike, so `e:metadata()` becomes `e:get_metadata()`.

What changes for scripts:
- `e:metadata()` must become `e:get_metadata()` (breaking).
- Extra arguments to a quiver.* expression function or binary operator now fail: `quiver.abs(e, 99)` raises `Cannot abs: too many arguments (expected 1, got 2)`. Before, sol2 ignored extras, because sol::overload matches arity exactly.
- A missing operand still reads `got nil`.
- Saving a file still open for writing raises `Cannot open_file: file is already open for writing: <canonical>` (binary_file.cpp:67-68).
- `f:save(f:get_file_path())` raises `Cannot save: output path collides with input file '<path>'`.
- A dot-call without self (`f.aggregate('stage','sum')`) raises sol2's own `stack index 1, expected userdata...` text, the same as every other method under the existing policy (src/AGENTS.md:822-827).
- Unchanged pre-existing quirk: Lua `==`/`<` between expressions are still always true (sol2's automatic `__eq`/`__lt`).
- Handle lifetime (RunHandles) is unchanged, because an expression built from a file keeps only its path.

**Cross-layer impact**

**C++ core**
- One new public header, abstract_expression.h. BinaryFile gains a vptr and inherits `save`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents`, so `file.aggregate(...)` and `file.save(...)` now work in C++.
- Every operator and free function takes `const AbstractExpression&`, so a file needs no conversion.
- `Expression(const BinaryFile&)` becomes `explicit Expression(const AbstractExpression&)`.
- `Expression::metadata()` is renamed `get_metadata()`.
- The friend block (expression.h:43-89) is deleted.
- `AggregateOperation` moves to namespace scope; `ExpressionAggregate::Operation` remains as an alias.

**C API**
- Handles stay opaque and every `quiver_expression_*` signature keeps its text. `quiver_expression_t` now means "any expression".
- One new function, `quiver_binary_file_as_expression` (a borrowed upcast, no allocation, no TOML parse).
- `quiver_expression_close` refuses a file view with a Pattern 1 error.
- `quiver_expression_from_file` stays as the owned snapshot.
- Internally `quiver_binary_file` derives from the polymorphic `quiver_expression` struct, and the concrete wrapper is renamed `quiver_expression_value`.
- Dart, Python and JS do not reference any binary or expression symbol (grep of bindings/dart/lib, bindings/python/src and bindings/js/src found none), so they are unaffected and need no generator change beyond Julia's.

**Julia**
- `abstract type AbstractExpression` is declared in Quiver.jl, with `Binary.File` and `Expression` both subtypes.
- Two `unsafe_convert` methods route either one to the C handle.
- The 97 Binary.File forwarding methods are deleted.
- New surface comes for free: `save(file, path)` and `Quiver.get_metadata(file)`.
- `Expression(file)` is unchanged (10 test uses).
- `Binary.get_metadata(::File)` stays a separate generic function.

**Lua**
- See lua_behavior. BinaryFile gains six methods; Expression loses `metadata` in favour of `get_metadata`.

**Homogeneity**
- After the change, every layer spells the metadata accessor `get_metadata` and accepts a file for every expression operation.

**Tests and docs impact**

**Tests**
- tests/test_expression.cpp:
  - 26 `.metadata()` calls become `.get_metadata()`.
  - The 101 `Expression(a)` calls compile unchanged through the explicit constructor.
  - ImplicitConversionFromBinaryFile (:1128) is renamed to describe `a + b` on two files.
  - Add tests that `file.aggregate`, `file.select_agents` and `file.save` match their `Expression(file)` counterparts.
  - Add a test that the scalar broadcast uses the leaf's metadata.
- tests/test_lua_expression.cpp:
  - 3 `:metadata()` calls become `:get_metadata()`.
  - The 13 pinned operand errors stay as they are.
  - Add `f:aggregate`, `f:aggregate_agents`, `f:select_agents`, `f:rename_agents` and `f:save` (including on a still-open writer, and onto its own path).
  - Add the too-many-arguments error, and `quiver.abs(db)` / BinaryMetadata giving `got userdata`.
- tests/test_c_api_expression.cpp:
  - The 141 existing call sites are unchanged.
  - Add tests for `quiver_binary_file_as_expression` (null args; apply/save/aggregate on a view; `get_metadata` on a view equal to `quiver_binary_file_get_metadata`; `quiver_expression_close` on a view returns QUIVER_ERROR and the file is still usable).
- bindings/julia/test/test_expression.jl: the existing Binary.File operator tests now exercise the AbstractExpression methods. Add `save(file, path)` and `Quiver.get_metadata(file)`.
- lua-api-sync.test.ts: no change. Expression still has quoted methods, and the new BinaryFile names are quoted inside `new_usertype<BinaryFile>`.

**Docs**
- lua-api.ts:
  - Rewrite the expression block (:905-919): drop "files auto-wrap"; show `f:aggregate` etc.; replace `e:metadata()` with `e:get_metadata()`; describe the arity rule.
- Root AGENTS.md:
  - Binary cross-layer table: Get metadata row, plus an `AbstractExpression` row.
  - Fix the Lua expression paragraph ("operate on files directly" becomes true).
  - Design Decisions: one abstract expression type in every layer; sol::overload plus a variadic fallback as the Pattern 1 mechanism.
- src/AGENTS.md:
  - Expression Subsystem section: AbstractExpression, `node()`, constructors.
  - Fix ":1021 Caches an open BinaryFile".
  - Note the enum exception: AggregateOperation lives at namespace scope.
  - Replace the binop/to_expression description at :682-686.
  - Record the base_classes `__index` cost.
- src/c/AGENTS.md: handle hierarchy and the ownership rule for views.
- bindings/julia/AGENTS.md: the abstract type and the unsafe_convert convention.

**CHANGELOG [0.13.0]**
- Rewrite the unreleased operand-error entry (CHANGELOG.md:81-84). Against v0.12.9 the text still changes from `Cannot build expression: ...`.
- Add BREAKING entries (see breaking_changes).
- 0.13.0 is already a minor bump, so no version change is needed.

**Breaking changes**

- C++: `Expression::metadata()` is renamed `get_metadata()` (it is now AbstractExpression's pure virtual). Callers must rename the call.
- C++: `Expression(const BinaryFile&)` is now `explicit Expression(const AbstractExpression&)`. Copy-initialization `Expression e = file;` no longer compiles. Write `Expression e(file);`, or pass the file directly, since every operator and function takes AbstractExpression.
- C++: the operator and free-function signatures change from `const Expression&` to `const AbstractExpression&`, and BinaryFile now derives from AbstractExpression (it gains a vtable and changes its exported layout). C++ consumers must recompile.
- C++: the aggregation enum is declared at namespace scope as `quiver::AggregateOperation`. `ExpressionAggregate::Operation` and `ExpressionAggregateAgents::Operation` remain as aliases, so existing spellings still compile.
- Lua: `expr:metadata()` is removed in favour of `expr:get_metadata()`, the same name a file uses.
- Lua: quiver.* expression functions and binary operators now reject extra arguments with `Cannot <op>: too many arguments (expected N, got M)`, where sol2 used to ignore them silently. Scripts must drop the extra arguments.
- C API (behavioural, not ABI): `quiver_expression_close` now returns QUIVER_ERROR for a handle obtained from `quiver_binary_file_as_expression`. The new function is additive, and the existing signatures are textually unchanged.

**Risks**

- Method lookup gets slower on BinaryFile and Expression. `sol::base_classes` makes `update_bases` replace the table `__index` with a C closure on every derived sub-metatable (usertype_storage.hpp:377-399, 564, 581). The probe printed `Plain __index type table` against `function` for BinaryFile. Cost:
- lookup plus call rose from 118-143 to 199-246 ns per call (about +90-110 ns per `f:read`/`f:write` call);
- a real Release `rd:read(...)` loop through build/release/bin/quiver_cli.exe measured 5.3-6.7 µs per call (1M reads);
- so the slowdown is about 2% on the Lua per-cell hot path.
Check it with a before/after benchmark: the Do-Not-Fix list protects binary hot-path decisions.
- Release-build warnings: in the probe, sol::overload produced 8 C4702 'unreachable code' warnings from sol2 headers at /O2 (stack.hpp:285, function_types_overloaded.hpp:48), even with a noinline throwing helper. The current src/lua_runner/binary.cpp compiled at /O2 produces 0. These are code-generation warnings, so /external:W0 does not suppress them. The repo has no /WX (cmake/CompilerOptions.cmake:6 sets only /W4), so they are noise in Release, not failures.
- Arity becomes strict because sol::overload matches arity exactly (call.hpp:178-190), a script-visible behaviour change. The fallback turns it into a Pattern 1 message rather than sol2's 'no matching function call'.
- A dot-call without self still surfaces sol2's own text, with an MSVC-demangled signature such as `bad argument into 'quiver::Expression(const quiver::AbstractExpression&, ...)'`. This is the existing policy for self, but the AbstractExpression name now appears in that text.
- C ownership is ambiguous by type: a `quiver_expression_t*` may be owned (the out-parameter of an operation) or borrowed (from `quiver_binary_file_as_expression`). The `dynamic_cast` guard makes a wrong close safe, but a C caller who closes the file and then uses the view hits a use-after-free, as with any borrowed pointer. Julia never keeps a view past one ccall. The alternative, a distinct `quiver_abstract_expression_t` view type, is type-safe but churns all 141 C test call sites; rejected for that reason.
- Julia's `unsafe_convert(::Type{Ptr{C.quiver_expression}}, ::Binary.File)` makes an FFI call, and can throw through `check`, while @ccall is converting arguments. This is legal but unusual. It relies on `Base.cconvert(::Type{<:Ptr}, x) = x` passing the File object through unchanged.
- Comparisons in C++20: `operator==`/`operator!=` now take base-class parameters, and rewritten candidates have the same conversion sequences as the originals. The tie is broken by the 'non-rewritten beats rewritten' rule. MSVC compiled it in the probe; GCC, Clang and AppleClang (Linux/macOS CI) are unverified.
- A moved-from BinaryFile's `node()` dereferences a null impl_ (binary_file.cpp:45-49). This already exists today through Expression(file), and the C API and Lua cannot reach it.
- Stale metadata is unchanged: each `node()` call re-reads the TOML, and `save()` reopens the file, so a file rewritten in between still gives mismatched dimensions or reads. A deleted file still throws the non-Pattern `File not found: <path>`.
- Every ExpressionFile now holds a BinaryFile that is itself an AbstractExpression (expression_node.h:43). This is harmless, but readers may find it confusing; document it.
- In the anonymous namespace, the method helpers must stay above bind_binary. Otherwise a clang-format-wrapped string-literal argument alone on its own line (e.g. `"aggregate_agents",`) would be picked up by sync-test Pass 2 as a method of whichever usertype is open (binary.cpp:53-55).

### abstract-base-minimal: AbstractExpression base: one pure virtual node(), BinaryFile and Expression derive from it, Lua uses typed sol::bases parameters plus an error-only reporting overload

- **Level:** cpp-and-lua
- **Keeps Pattern 1:** true
- **LOC:** Measured on the scratch patch: about +374/-338 production lines, net about +36. - abstract_expression.h: +51 (new) - expression.h: +50/-114 (40 friend declarations gone) - expression.cpp: +138/-135 (overloads become one-liners over 3 helpers) - expression_file.cpp: +7 - binary_file.h: +6/-2 - expression_node.h: +2/-1 - lua_runner/binary.cpp: +120/-86 (BinaryFile lists the 6 methods; the reporting entry replaces to_expression/is_number)  Plus about +60 test lines and about +40 doc/CHANGELOG lines. The C API and Julia have 0 lines changed.

**Mechanism**

**C++ core**
- Add one new header, include/quiver/expression/abstract_expression.h, declaring `class QUIVER_API AbstractExpression`.
  - It has a virtual dtor and exactly one pure virtual, `std::shared_ptr<ExpressionNode> node() const`: the DAG node this expression contributes.
  - The six expression methods move to it as non-virtual members built on node(): metadata, save, aggregate, aggregate_agents, select_agents, rename_agents.
- `Expression` derives from it and returns its node_.
- `BinaryFile` derives from it. Its node() returns a fresh path-based `ExpressionFile` leaf, defined in src/expression/expression_file.cpp, which is the same thing `Expression(const BinaryFile&)` builds today (src/expression/expression.cpp:18).
  - The expression never holds, opens or closes the caller's handle. That keeps the "close the file right after building" guarantee (bindings/julia/test/test_expression.jl:140-151) and the RunHandles story intact.
- Every operator and free function changes its parameters from `const Expression&` to `const AbstractExpression&`: `+ - * /`, the comparisons, `&& || !`, unary `-`, abs/sqrt/log/exp and ifelse. The double overloads stay.
  - Because the operators read node() publicly, the 40 friend declarations at expression.h:43-89 are deleted.
- There is no header cycle. abstract_expression.h needs only binary_metadata.h and forward declarations of Expression and ExpressionNode.
  - The one nested enum it needed, `ExpressionAggregate::Operation` (expression_node.h:142), moves to namespace scope as `AggregateOperation`. `ExpressionAggregate` keeps `using Operation = AggregateOperation;`, so the C API (src/c/expression/expression.cpp:77-88) compiles unchanged.
- `Expression(const BinaryFile&)` stays and is implicit, now written as `node_(file.node())`. The C API at src/c/expression/expression.cpp:103 and the tests compile unchanged.
- The C API sources, the C header and Julia are untouched.

**Lua (src/lua_runner/binary.cpp)**
- Both usertypes declare `sol::base_classes, sol::bases<AbstractExpression>()`. `AbstractExpression` itself is never registered.
  - sol2 accepts a derived userdata for a base parameter through the weak_derive flag that this registration sets (build/_deps/sol2-src/include/sol/usertype_storage.hpp:386) and the class_check/class_cast stored in each derived metatable (stack_check_unqualified.hpp:541-552, stack_get_unqualified.hpp:906-918).
- Every quiver.* expression function, every operator metamethod and every expression-method lambda takes `const AbstractExpression&`, so sol2 does the type check.
  - Numbers have their own typed candidates in a `sol::overload` (A,A), (A,double), (double,A).
  - Unary metamethods take (A, A), because Lua passes the operand twice (lua-src/src/lvm.c:1555 OP_UNM, :1566 OP_BNOT).
- The six method lambdas are defined once as named lambdas and listed by quoted name in both `new_usertype<BinaryFile>` and `new_usertype<Expression>`.
  - They are not inherited from a base usertype, because sol2 calls base member lookup "an undocumented, unsupported feature" (sol2-src/documentation/source/api/usertype.rst:288).
  - Metamethods can never be inherited, because Lua reads them from the operand's own metatable. So `bind_expression_operators<T>` stays.
  - Listing the names in both usertypes also keeps the lua-api-sync Pass 2 parser seeing the methods on each type (bindings/js/test/lua-api-sync.test.ts:35-53).
- `to_expression` and `is_number` (binary.cpp:99-115) are deleted.
- **The one non-typed piece is a reporting entry, and it is error-path only.** Each overload ends with an `operand_error(op, arity, numbers)` `sol::variadic_args` candidate, which is sol2's documented fallback pattern (examples/source/overloading_with_fallback.cpp).
  - sol2 reaches it only after rejecting every typed candidate.
  - It uses sol2's own `is<AbstractExpression>()` check only to name the leftmost offending operand, then throws the existing Pattern 1 text through `lua_type_error` (internal.h:187-197).
  - Dropping it gives pure sol2 text (see wrong_type_error_text), at a cost of about 25 fewer lines and 13 rewritten test expectations.
- A sol2 argument_handler specialization was rejected. It is keyed by signature, so abs, sqrt, log, exp and expression would share one handler, and the op name could only come from lua_getinfo, which reports a local alias name ("Cannot f"; earlier probe, solprobe/handler.exe). It also is not consulted on the overload no-match path (call.hpp:157).

**Code sketch**

```cpp
// ===== include/quiver/expression/abstract_expression.h (NEW) =====
#include "../binary/binary_metadata.h"
#include "../export.h"
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace quiver {
class Expression;
class ExpressionNode;

// Namespace scope so this header needs no expression_node.h (which needs a complete BinaryFile).
enum class AggregateOperation { Sum, Mean, Min, Max, Percentile };

// Anything that is an expression: an Expression (lazy DAG) or a BinaryFile (a leaf).
class QUIVER_API AbstractExpression {
public:
    virtual ~AbstractExpression() = default;
    // A BinaryFile returns a fresh path-based leaf: the caller's handle is never opened or closed.
    virtual std::shared_ptr<ExpressionNode> node() const = 0;

    BinaryMetadata metadata() const;  // by value: a BinaryFile's leaf is a temporary
    void save(const std::string& path) const;
    Expression aggregate(const std::string& dimension, AggregateOperation operation,
                         std::optional<double> parameter = std::nullopt) const;
    Expression aggregate_agents(AggregateOperation operation, std::optional<double> parameter = std::nullopt) const;
    Expression select_agents(const std::vector<std::string>& labels) const;
    Expression rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const;
};
}  // namespace quiver

// ===== include/quiver/binary/binary_file.h (diff) =====
#include "../expression/abstract_expression.h"
class QUIVER_API BinaryFile : public AbstractExpression {
    ~BinaryFile() override;
    // ... unchanged I/O members ...
    std::shared_ptr<ExpressionNode> node() const override;  // defined in expression_file.cpp
};

// ===== include/quiver/expression/expression.h (class replaces lines 17-92; 40 friends deleted) =====
class QUIVER_API Expression : public AbstractExpression {
public:
    Expression(const BinaryFile& file);  // kept implicit; C API + tests unchanged
    explicit Expression(std::shared_ptr<ExpressionNode> node);
    std::shared_ptr<ExpressionNode> node() const override { return node_; }
private:
    std::shared_ptr<ExpressionNode> node_;
};
QUIVER_API Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator+(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression abs(const AbstractExpression& operand);
QUIVER_API Expression ifelse(const AbstractExpression& condition, const AbstractExpression& then_value,
                             const AbstractExpression& else_value);
// ... every other operator/free function: s/const Expression&/const AbstractExpression&/

// ===== include/quiver/expression/expression_node.h =====
class QUIVER_API ExpressionAggregate final : public ExpressionNode {
public:
    using Operation = AggregateOperation;  // was: enum class Operation { Sum, Mean, Min, Max, Percentile };

// ===== src/expression/expression_file.cpp (added) =====
std::shared_ptr<ExpressionNode> BinaryFile::node() const {
    return std::make_shared<ExpressionFile>(get_file_path());
}

// ===== src/expression/expression.cpp =====
Expression::Expression(const BinaryFile& file) : node_(file.node()) {}

BinaryMetadata AbstractExpression::metadata() const { return node()->metadata(); }

void AbstractExpression::save(const std::string& path) const {
    const auto root = node();  // LOAD-BEARING: keeps a BinaryFile's temporary leaf (and its BinaryFile*) alive
    std::vector<BinaryFile*> input_files;
    root->collect_input_files(input_files);
    // ... body unchanged, node_-> becomes root-> ...
}

namespace {
using BinaryOp = ExpressionBinary::Operation;
using UnaryOp = ExpressionUnary::Operation;
// One node per operand: node() on a BinaryFile parses its TOML and builds a new leaf each call.
Expression binary(BinaryOp op, std::shared_ptr<ExpressionNode> lhs, std::shared_ptr<ExpressionNode> rhs) {
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(lhs), std::move(rhs)));
}
Expression binary(BinaryOp op, std::shared_ptr<ExpressionNode> lhs, double rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(rhs, lhs->metadata());
    return binary(op, std::move(lhs), std::move(scalar));
}
Expression binary(BinaryOp op, double lhs, std::shared_ptr<ExpressionNode> rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(lhs, rhs->metadata());
    return binary(op, std::move(scalar), std::move(rhs));
}
Expression unary(UnaryOp op, const AbstractExpression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(op, operand.node()));
}
}  // namespace
Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs) { return binary(BinaryOp::Add, lhs.node(), rhs.node()); }
Expression operator+(const AbstractExpression& lhs, double rhs) { return binary(BinaryOp::Add, lhs.node(), rhs); }
Expression operator+(double lhs, const AbstractExpression& rhs) { return binary(BinaryOp::Add, lhs, rhs.node()); }
Expression abs(const AbstractExpression& operand) { return unary(UnaryOp::Abs, operand); }
Expression ifelse(const AbstractExpression& c, const AbstractExpression& t, const AbstractExpression& e) {
    return Expression(std::make_shared<ExpressionTernary>(ExpressionTernary::Operation::IfElse, c.node(), t.node(), e.node()));
}

// ===== src/lua_runner/binary.cpp (replaces is_number/to_expression/binop at :99-139) =====
// sol2 rejected every typed candidate (this is sol::overload's last entry). Names the leftmost operand none
// of them takes; a missing one reads as nil; an all-number call reports its first operand.
auto operand_error(const char* operation, int arity, bool numbers) {
    return [operation, arity, numbers](sol::variadic_args args) -> Expression {
        const int count = std::max(arity, static_cast<int>(args.size()));
        auto at = [&](int i) {
            return i < static_cast<int>(args.size()) ? sol::object(args[i])
                                                     : sol::make_object(args.lua_state(), sol::lua_nil);
        };
        int bad = 0;
        for (int i = 0; i < count; ++i) {
            const auto o = at(i);
            if (i >= arity || (!o.is<AbstractExpression>() && !(numbers && o.get_type() == sol::type::number))) {
                bad = i;
                break;
            }
        }
        throw lua_type_error(operation, "operand", "an expression or a binary file", at(bad));
    };
}
template <typename Op>
auto binop(const char* operation) {  // one metamethod/comparison WITH number handling
    return sol::overload(
        [](const AbstractExpression& lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        [](const AbstractExpression& lhs, double rhs) { return Op{}(lhs, rhs); },
        [](double lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        operand_error(operation, 2, true));
}
template <int Arity, typename F>
auto typed(const char* operation, F f) { return sol::overload(f, operand_error(operation, Arity, false)); }

template <typename T>
void bind_expression_operators(sol::usertype<T>& type) {
    type[sol::meta_function::addition] = binop<std::plus<>>("add");  // file + 2, 2 + file, file + expr
    // ... sub/mul/div/band/bor likewise ...
    type[sol::meta_function::unary_minus] = [](const AbstractExpression& a, const AbstractExpression&) { return -a; };
    type[sol::meta_function::bitwise_not] = [](const AbstractExpression& a, const AbstractExpression&) { return !a; };
}

// in bind_binary, after the csv_to_bin set_function (resets the sync parser):
const auto save = [&db](const AbstractExpression& self, const std::string& path) {
    self.save(resolve_sandboxed_path(db, "save", path));
};
const auto metadata = [](const AbstractExpression& self) { return self.metadata(); };
const auto aggregate = [](const AbstractExpression& self, const std::string& dimension, const std::string& op,
                          const sol::object& parameter) {
    const auto operation = parse_aggregate_op(op, "aggregate");  // now returns AggregateOperation
    const auto value = optional_from_lua<double>(parameter, "aggregate", "parameter", "a number");
    return self.aggregate(dimension, operation, value);
};
// aggregate_agents / select_agents / rename_agents: existing bodies, self is const AbstractExpression&

auto binary_file_type = state.new_usertype<BinaryFile>(
    "BinaryFile",
    sol::no_constructor,
    sol::base_classes,
    sol::bases<AbstractExpression>(),
    "read",  // FILE-SPECIFIC: stays typed BinaryFile& (I/O is not an expression operation)
    [](BinaryFile& self, const sol::object& dims, const sol::object& allow_nulls, sol::this_state s) {
        sol::state_view lua(s);
        const auto coordinates = lua_table_to_dim_map(dims, "read");
        const bool nulls = optional_from_lua<bool>(allow_nulls, "read", "allow_nulls", "a boolean").value_or(false);
        return to_lua_table(lua, self.read(coordinates, nulls));
    },
    // ... write / close / is_open / get_metadata / get_file_path unchanged (BinaryFile& self) ...
    "save",
    save,
    "metadata",
    metadata,
    "aggregate",
    aggregate,
    "aggregate_agents",
    aggregate_agents,
    "select_agents",
    select_agents,
    "rename_agents",
    rename_agents);
bind_expression_operators(binary_file_type);
auto expression_type = state.new_usertype<Expression>(
    "Expression", sol::no_constructor, sol::base_classes, sol::bases<AbstractExpression>(),
    "save", save, "metadata", metadata, "aggregate", aggregate, "aggregate_agents", aggregate_agents,
    "select_agents", select_agents, "rename_agents", rename_agents);  // clang-format puts one per line
bind_expression_operators(expression_type);

ns.set_function("expression", typed<1>("expression", [](const AbstractExpression& o) { return Expression(o.node()); }));
ns.set_function("abs", typed<1>("abs", [](const AbstractExpression& o) { return quiver::abs(o); }));
ns.set_function(
    "ifelse",
    typed<3>("ifelse", [](const AbstractExpression& c, const AbstractExpression& t, const AbstractExpression& e) {
        return quiver::ifelse(c, t, e);
    }));
ns.set_function("gt", binop<std::greater<>>("gt"));  // lt/gte/lte/eq/neq likewise
```

**Wrong-type error text**

**With this design (Pattern 1 kept)**
- `quiver.abs(42)`:
  - Seen by the script via pcall: `Cannot abs: operand must be an expression or a binary file, got number`.
  - Through `LuaRunner::run`: `Failed to run Lua script: Cannot abs: operand must be an expression or a binary file, got number` followed by `\nstack traceback:\n\t[C]: in field 'abs'...`. Captured from the scratch quiver_tests build, `[RAW]` line.
- `file + "x"`: `Cannot add: operand must be an expression or a binary file, got string`. The same message comes back for `"x" + file`.
- There is no `[string ...]:1:` position prefix. The C++ exception is pushed verbatim by sol2's trampoline (trampoline.hpp:117-127).

**If the reporting overload is dropped (pure sol2, MSVC; from the probe, with q:: replaced by quiver::)**
- `quiver.abs(42)`: `[string "..."]:1: stack index 1, expected userdata, received number: value is not a valid userdata (bad argument into 'quiver::Expression(const quiver::AbstractExpression&)')`. The demangled part differs on GCC/Clang.
- `file + "x"` (an overloaded metamethod): `[string "..."]:1: sol: no matching function call takes this number of arguments and the specified types` (call.hpp:157). This names neither the operation nor the operand.

**Lua behaviour**

**What scripts can now do**
- A BinaryFile is an expression everywhere:
  - Operators and quiver.* functions take a file directly. They already did through to_expression; now sol2 does the check.
  - **New:** `f:aggregate(dim, op[, p])`, `f:aggregate_agents(op[, p])`, `f:select_agents{...}`, `f:rename_agents{...}`, `f:save(path)` and `f:metadata()` work on a raw file without `quiver.expression(f)`. This matches Julia, which already forwards aggregate/aggregate_agents/select_agents/rename_agents for Binary.File (expression.jl:224-239, 265-266).
- `f:save(path)` uses a fresh path-based leaf, so `f` is still open afterwards. Verified: `assert(f:is_open())` in the scratch test ProbeFileHasTheExpressionMethods.
- The existing safeguards apply to a raw file:
  - The output-path collision guard ("Cannot save: output path collides with input file '<p>'").
  - The sandbox ("Cannot save: path ... escapes the database directory").
  - The write-registry error when the file is still open for writing ("Cannot open_file: file is already open for writing: <canonical>", binary_file.cpp:67-68).
- `quiver.expression(x)` stays: it is documented, the sync test's "no documented name removed" check enforces it, and it mirrors Julia's `Expression(file)`. It is now redundant.

**Unchanged**
- Every wrong-type message, including the 13 pinned expectations in tests/test_lua_expression.cpp:556-584 (they pass in the scratch build).
- Scalars on either side of an operator: `2 + f`, `f & 1`.
- `'x' + f` still reaches our `__add` through the string library's trymt (lstrlib.c:277) and reports "got string".
- The existing quirk where `e == e2` and `f == g` are always true, from sol2's automatic __eq on an Expression-returning operator==. Checked against the current quiver_cli: `f==g true, e==e2 true, e==f false`. The new design behaves the same.

**Behaviour changes**
- Extra arguments to a quiver.* expression function now raise, because sol::overload matches arity strictly (call.hpp:180-190).
  - Example: `quiver.abs(e, 99)` raises `Cannot abs: operand must be an expression or a binary file, got number`. The current CLI returns an Expression and ignores the 99.
  - Metamethods are unaffected, since Lua always passes exactly two operands.
  - To keep the old leniency, give the typed candidates a trailing `sol::variadic_args`.
- On a file, `f:metadata()` re-reads the .toml from disk, exactly as `quiver.expression(f):metadata()` does today. `f:get_metadata()` stays the handle's in-memory copy.

**Cross-layer impact**

**C++ core (the only API change)**
- New `quiver::AbstractExpression` and `quiver::AggregateOperation`. `ExpressionAggregate::Operation` stays as an alias.
- `BinaryFile` gains the six members: `file.aggregate(...)`, `file.save(...)`, `abs(file)`, `!file`, `file > 1.0`.
- `Expression::metadata()` now returns by value.
- `BinaryFile` and `Expression` become polymorphic (one vptr), so every C++ consumer rebuilds. That is acceptable under the WIP rule.

**C API**
- Zero source changes. src/c/expression/expression.cpp and src/c/binary/binary_file.cpp compile cleanly with QUIVER_C_EXPORTS against the new headers, with AbstractExpression dllimported.
- The C ABI is unchanged because the handles are opaque (src/c/internal.h:32-40).
- `quiver_expression_from_file` remains the single bridge, and it now routes through `BinaryFile::node()`. That gives one file-to-node construction site where there were three (expression.cpp:18, c/expression/expression.cpp:103, lua binary.cpp:111-112).
- No `quiver_binary_file_aggregate` or similar, since C has no inheritance.

**Julia**
- No change. The 97 forwarding methods keep working on top of the unchanged C API.
- Homogeneity gap: Lua now has `file:save` / `file:metadata`, while Julia has no `save(::Binary.File)`; its `Binary.get_metadata(file)` already exists under its own name. Optional parity is a one-line forwarding method plus a test.
- Collapsing the 97 methods into a `Union` alias is out of scope.

**Dart / Python / JS**
- None. They do not expose binary/expression, per the root AGENTS.md design decision.

**sol2**
- No customization points, interop or argument_handler, so there is no ODR or visibility hazard.
- AbstractExpression is never a registered usertype, and nothing relies on base member lookup.

**Tests and docs impact**

**Already run in scratch**
- The repo was exported with `git archive HEAD` to `C:\Users\rsampaio\AppData\Local\Temp\claude\C--Development-Quiver-quiver1\31615b61-c410-4c50-9b4b-09c06026f2eb\scratchpad\absprobe\qsrc`, patched, and built into `absprobe\qbuild`, using the repo's `build/_deps` sources read-only through FETCHCONTENT_SOURCE_DIR_*. No repo file was modified.
- Full `quiver_tests`: 1457 tests. All pre-existing tests pass unchanged, including:
  - ImplicitConversionFromBinaryFile
  - OperatorMetamethodsOnFileAndExpression
  - FilePlusFile, IfElse, ComparisonFreeFunctions, LogicalOperators
  - OperandErrorsNameTheOperation and OperandErrorsReportTheLeftmostBadOperand
  - the lifecycle tests
- The only failure was my own probe line (MW + unitless units mismatch). After fixing it, the `*Expression*:*Binary*:*Lifecycle*` filter passes 331/331.
- `bun test test/lua-api-sync.test.ts` against the patched copy passes 6/6, even before any doc edit, because the coverage check ignores receivers.

**Tests to add (the scratch probes, cleaned up)**
- C++: `ProbeBinaryFileIsAnAbstractExpression` covers `a.save`, `abs(a).aggregate(...)`, `a.select_agents` and `a.rename_agents` on a raw BinaryFile, and `a.is_open()` after save.
- Lua: `ProbeFileHasTheExpressionMethods` covers `f:aggregate/aggregate_agents/select_agents/rename_agents/save/metadata` on a raw file, with value checks and `f:is_open()` after save.
- Lua: `ProbeArityAndRawErrors` covers `quiver.abs(e, 99)` → got number, `quiver.gt(e)` → got nil, `quiver.abs()` → got nil, `f + db` → got userdata, and `'x' + f` → got string.
- A Lua test for `f:save` while `f` is a live writer, pinning the write-registry error.
- No C API, Julia, Dart, Python or JS test changes.

**Docs**
- lua-api.ts:
  - Change `local e = (quiver.expression(r) + 10.0) * 2.0 -- files auto-wrap` to say a file is an expression.
  - Show `r:aggregate(...)` / `r:save(...)` on a file.
  - Keep `quiver.expression`.
  - Note `get_metadata` (handle) vs `metadata` (re-read from disk), and that extra arguments raise.
- Root AGENTS.md:
  - Add a Design Decision for BinaryFile as an AbstractExpression: one pure virtual node(); sol::bases; the methods are listed on both usertypes because sol2 base lookup is unsupported (usertype.rst:288) and metamethods are not inherited; the reporting overload owns Pattern 1.
  - Make the "operate on files directly" claim at :821-822 true for the methods too.
- src/AGENTS.md:
  - Rewrite the binop/to_expression bullet (:682-686).
  - Amend the typed-parameter stance (:284-293): expression operands are typed, and sol2's text is masked by a trailing variadic_args reporting entry.
  - Update the Expression type bullet (:1008-1013): methods now on AbstractExpression; `AggregateOperation` replaces the nested enum.
  - Fix the stale ":1021 Caches an open BinaryFile" (it is unopened except inside save, expression.cpp:61-73).
- CHANGELOG [0.13.0]: one new entry. 0.13.0 is already the unreleased minor (CHANGELOG.md:8, latest tag v0.12.9), so no version bump. The :81-84 operand-message entry stays accurate.

**Breaking changes**

- C++ ABI: BinaryFile and Expression become polymorphic (they derive from AbstractExpression, which adds a vptr), so every C++ consumer of libquiver must recompile. The C ABI is unchanged (opaque handles).
- C++ source: Expression::metadata() now returns BinaryMetadata by value, where it returned const BinaryMetadata&. `const auto& m = e.metadata();` still works through temporary lifetime extension, which is how every existing caller uses it (tests/test_expression.cpp:1382 etc.). Code that keeps a pointer or reference beyond the full expression must copy instead.
- C++ source, compatible: the operators and free functions are declared on const AbstractExpression&, and ExpressionAggregate::Operation is now an alias of quiver::AggregateOperation. All existing call sites compiled unchanged (test_expression.cpp, test_binary_file.cpp, C API).
- Lua behaviour, worth flagging as BREAKING in the CHANGELOG: extra arguments to quiver.expression/abs/sqrt/log/exp/ifelse/gt/lt/gte/lte/eq/neq now raise a Pattern 1 error. For example, quiver.abs(e, 99) gives `Cannot abs: operand must be an expression or a binary file, got number`. Today the extra arguments are silently ignored (verified with build/bin/quiver_cli.exe). A caller must drop the extra arguments.

**Risks**

- The reporting overload re-inspects arguments with `is<AbstractExpression>()` after sol2's typed check fails. It is not conversion plumbing; the success path is pure typed sol2 dispatch. If the user reads 'no sol::object' literally, it can be deleted (about 25 lines), but then 13 pinned expectations become sol2's compiler-dependent text, contradicting the root Error Messages rule and src/AGENTS.md:284-293.
- AbstractExpression::save must hold `const auto root = node();`. Without the local, a BinaryFile's temporary leaf dies after collect_input_files and the collected BinaryFile* dangle (UB). Comment it as load-bearing.
- Every node() call on a BinaryFile parses the .toml and builds a new leaf (expression_file.cpp:10), so `f + f` builds two leaves, exactly as the implicit conversion does today. The binary() helpers take node() once per operand, so a scalar op does not parse twice. Metadata is still snapshotted at build time and re-read at save (binary_file.cpp:79): this stale-metadata window is pre-existing.
- BinaryFile now carries two metadata accessors: get_metadata() (the handle's in-memory copy, binary_file.h:45) and metadata() (re-read from disk through a leaf). Both C++ and Lua docs must state the difference.
- sol::overload is strict about arity (call.hpp:180-190): see breaking_changes. A wrong unary metamethod call cannot happen from Lua syntax, but `getmetatable(e).__unm(5)` would surface sol2's raw text. That is an edge case and is accepted.
- The design relies on sol2's weak_derive<AbstractExpression> flag, which is set only when a usertype registers bases<AbstractExpression> (usertype_storage.hpp:386). Both BinaryFile and Expression must keep `sol::base_classes, sol::bases<AbstractExpression>()`, or that type stops passing as an expression. A regression test (OperatorMetamethodsOnFileAndExpression) already covers both.
- Do not move the six methods into a registered AbstractExpression usertype and rely on inheritance. sol2 documents base member lookup as unsupported (usertype.rst:288), and metamethods are never inherited because Lua rawgets them on the operand's own metatable.
- Lua now has file:save and file:metadata, which Julia's Binary.File lacks (bindings/julia/src/expression.jl has no save(::Binary.File)). This is a small homogeneity gap unless the optional Julia one-liners are added.
- Platforms not yet compiled: GCC/Clang/MinGW with QUIVER_API visibility on an exported polymorphic base (vtable and typeinfo export across the DLL boundary) and sol2's demangled qualified_name on GCC/Clang. The design does not depend on the qualified_name text matching anything handwritten, only on sol2 matching it against itself.

## Judgments

### intent-simplicity — winner: AbstractExpression base: one pure virtual node(), BinaryFile and Expression derive from it, Lua uses typed sol::bases parameters plus an error-only reporting overload

| Design | Intent | Rules | Simplicity | Risk | Total | Rationale |
|---|---|---|---|---|---|---|
| AbstractExpression: a real C++ base class shared by BinaryFile and Expression, mirrored in sol2 (base_classes), Julia (abstract type) and C (one abstract handle) | 9 | 6 | 4 | 5 | 24 | Follows the directive most literally, and in every layer. It has one abstract type in C++, Lua, Julia and C. It also renames metadata() to get_metadata(), which matches Julia (expression.jl:183) and C (quiver_expression_get_metadata), so a BinaryFile ends up with one metadata accessor instead of two. The cost is machinery the directive did not ask for. The C struct becomes polymorphic, a new borrowed-view function quiver_binary_file_as_expression is added, quiver_expression_close gets a dynamic_cast guard, and Julia's unsafe_convert makes an FFI call while @ccall converts arguments. The borrowed view makes quiver_expression_t* sometimes owned and sometimes borrowed, which breaks the root AGENTS.md rule that ownership 'must be explicit and unambiguous'. Its fallback also changes current behaviour. `quiver.gt(1, {})` now reports 'got number', because the beside_expression rule rejects the number first. Today binop reports the rhs table (binary.cpp:126-128). The probe used mock types only, nothing was built in the real tree, and GCC/Clang are unverified. Deleting 97 Julia forwarders is a real gain, but Julia could get it without any change to the C API. |
| Stateless AbstractExpression base: a BinaryFile stands for the file at its path, never for the open handle | 8 | 4 | 7 | 6 | 25 | The purest reading of 'let sol2 do the type check': typed parameters only, no fallback, and no C API or Julia changes. Its trait mechanism is correct in sol2 v3.5. Every usertype unconditionally stores class_check from inheritance<T>::type_check (usertype_storage.hpp:1099), which reads base<T>::type (inheritance.hpp:67-79). The checker consults derive<T> (stack_check_unqualified.hpp:541-552). So SOL_BASE_CLASSES/SOL_DERIVED_CLASSES (forward.hpp:249-265) work without change_indexing. Against that, it drops Pattern 1 for compiler-dependent sol2 text. That breaks the root 'Error Messages' rule and reverts the deliberate C8 messages pinned at test_lua_expression.cpp:556-584. It also makes binary ops arity-strict but leaves unary ops lenient. The namespace-sol trait and is_automagical specializations create an ODR coupling with the direction-2 file split. Like design 3, it leaves BinaryFile with both metadata(), which re-reads disk, and get_metadata(), which returns the handle's copy. One factual error: it says `f1 == f2` 'stays an identity comparison, as today'. Today the implicit Expression(const BinaryFile&) together with sol2 automagic __eq makes it always true, which design 3 confirmed against quiver_cli. |
| AbstractExpression base: one pure virtual node(), BinaryFile and Expression derive from it, Lua uses typed sol::bases parameters plus an error-only reporting overload | 8 | 8 | 8 | 8 | 32 | Meets the directive in C++ and Lua with the smallest blast radius. One pure virtual node() is the only thing the two kinds supply, and the six methods are non-virtual on the base. Every operator and quiver.* function takes const AbstractExpression&, checked by sol2 through sol::bases. to_expression and is_number (binary.cpp:99-115) are deleted. The C API and Julia need zero source changes. The variadic_args entry runs only on the error path, after sol2 has rejected every typed candidate, so it is sol2's documented fallback pattern, not conversion plumbing; still, it is about 25 lines of sol::object inspection, a small cost. It keeps the 13 pinned Pattern 1 strings byte-identical, and its first-bad-operand rule reproduces today's `quiver.gt(1, {})` -> 'got table'. It has the strongest evidence: a real-tree scratch build (scratchpad/absprobe/qbuild/bin/quiver_tests.exe exists), the full quiver_tests passing, and lua-api-sync passing 6/6. Two warts remain. It keeps the now-anomalous implicit Expression(const BinaryFile&). It also gives BinaryFile two metadata accessors with different semantics: metadata() builds a fresh leaf from the .toml, while get_metadata() returns the in-memory copy from binary_file.h:45. The sol::base_classes __index closure cost on f:read/f:write has not been benchmarked. |

**Graft ideas**

- From design 1: one metadata accessor named get_metadata on the base. Rename Expression::metadata() to get_metadata(), which matches Julia expression.jl:183 and C quiver_expression_get_metadata. Do not add a second `metadata` method to BinaryFile in Lua, because its semantics would diverge from f:get_metadata(): a fresh .toml leaf versus the in-memory copy (binary_file.h:45).
- From designs 1 and 2: replace the implicit Expression(const BinaryFile&) (expression.h:19) with `explicit Expression(const AbstractExpression&)`. The C API (src/c/expression/expression.cpp:103) and the tests already use direct-init, so they still compile. This removes the leftover special case, and quiver.expression becomes `Expression(o)`.
- From design 2: use the trait form (SOL_BASE_CLASSES/SOL_DERIVED_CLASSES) instead of sol::base_classes if a before/after benchmark of a per-cell f:read loop shows the change_indexing __index closure regresses the hot path. It is valid because class_check is registered unconditionally (usertype_storage.hpp:1099). Put the macros in one lua_runner header so the direction-2 file split cannot violate the ODR.
- From design 2 (separate decision): specialize sol::is_automagical<Expression/BinaryFile> to false. That ends the pre-existing always-true Lua `==`/`<` caused by automagic __eq/__lt on an operator that returns an Expression. Record it either way.
- From design 1, Julia part only: add `abstract type AbstractExpression`, make File and Expression subtypes, and collapse the 97 forwarders by converting a File through the existing quiver_expression_from_file (Expression(f)). That closes the Lua/Julia homogeneity gap (file:save) without a borrowed-view C handle.
- From design 1: on extra arguments, report `Cannot <op>: too many arguments (expected N, got M)`. That is clearer than design 3's 'operand must be ... got number', which names an extra argument as if it were a bad operand.

**Fatal flaws**

- Design 2 drops Pattern 1 for Lua operand errors. That breaks the root AGENTS.md rule that all error messages come from the C++/C layer, and reverts the deliberately pinned C8 strings (tests/test_lua_expression.cpp:556-584). The replacement text includes a compiler-dependent demangled signature. This is disqualifying unless the user explicitly approves it as a recorded Design Decision.
- Design 2 makes a false factual claim: that `f1 == f2` is an identity comparison 'as today' or 'as BinaryFile had'. Today Expression(const BinaryFile&) is implicit (expression.h:19), so sol2's automagic __eq applies to BinaryFile and returns an Expression, which Lua reads as true. Design 3 confirmed `f==g true` against quiver_cli.
- Design 1's C borrowed view (quiver_binary_file_as_expression) lets the same handle type, quiver_expression_t*, be owned or borrowed. That breaks the root 'Ownership must be explicit and unambiguous' principle, and a C caller who closes the file and keeps the view hits a use-after-free. Not a sol2 error, but a rules-level defect in a layer the directive never required changing.
- Design 1's fallback changes the reported operand for `quiver.gt(1, {})` from the table, which is current behaviour per binary.cpp:126-128, to the number. This is a minor regression; no test pins it.

### rules-risk — winner: AbstractExpression base: one pure virtual node(), BinaryFile and Expression derive from it, Lua uses typed sol::bases parameters plus an error-only reporting overload

| Design | Intent | Rules | Simplicity | Risk | Total | Rationale |
|---|---|---|---|---|---|---|
| AbstractExpression: a real C++ base class shared by BinaryFile and Expression, mirrored in sol2 (base_classes), Julia (abstract type) and C (one abstract handle) | 9 | 6 | 4 | 5 | 24 | Covers the directive most fully: C++, Lua, Julia and C all get one abstract expression type. It deletes about 54 Binary.File forwarding lines from Julia (bindings/julia/src/expression.jl:71-176). It also unifies the accessor name: today C++/Lua use metadata() (expression.h:23, binary.cpp:285) while C and Julia use get_metadata (expression.jl:183-185), and D1 makes every layer get_metadata.  The sol2 claims hold: - update_bases writes class_check/class_cast into every sub-metatable (usertype_storage.hpp:262-270, 376-401). - The derived check is at stack_check_unqualified.hpp:541-557. - The overload arity skip is at call.hpp:167-190. - The fallback keeps all 13 Pattern 1 strings and adds a clear 'too many arguments' message.  Rules cost: the C API now has a quiver_expression_t* that is sometimes owned (what an operation returns) and sometimes a borrowed alias of a file handle (quiver_binary_file_as_expression). That is ambiguous by type, against the root rule that ownership must be explicit and unambiguous. A dynamic_cast guard in close and a possible use-after-free for C callers are only mitigations.  It touches four layers and about 25 files: a polymorphic C struct hierarchy (src/c/internal.h:32-40), a regenerated c_api.jl, and a Julia unsafe_convert that makes an FFI call and can throw during @ccall argument conversion. Its probe used mock types, not the real tree.  Risk: it is the only design that measured the base_classes __index cost (change_indexing, usertype_storage.hpp:559-584 swaps the table at :1137 for a closure), about 2% on f:read. |
| Stateless AbstractExpression base: a BinaryFile stands for the file at its path, never for the open handle | 7 | 3 | 7 | 5 | 22 | Its technical claims are correct: - The SOL_BASE_CLASSES/SOL_DERIVED_CLASSES traits exist (forward.hpp:249-264). - The trait path works because every usertype gets class_check from inheritance<T>::type_check over base<T>::type (usertype_storage.hpp:1099-1100, inheritance.hpp:78-80). It keeps __index a plain table (:1137), so f:read pays nothing extra. - is_automagical<T>=false gates only the eq/lt/le/pairs/length/tostring/call registrations (usertype_core.hpp:126-178). __gc is registered separately (usertype_storage.hpp:1073-1090), so turning it off is safe and fixes the always-true Lua ==.  It fails the project rules: - keeps_pattern1 is false, so the 13 Pattern 1 strings the last milestone added on purpose (tests/test_lua_expression.cpp:556-584) become sol2 text with a compiler-specific demangled signature. - That contradicts the root Error Messages rule and src/AGENTS.md:822-827, which say the safeties are a backstop and the explicit checks own every Pattern 1 message, and :284-293, which rejected typed params for exactly this text. - Tests have to fall back to substring matching.  Risks: - The trait and is_automagical specializations must be visible in every TU that instantiates sol2 for these types. Direction 2's file re-split makes a silent ODR violation (no diagnostic required) likely. - Arity becomes strict for binary operators but stays lenient for unary ones, which is inconsistent. - It leaves two metadata accessors on BinaryFile, metadata() and get_metadata(), with different semantics.  Julia keeps its forwarding methods. |
| AbstractExpression base: one pure virtual node(), BinaryFile and Expression derive from it, Lua uses typed sol::bases parameters plus an error-only reporting overload | 8 | 8 | 8 | 8 | 32 | It meets the directive in the two layers it names: - One abstract type with typed `const AbstractExpression&` sol2 parameters checked through sol::base_classes. weak_derive is set at usertype_storage.hpp:386 and the check is at stack_check_unqualified.hpp:541-557. - The variadic fallback is reached only after every typed candidate fails (call.hpp:160-203), so sol2 still does the type check and Pattern 1 survives unchanged (internal.h:187-197).  The C API (src/c/internal.h:32-40, src/c/expression/expression.cpp:103) and Julia need no source change. Ownership stays as it is today: BinaryFile::node() is the current path-based leaf (expression.cpp:18, expression_file.cpp:10), and save holds the root in a local.  It is the only design built in the real tree. I re-ran its scratch quiver_tests (absprobe/qbuild/bin) with filter *LuaExpression*:*ExpressionTest*:*Probe*: 31/31 pass, including the pinned operand errors and the new raw-file method tests.  Weaknesses: - It never mentions the base_classes __index cost (usertype_storage.hpp:559-584): every f:read/f:write lookup becomes a C closure, about 2% per D1's measurement, which matters under the binary hot-path rule. - It never mentions the Release C4702 noise from sol::overload. - It keeps the special-cased implicit Expression(const BinaryFile&) when a general explicit Expression(const AbstractExpression&) would do. - BinaryFile ends up with both metadata() (reads the TOML) and get_metadata() (the handle's copy). - An extra argument is reported as 'got number' (quiver.abs(e, 99)), which misleads. - Lua gains file:save, which Julia lacks, a small homogeneity gap. |

**Graft ideas**

- From D1: give extra arguments their own Pattern 1 message, 'Cannot <op>: too many arguments (expected N, got M)'. D3's fallback currently reports the extra argument's type ('Cannot abs: operand must be an expression or a binary file, got number' for quiver.abs(e, 99)), which reads as a type error. The other option is to keep the old leniency by giving the typed candidates a trailing sol::variadic_args, which sol2 lets skip the arity filter (call.hpp:167-190).
- From D1: benchmark a 1M-iteration Release rd:read loop through quiver_cli before and after, because sol::base_classes replaces the __index table with a closure (usertype_storage.hpp:559-584, :1137). If the cost matters, switch to D2's SOL_BASE_CLASSES/SOL_DERIVED_CLASSES traits (forward.hpp:249-264). They work because every usertype gets class_check from base<T>::type (usertype_storage.hpp:1099). Put the traits in one src/lua_runner header that every TU binding these types includes, so direction 2's re-split cannot cause an ODR violation.
- From D1/D2: replace the special-cased implicit Expression(const BinaryFile&) with explicit Expression(const AbstractExpression&). The C API (src/c/expression/expression.cpp:103) and the tests use direct-init, so they still compile, and no file-specific conversion remains once a file is an expression.
- From D1: one accessor name. Today C++/Lua say metadata() (expression.h:23, binary.cpp:285) while C/Julia say get_metadata (expression.jl:183-185). D3 would put both metadata() and get_metadata(), with different semantics, on BinaryFile. Either unify on get_metadata as D1 does (virtual, with the file override returning the in-memory copy), or at minimum do not bind a second metadata on the BinaryFile usertype. Record the choice in the CHANGELOG as BREAKING if Lua's e:metadata() is renamed.
- From D2: specialize sol::is_automagical<BinaryFile/Expression> to false so Lua's e1 == e2 / f == g stop being always true (sol2 auto-registers __eq from the Expression-returning operator==, usertype_core.hpp:142-150). __gc is unaffected (usertype_storage.hpp:1073-1090). This is a separate breaking decision; the specialization must sit in a header shared by every TU that binds these types.
- From D2 (rather than D1's C-handle mechanism): if Julia should mirror the abstract type, declare `abstract type AbstractExpression end`, make File and Expression subtypes, and convert locally with `_expr(f::File) = Expression(f)` to collapse the forwarding methods (expression.jl:71-176). That needs no borrowed quiver_expression_t* view in the C API.
- From D1/D2: protected defaulted copy and move operations on AbstractExpression, to prevent assignment through a base reference. D3's real header (absprobe/overlay/.../abstract_expression.h) declares none.

**Fatal flaws**

- D2: keeps_pattern1=false is fatal under the project rules. Root AGENTS.md says every error message is a Pattern 1/2/3 message defined in C++. src/AGENTS.md:822-827 says the sol2 safeties are only the backstop behind the explicit checks, which own Pattern 1. src/AGENTS.md:284-293 already rejected a typed parameter because it surfaced sol2's 'stack index N, expected ...' text. D2 replaces the 13 deliberately pinned messages (tests/test_lua_expression.cpp:556-584) with text that depends on the compiler, even though sol2's documented overload-with-fallback (which D1 and D3 use) keeps sol2 doing the type check.
- D2: its trait and is_automagical specializations must be visible in every TU that instantiates sol2 for BinaryFile/Expression. Direction 2's lua_runner re-split makes a silent ODR violation (no diagnostic required) likely unless the specializations move into a shared header first.
- D1: not a sol2 falsehood, but it is a correctness and ownership hazard. quiver_expression_t* becomes owned or borrowed depending on where it came from (quiver_binary_file_as_expression returns the file handle itself). That breaks the root rule that ownership must be explicit and unambiguous; a C caller who closes the file and then uses the view hits a use-after-free. The dynamic_cast guard in quiver_expression_close only covers the double-close case.
- D3: no false claim found (weak_derive at usertype_storage.hpp:386 and the no-match luaL_error at call.hpp:157 both check out against the sol2 source). But its risk list omits the per-lookup __index closure cost that sol::base_classes introduces (usertype_storage.hpp:559-584), which needs measuring under the binary hot-path rule before merge.

## Compiled spike

- **Compiled:** true
- **Dir:** `C:\Users\rsampaio\AppData\Local\Temp\claude\C--Development-Quiver-quiver1\31615b61-c410-4c50-9b4b-09c06026f2eb\scratchpad\spike`
- **Build:** `Debug (the project's flags, copied from build/compile_commands.json for src/lua_runner/binary.cpp): cl /nologo /TP -DSOL_ALL_SAFETIES_ON=1 -DSOL_NO_NIL=1 -DSOL_PRINT_ERRORS=0 -DSOL_SAFE_GETTER=0 -DSOL_SAFE_NUMERICS=1 -DSOL_SAFE_STACK_CHECK=0 -IC:\Development\Quiver\quiver1\build\_deps\lua-src\src -IC:\Development\Quiver\quiver1\build\_deps\lua-build\src -external:IC:\Development\Quiver\quiver1\build\_deps\sol2-src\include -external:W0 /DWIN32 /D_WINDOWS /EHsc /Ob0 /Od /RTC1 -std:c++20 -MDd -Zi /bigobj /W4 /permissive- /Zc:__cplusplus /utf-8 /wd4251 /FS /Fe:spike_debug.exe main.cpp /link C:\Development\Quiver\quiver1\build\_deps\lua-build\src\lua-5.4d.lib. Release is the same with /O2 /Ob2 /DNDEBUG -MD and build\release\_deps\lua-build\src\lua-5.4.lib. Wrapped in spike\build.bat ('cmd /c build.bat' or 'cmd /c build.bat release'). MSVC 14.51.36231 (VS 18 Community), sol2 v3.5.0, Lua 5.4.8. The Debug build has 0 warnings at /W4.`

| Verified | Design | Claim | Evidence |
|---|---|---|---|
| yes | winning+runner-up | A typed sol2 parameter `const AbstractExpression&` accepts a shared_ptr<BinaryFile> userdata (what the real db:open_file returns, binary.cpp:190-197) and an Expression value userdata. Registration is `sol::base_classes, sol::bases<AbstractExpression>()` on both derived usertypes; AbstractExpression is never registered. | spike main.cpp setup(): AbstractExpression is never registered. Output: `quiver.abs(f)` gives abs(file(f.qvr)), `quiver.abs(e)` gives abs((file(f.qvr) * 2)). In sol2, update_bases sets weak_derive<Base> at usertype_storage.hpp:386; the checker rawgets class_check from the object's own metatable at stack_check_unqualified.hpp:541-552; the getter applies class_cast at stack_get_unqualified.hpp:905-916. |
| yes | winning+runner-up | A BinaryFile moved by value into a Lua userdata also binds to const AbstractExpression&, and the reference has the right dynamic type: the virtual node() dispatches to BinaryFile::node. | g = db:open_file_value('g.qvr') returns BinaryFile by value. `g:dyn()` (typeid of the const AbstractExpression& self) prints `class q::BinaryFile | file(g.qvr)`, and f (shared_ptr) prints the same type. `e:dyn()` prints `class q::Expression | (file(f.qvr) * 2)`. `quiver.abs(g)`, `-g` and `g:aggregate('stage','mean',3)` all work. |
| yes | winning+runner-up | sol::object::is<AbstractExpression>() compiles for the abstract class. It is true for a file and an expression, and false for a number, a table, another usertype, db, nil and a string. | quiver.is_expr: `truetruetrue` for f/g/e; `falsefalsefalsefalsefalsefalse` for 42, {}, Meta userdata, db, nil and 'x'. |
| yes | winning+runner-up | sol::overload tries the typed (A,A), (A,double) and (double,A) candidates in order and reaches the trailing sol::variadic_args fallback only on a mismatch, wrong arity included. A std::runtime_error thrown there reaches pcall verbatim, with no position prefix. | f+1, 1+f, f+f, f+g, e+f, f&1 and quiver.gt(2,f) all dispatch correctly. Errors arrive as `pcall: Cannot add: ...` with no `[string ...]:1:` prefix: sol2 pushes e.what() in its trampoline catch (trampoline.hpp:116-117) and returns lua_error at :127. sol2's own failures go through luaL_error and carry the prefix (call.hpp:157). |
| yes | winning+runner-up | The fallback reproduces the Pattern 1 text for every pinned case. | Both modes give: `Cannot abs: operand must be an expression or a binary file, got number` (abs(42)); `got string` (abs('x')); `got nil` (abs()); `got userdata` (abs(meta)). For `+`: `Cannot add: ... got string` for e+'x', f+'x' and 'x'+f; `got userdata` for f+db; `got table` for f+{}. For gt: `Cannot gt: ... got nil` (gt(e)); `got number` (gt(1,2)); `got string` (gt('x',2)). Also `Cannot ifelse: ... got number` (ifelse(f,1,e)), `got nil` (ifelse(f,e)), and `Cannot expression: ... got number`. |
| yes | winning+runner-up | 'x' + f reaches the file's __add through the string library's trymt and reports got string. | The spike prints `'x' + f => Cannot add: operand must be an expression or a binary file, got string`. trymt is at lua-src/src/lstrlib.c:277-285: it fetches arg 2's metafield and calls it. |
| yes | winning | Unary metamethods typed (const A&, const A&) work for -f and ~f, because Lua passes the operand twice. | Mode W prints -f = neg(file(f.qvr)), -g, -e, ~f = not(file(f.qvr)), ~e. Lua passes rb twice at lvm.c:1555 (OP_UNM, `luaT_trybinTM(L, rb, rb, ra, TM_UNM)`) and :1566 (OP_BNOT). |
| yes | runner-up | A single-parameter typed unary metamethod works: sol2 ignores the duplicated operand. | Mode R binds `[](const AbstractExpression& a){ return -a; }`: -f, -g, -e, ~f and ~e all produce the same results as mode W. |
| yes | winning+runner-up | The six expression methods, defined once as lambdas taking `const AbstractExpression& self` and listed by name on both usertypes, work on a raw file and on an expression. f:save leaves f open. | f:aggregate('stage','sum') gives agg[stage,0](file(f.qvr)); f:select_agents{'a','b'} works; f:metadata().unit is `MW` while f:get_metadata().unit is `in-memory`; `f:save('out.qvr')` followed by `open=true`; e:save works. |
| yes | winning | node() runs once per operand: f + 2 builds one leaf, f + f builds two. | quiver.node_calls() returns 1 after `f + 2` and 2 after `f + f`. |
| yes | winning | Extra arguments to a quiver.* expression function now raise, because sol::overload matches arity strictly: quiver.abs(e, 99) gives `Cannot abs: operand must be an expression or a binary file, got number`. | Mode W prints exactly that for quiver.abs(e, 99). sol2's arity filter is at call.hpp:167-180 (`free_arity != fxarity` at :180), which is skipped only for runtime-variadic candidates (:167, :179). |
| yes | winning | Optional: a typed candidate with a trailing sol::variadic_args keeps today's extra-argument leniency and still reports Pattern 1 on a wrong type. | quiver.abs_lenient = sol::overload([](const A&, sol::variadic_args){...}, operand_error(...)). `abs_lenient(e, 99)` succeeds (NO ERROR: abs(...)); `abs_lenient(42)` gives `Cannot abs: operand must be an expression or a binary file, got number`. |
| yes | runner-up | The fallback reports `Cannot <op>: too many arguments (expected N, got M)` when every operand is valid. | Mode R: `Cannot abs: too many arguments (expected 1, got 2)` for abs(e,99) and abs(e,f); `Cannot gt: too many arguments (expected 2, got 3)` for gt(e,f,3), gt(e,f,f) and gt(e,1,'x'). |
| yes | winning+runner-up | With no reporting overload, sol2's raw text is `stack index 1, expected userdata, received number ... (bad argument into 'quiver::Expression(const quiver::AbstractExpression&)')` for a single function, and `sol: no matching function call takes this number of arguments and the specified types` for an overload set. | abs_raw(42): `[string "..."]:1: stack index 1, expected userdata, received number: value is not a valid userdata (bad argument into 'q::Expression(const q::AbstractExpression&)')`. add_raw(f,'x'): `[string "..."]:1: sol: no matching function call takes this number of arguments and the specified types` (call.hpp:157). |
| yes | winning+runner-up | The pre-existing __eq quirk is preserved: f==g is true, e==e2 is true, e==f is false. | The spike prints `true true false`. sol2's auto __eq pushes op(l,r), an Expression, which is truthy (stack_core.hpp:1387-1416). Between different usertypes, unqualified_check_get<T> fails and it returns false (:1392-1398). |
| yes | winning+runner-up | C++ call sites resolve with operators on const AbstractExpression& and the double overloads: a+b, Expression(a)+Expression(b), 2.0+Expression(a), (E(a)>1.0)&&(E(b)<2.0), C++20 a==b / 2.0==e / e!=a, !a, ifelse(a, e, 1.0+b), std::plus<>{}(a,2.0), the copy ctor, the converting ctor, implicit copy-init `Expression e = a;`, a.aggregate and a.save. | cpp_call_sites() compiles with MSVC /permissive- /std:c++20 /W4 and no warnings. It prints e.g. `a==b=(file(a.qvr) == file(b.qvr)) | 2==e1=(2 == (...)) | e1!=a=(...) | implicit=file(a.qvr) | a.aggregate=agg[stage,0](file(a.qvr)) | open=1`. GCC and Clang are not tested. |
| **no** | winning | Every TU compiles at /W4 with no new warnings. | True in Debug only (0 warnings). At Release /O2 /Ob2 the always-throwing variadic fallback adds 12 C4702 'unreachable code' warnings inside sol2 headers: stack.hpp(285) once and function_types_overloaded.hpp(48) 11 times. The current real src/lua_runner/binary.cpp compiled with the build/release flags has 0 warnings (object written to scratch). A variant whose fallback has a reachable return (-DSPIKE_FALLBACK_RETURNS) has 0, so the throw is the cause. They do not fail the build: there is no /WX (cmake/CompilerOptions.cmake:4-14). |
| yes | runner-up | sol::overload plus the fallback produces C4702 at /O2 (probe: 8), against 0 today. | Spike: 12 C4702, against 0 for the real binary.cpp at /O2. The count scales with the number of distinct overload sets (11 here), so expect a similar number in the real TU. |
| yes | runner-up | sol::base_classes replaces the table __index with a C function on each derived metatable, costing about +90-110 ns per method call. | `type(getmetatable(x).__index)` is `function` for f, e and Based, and `table` for Plain. Release timings per o:get() over 2M iterations, 3 runs: value Plain 53-65 ns vs Based 138-172 ns; shared_ptr<Plain> 94-106 ns vs shared_ptr<Based> 179-194 ns. That is +80-110 ns per call. The cause is update_bases installing index_call_with_bases (usertype_storage.hpp:393-398). The winning design's risks do not mention it. |
| yes | winning+runner-up | sol2 documents base-class member lookup as unsupported, so the methods must be listed on each derived usertype. | sol2-src/documentation/source/api/usertype.rst:288 (`an undocumented, unsupported feature`). The spike lists the methods on both usertypes and never relies on base lookup. examples/source/overloading_with_fallback.cpp exists. |
| yes | winning+runner-up | Dot-calls without self surface sol2's raw text, which now names AbstractExpression for the shared methods. | f.aggregate('stage','sum'): `[string "..."]:1: stack index 1, expected userdata, received string: value is not a valid userdata (bad argument into 'q::Expression(const q::AbstractExpression&, const std::basic_string<...>&, const std::basic_string<...>&, const sol::basic_object<sol::basic_reference<0> >&)')`. A file-only method on an Expression, e:is_open(), gives `attempt to call a nil value (method 'is_open')`. |

### Observed error texts

| Scenario | Text |
|---|---|
| quiver.abs(42), both designs (pcall) | Cannot abs: operand must be an expression or a binary file, got number |
| quiver.abs('x') | Cannot abs: operand must be an expression or a binary file, got string |
| quiver.abs() | Cannot abs: operand must be an expression or a binary file, got nil |
| quiver.abs(f:metadata()) (another usertype) | Cannot abs: operand must be an expression or a binary file, got userdata |
| e + 'x' / f + 'x' / 'x' + f | Cannot add: operand must be an expression or a binary file, got string |
| f + db | Cannot add: operand must be an expression or a binary file, got userdata |
| f + {} | Cannot add: operand must be an expression or a binary file, got table |
| quiver.gt(e) | Cannot gt: operand must be an expression or a binary file, got nil |
| quiver.gt(1, 2) | Cannot gt: operand must be an expression or a binary file, got number |
| quiver.ifelse(f, 1, e) | Cannot ifelse: operand must be an expression or a binary file, got number |
| quiver.ifelse(f, e) | Cannot ifelse: operand must be an expression or a binary file, got nil |
| quiver.expression(42) | Cannot expression: operand must be an expression or a binary file, got number |
| WINNING: quiver.abs(e, 99) | Cannot abs: operand must be an expression or a binary file, got number |
| WINNING: quiver.abs(e, f) (extra argument is a valid expression) | Cannot abs: operand must be an expression or a binary file, got userdata |
| WINNING: quiver.gt(e, f, 3) | Cannot gt: operand must be an expression or a binary file, got number |
| WINNING: quiver.gt(e, f, f) | Cannot gt: operand must be an expression or a binary file, got userdata |
| RUNNER-UP: quiver.abs(e, 99) / quiver.abs(e, f) | Cannot abs: too many arguments (expected 1, got 2) |
| RUNNER-UP: quiver.gt(e, f, 3) / gt(e, f, f) / gt(e, 1, 'x') | Cannot gt: too many arguments (expected 2, got 3) |
| WINNING: getmetatable(e).__unm(5) (raw sol2; unary typed (A,A)) | [string "..."]:1: stack index 1, expected userdata, received number: value is not a valid userdata (bad argument into 'q::Expression(const q::AbstractExpression&, const q::AbstractExpression&)') |
| RUNNER-UP: getmetatable(e).__unm(5) (unary typed (A)) | [string "..."]:1: stack index 1, expected userdata, received number: value is not a valid userdata (bad argument into 'q::Expression(const q::AbstractExpression&)') |
| No reporting overload: single typed function abs_raw(42) | [string "..."]:1: stack index 1, expected userdata, received number: value is not a valid userdata (bad argument into 'q::Expression(const q::AbstractExpression&)') |
| No reporting overload: overload set add_raw(f, 'x') | [string "..."]:1: sol: no matching function call takes this number of arguments and the specified types |
| Dot-call f.aggregate('stage','sum') | [string "..."]:1: stack index 1, expected userdata, received string: value is not a valid userdata (bad argument into 'q::Expression(const q::AbstractExpression&, const std::basic_string<char,std::char_traits<char>,std::allocator<char> >&, const std::basic_string<char,std::char_traits<char>,std::allocator<char> >&, const sol::basic_object<sol::basic_reference<0> >&)') |
| Dot-call f.text() (shared method, no args) | [string "..."]:1: stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'std::basic_string<char,std::char_traits<char>,std::allocator<char> >(const q::AbstractExpression&)') |
| Dot-call f.is_open() (file-only method) | [string "..."]:1: stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'bool(q::BinaryFile&)') |
| Dot-call db.open_file('x') | [string "..."]:1: stack index 1, expected userdata, received string: value is not a valid userdata (bad argument into 'std::shared_ptr<q::BinaryFile>(q::Database&, const std::basic_string<char,std::char_traits<char>,std::allocator<char> >&)') |
| f.abs(f) (quiver.* function used as a file method) | [string "..."]:1: attempt to call a nil value (field 'abs') |
| e:is_open() (file-only method on an Expression) | [string "..."]:1: attempt to call a nil value (method 'is_open') |

### Corrections

- WINNING operand_error misreports extra arguments. Once all `arity` operands are valid, it reports the first extra argument as if it were a bad operand. quiver.abs(e, f) gives `Cannot abs: operand must be an expression or a binary file, got userdata`, which contradicts itself because the extra argument is an expression. quiver.gt(e, f, 3) gives `... got number`. Fix: when every operand within arity is valid and args.size() > arity, throw the runner-up's `Cannot <op>: too many arguments (expected N, got M)`. Alternatively, keep today's leniency by giving each typed candidate a trailing sol::variadic_args; that compiles and works (abs_lenient(e,99) succeeds, abs_lenient(42) still gives Pattern 1).
- WINNING's 'no new warnings at /W4' holds only in Debug. In Release /O2 the always-throwing variadic fallback adds about 12 C4702 'unreachable code' warnings in sol2 headers (stack.hpp:285 and function_types_overloaded.hpp:48). Today's real binary.cpp at /O2 has 0, and a fallback with a reachable return path also has 0. There is no /WX (cmake/CompilerOptions.cmake:4-14), so these are noise, not failures. Accept them, or suppress them (a never-taken return path, or #pragma warning(disable:4702) around the sol2 include). The runner-up's count of 8 differs because the count scales with the number of distinct overload sets.
- WINNING's risk list omits the sol::base_classes __index cost, which the spike measured. update_bases (usertype_storage.hpp:393-398) turns the __index of both BinaryFile and Expression from a table into a C function. Every method call, including the f:read/f:write hot path, pays about +80-110 ns in Release: shared_ptr userdata went from 94-106 to 179-194 ns per o:get(). Add it to the risks and check it with a before/after benchmark of the real quiver_cli, since the Do-Not-Fix list protects binary hot-path decisions.
- Citation nits. sol2's arity filter is at call.hpp:167-180 (runtime_variadics checks at :167/:179, `free_arity != fxarity` at :180), not 178-190. The class_cast in the getter is at stack_get_unqualified.hpp:905-916. The other citations check out: usertype_storage.hpp:386 (weak_derive), stack_check_unqualified.hpp:541-552, call.hpp:157, trampoline.hpp:116-127, lvm.c:1555/1566, lstrlib.c:277 and usertype.rst:288.
- Both designs: sol2's raw text for a dot-call without self, or a direct metamethod call, now names `quiver::AbstractExpression` in the bad-argument signature, e.g. `bad argument into 'q::Expression(const q::AbstractExpression&, const q::AbstractExpression&)'` for getmetatable(e).__unm(5). Both designs accept this; it is noted only so the docs do not promise Pattern 1 there.
- Not verified here, still open: GCC, Clang and MinGW, a QUIVER_API dllexport/visibility base across a real DLL boundary (the spike is a single executable, which matches the real layout where the Lua binding lives inside quiver.dll with the classes), Julia, the C API, and lua-api-sync.
