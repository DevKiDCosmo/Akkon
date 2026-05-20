# Construction flow

This file mirrors the high-level flow and modes for database construction.

Modes can define how the db should be built as presets:
- --mode user
- --mode transaction
- --mode ...

Flow:
1. Ensure `db/` directory exists.
2. Open (or create) `master.db` and ensure the registry table exists.
3. Fetch registry entries from `master.db`.
4. Validate that mapped database files exist in the `db/` directory.
5. Recreate missing mapped databases (and restore `metadata` table) if needed.
6. Scan the `db/` directory for database files not present in `master.db` and import them
   into the registry with `status='unmapped'`.
   - Imported DBs are first checked for a `metadata` table (`uuid`, `creation_date`).
   - If metadata is absent, the filename stem is used as `uuid` and the filesystem timestamp
     is used for `creation_date`.
7. If `--no-import` is enabled, skip step 6.
8. When `-c X` is provided:
   - If X > mapped_count: create and register additional databases until mapped_count == X.
   - If X < mapped_count: warn and keep existing mapped databases.
9. Load registry entries into RAM for further processing.
10. (Optional) Initialize network handlers or other modules.

Notes:
-- Files found inside `db/` that are not mapped in `master.db` are now imported automatically and marked with status `unmapped` unless `--no-import` is set.
- All created databases are named by UUID (`<uuid>.db`) and contain a `metadata` table with uuid, creation_date and file_path.

