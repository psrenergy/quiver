---
phase: 04-struct-marshaling-indirect-modules
plan: 01
status: complete
started: 2026-04-17T22:00:00Z
completed: 2026-04-17T22:30:00Z
---

## Summary

Rewrote `bindings/js/src/metadata.ts` to replace all koffi.decode calls with Deno `UnsafePointerView` at verified byte offsets. ScalarMetadata (56 bytes) and GroupMetadata (32 bytes) struct fields are now read directly from native memory using `getPointer`, `getInt32`, `getBigUint64`, and `getCString` at exact byte offsets matching the C struct layouts.

## Key Changes

- Replaced `readStrAt`/`readNullableStrAt` (koffi-based) with `readNullableString` using `UnsafePointerView.getPointer` + null check
- Rewrote `readScalarMetadataAt` with byte offsets: name=0, dataType=8, notNull=12, primaryKey=16, defaultValue=24, isForeignKey=32, referencesCollection=40, referencesColumn=48
- Rewrote `readGroupMetadataAt` with byte offsets: groupName=0, dimensionColumn=8, valueColumns=16, valueColumnCount=24, including nested ScalarMetadata array traversal
- Rewrote all 4 `get*Metadata` methods to use `Uint8Array` output buffers + `Deno.UnsafePointer.of()` instead of `Buffer.alloc`
- Rewrote `listMetadata` generic to use `toCString` for collection strings and `UnsafePointerView` for readers
- Updated `declare module` path to `"./database.ts"`
- All imports changed from `.js` to `.ts` extensions

## Self-Check: PASSED

- Zero koffi imports in metadata.ts
- Zero Buffer references in metadata.ts
- Zero .js import extensions in metadata.ts
- `deno check` passes on metadata.ts

## Key Files

### Created
(none)

### Modified
- `bindings/js/src/metadata.ts` — Full rewrite from koffi to UnsafePointerView struct readers
