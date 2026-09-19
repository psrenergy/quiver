# Phase 1: Enum Labels in Describe - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-19
**Phase:** 1-Enum Labels in Describe
**Areas discussed:** Exact render format, Phase 1 public surface / UI metadata design, Fixture corpus design, Degradation loudness

---

## Exact render format

### Where the enum vocabulary and full value list go

| Option | Description | Selected |
|--------|-------------|----------|
| Inline on the scalar line | Shortest diff to `write_collection_section`; a 10-value vocabulary makes a long line and a shared vocabulary repeats per column | ✓ |
| Indented sub-line | Keeps scalar lines short, vocabulary adjacent to its column | |
| Trailing `Enums:` section | Deduplicates (127 bindings over 62 vocabularies); costs a lookup hop | |

**User's choice:** Inline on the scalar line.

### Does `describe()` get the same per-scalar detail?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes — one shared renderer | No branching in `write_collection_section`; long on a 25-collection model but one call gives an agent everything | ✓ |
| No — collection labels only | Keeps the overview an overview; needs a flag and N+1 calls | |
| Yes, but enums abbreviated | Vocabulary name only in `describe()`; a third format to assert | |

**User's choice:** Yes — one shared renderer.

### A data code the vocabulary does not declare

| Option | Description | Selected |
|--------|-------------|----------|
| Mark it explicitly — `2: 1 (undeclared)` | Distinguishes "no label for 2" from "no vocabulary at all"; serves the accepted-risk posture | ✓ |
| Leave it bare — `2: 1` | Simplest rule; ambiguous with a partially-configured column | |
| Question mark — `2: 1 (?)` | Same information, shorter, less self-explanatory to an LLM | |

**User's choice:** Mark it explicitly.

### Which reports carry the header line

| Option | Description | Selected |
|--------|-------------|----------|
| All three reports | An agent calling only `describe_collection` still knows a config loaded and from where; three byte-identical baselines to verify | ✓ |
| `describe()` only | Exactly what DESC-04 says; smallest blast radius | |
| `describe()` + `describe_collection()` | Hardest rule to justify in a test | |

**User's choice:** All three reports.

### Header line text

| Option | Description | Selected |
|--------|-------------|----------|
| `UI config: <path> (locale: en)` | Matches the existing `Database:` / `Version:` `Key: value` shape | ✓ |
| `UI: <path> [en]` | Terser, bracket matches `[MW]`/`[hidden]`; a bare `[en]` could read as a tag | |
| `… (locale: en, N vocabularies)` | Adds a load receipt; duplicates Phase 3's list-vocabularies getter | |

**User's choice:** `UI config: <path> (locale: en)`.

**Notes:** Scalar-line token order was *derived*, not asked — append-only ordering is forced by
DESC-05, and it keeps today's line a literal prefix so existing `contains` assertions hold.
User then chose "Next area" over more format questions.

---

## Phase 1 public surface / UI metadata design

This area was not resolved by a single question. The user interrupted the first question set to
propose that UI metadata live on the existing `ScalarMetadata` struct, and the area became an
extended design discussion. Four AskUserQuestion rounds were rejected by the user in favour of
open discussion; the record below is the argument, not a table.

### Step 1 — Was the recorded constraint blocking?

The user's proposal appeared blocked by PROJECT.md's "do **not** add fields to `ScalarMetadata` /
`GroupMetadata`" with the `SCALAR_METADATA_SIZE = 56` rationale. A 7-agent cross-layer audit found
the constraint is **wrong as written**: it names the C++ types while describing the C ones. No
binding can see `quiver::ScalarMetadata`; `sizeof` it is already ~200 bytes. **Not** a blocker.

### Step 2 — User's objection that changed the design

The user challenged `enum_name` + `enum_values` as two fields on one record. Upheld: it makes
illegal states representable (named-but-empty is VALID-02's drift; unset-but-populated is
nonsense) and denormalises a shared entity. This became **D-10** (vocabulary by reference).

### Step 3 — Adversarial design panel

The user asked for an unbiased answer. An 8-agent panel ran: four blind designers, two adversaries
tasked with refuting the incumbent, one feasibility probe, one judge at max effort.

| Design | model | deps | cost | validator | readability | Selected |
|--------|-------|------|------|-----------|-------------|----------|
| Two Records, One Seam | 8 | 9 | 9 | 9 | 8 | ✓ (+ graft) |
| Sidecar Sibling | 8 | 9 | 8 | 7 | 9 | (vocabulary C surface grafted) |
| UI Catalog — temp relations | 4 | 7 | 9 | 10 | 3 | |
| Sidecar in a String | 5 | 8 | 9 | 8 | 4 | |
| **Incumbent — composed optional** | 3 | 2 | 2 | 5 | 4 | rejected |

