

# Shell and construction flow

Modes can define how the db should be built as presets:
- `--mode user`
- `--mode transaction`
- `--mode ...`

Argument parsing and startup orchestration live in `src/shell`.

Flow:
1. Ensure `db/` directory exists.
2. Open `master.db` or create it if missing.
3. Load the master registry into RAM.
4. Scan `db/` for `.db` files.
5. Import files that are not mapped in `master.db`:
   - mark them as `unmapped`
   - try to read `metadata` (`uuid`, `creation_date`)
   - if metadata is missing, use filename stem as `uuid` and file timestamp as `creation_date`
6. If `--no-import` is set, skip step 5.
7. If `-c X` is set:
   - if X > mapped count: create and register additional databases until mapped count == X
   - if X < mapped count: warn and keep existing databases
8. Re-check missing mapped files and recreate them if needed.
9. Load entries into RAM.
10. Open network handler or later modules.
