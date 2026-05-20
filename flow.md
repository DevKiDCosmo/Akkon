

# Modes can define how the db should be build as preset
--mode user
--mode transaction
--mode ...

--modeX "table.akkon"

1. Ensure `db/` directory exists and `master.db` location.
2. Fetch all `.db` files inside `db/`.
3. Open `master.db` and read registry to match and check.
4. Check for missing mapped databases and for db files not mapped in `master.db`.
5. Import unmapped `.db` files into `master.db` with `status='external'`:
   - Try to read a `metadata` table (uuid, creation_date) from each file.
   - If metadata missing, use the filename stem as `uuid` and file timestamp as `creation_date`.
6. If `-c X` is set:
   - If X > mapped_count: create and register additional databases until mapped_count == X.
   - If X < mapped_count: warn and keep existing mapped databases (no deletion).
7. Load registry entries into RAM.
8. Open network handler or other modules.