**Why the incumbent lost** (all three verified against source, not asserted):
1. `GroupMetadata::value_columns` is `vector<ScalarMetadata>` — `ui` lands on every group value
   column, unfillable by four of five producers.
2. The boasted 56→48 shrink is the **out-of-bounds** direction — `SCALAR_METADATA_SIZE` is an
   array stride, not just an out-buffer.
3. "One call returns everything" never reaches `describe_collection`, which reads
   `ColumnDefinition` directly.

**Notes:** The relational option was *not* blocked on feasibility — verified `read_only` uses
`SQLITE_OPEN_READONLY` (temp tables permitted) and `Schema` reads `sqlite_master` (no pollution).
It lost on the consumer-facing read path: `execute` is private and `query_*` returns one cell.
Its validator SQL was the panel's strongest single artifact and is flagged for Phase 5.

### Step 4 — Consumer premise check

Reading `Claw/claw/src/core/study-config.ts` (68 lines) showed it reads only `main.model`,
`main.processes` and collection `id`s, and states per-attribute semantics are intentionally not
read there. So PROJECT.md's "per-attribute half of `study-config.ts`" does not exist.

| Option | Description | Selected |
|--------|-------------|----------|
| Keep Phase 3 — a consumer exists | User knows of a consumer the source does not yet show | ✓ |
| Demote behind Phase 4 | Collection-level is what claw would actually drop code for | |
| Cut META to v2 | Biggest scope reduction; most literal reading of the evidence | |

**User's choice:** Keep it — there is a consumer I know of.

---

## Fixture corpus design

| Option | Description | Selected |
|--------|-------------|----------|
| Hand-written miniatures, one tolerance each | A failing fixture names the broken rule; risk that a miniature is easier than reality | ✓ |
| Near-verbatim slices of real repos | Proves the parser against shapes that exist; a failure does not say which rule broke | |
| Both | What CORPUS-01 + CORPUS-02 literally ask for; largest surface | |

**User's choice:** Hand-written miniatures, one tolerance each.

**Notes:** CORPUS-01 names BESSOperation / Foresight / HydroThermalDispatch explicitly, so three
of the miniatures must carry those three cases (all-bare-string, mixed en/es/pt, plain
`date_time` attribute) to satisfy it — recorded as D-27.

### Test harness — reaching `<db_dir>/ui/`

| Option | Description | Selected |
|--------|-------------|----------|
| Per-suite helper copies to a temp dir | Single source, seven thin helpers, all obsolete once OPT-01 lands | |
| Build the database inside the fixture directory | No copy step; writes generated files into `tests/schemas/`; parallel-run collision risk | ✓ |
| Reorder — land OPT-01 first | Cleanest tests; drags the ABI break into Phase 1 and forfeits ships-as-a-patch | |

**User's choice:** Build the database inside the fixture directory.

**Notes:** Collision risk raised and mitigated in D-29 — each suite uses its own db filename
inside the shared fixture dir, all gitignored.

---

## Degradation loudness

| Option | Description | Selected |
|--------|-------------|----------|
| Absent = debug, malformed = warn | Absence is normal for every non-PSR database; amends PARSE-11's wording | ✓ |
| Both warn, as PARSE-11 says | Literal to the requirement; noise on every non-PSR open at default `Info` | |
| Absent = silent, malformed = warn | Quietest; a moved `ui/` gives no hint at all | |

**User's choice:** Absent = debug, malformed = warn.

**Notes:** Whole-config-fails-on-one-broken-file (D-25) and per-file unknown-key logging (D-26)
were derived from PARSE-12 and corpus size rather than asked.

---

## Claude's Discretion

- Exact wording of the `(undeclared)` marker and the `enum <vocab> {…}` separator.
- Blank-line placement around `describe()`'s header line.
- Internal layout of `src/ui_config.cpp` (single file vs. parser/validator split).
- **Unresolved and escalated to the planner:** whether the four weak binding describe assertions
  are strengthened, annotated, or deleted (D-31). The roadmap demands an explicit written call;
  it was not made in discussion.

## Deferred Ideas

- `is_foreign_key` → `optional<ForeignKeyRef>` — own PR, after Phase 2's SAFE-01..03.
- Correcting the 56/32 constraint text in `PROJECT.md` and `.claude/CLAUDE.md`.
- Recording the `libquiver`/`libquiver_c` by-value ABI hazard (written down nowhere today).
- Correcting PROJECT.md's "committed consumer" justification for Phase 3.
- Static `validate_ui_config(ui_directory, schema_path)` overload for GOV-02 — revisit at Phase 5.
- A `main.model` / `main.processes` getter — what claw would actually drop code for; raise before
  Phase 4 planning.
- The UI Catalog's validator SQL — revisit at Phase 5 as an implementation technique.
