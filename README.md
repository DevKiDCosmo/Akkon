# Akkon

Akkon manages multiple SQLite database files in a shared `db/` directory.

## Build

```bash
cd /Users/duynamschlitz/CLionProjects/Akkon
rm -rf cmake-build-debug
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
```

## Run

```bash
./Akkon -c 3
./Akkon -c 3 --no-import
./Akkon -c 2 --debug
./Akkon --help
ctest --output-on-failure
ctest -L benchmark --output-on-failure
```

## Features

- **Memory Domain Isolation**: RUNTIME_META (program state) and RUNTIME_REQUEST (transient data)
- **Arena Allocator**: Efficient O(1) allocation and reset for each domain
- **Argument Registry**: Type-safe command-line argument registration with metadata
- **Debug Mode**: Warns on unregistered arguments instead of erroring (with `--debug`)
- **CTest Benchmark**: `benchmark_1m` is registered as a CTest target

## Notes

- Databases are created as UUID-named files in `db/`.
- `master.db` stores the registry.
- Unmapped `.db` files in `db/` are imported with status `unmapped` unless `--no-import` is set.


DESIGN PHILOSOPHY:

All information that the user can give or must give to identify are always hashed. Like email, pw etc.
All information that must be retrieved must be encoded or in plain text or encrypted in a ways or another.


--mode user etc are used for predefined setting for db. instead of --modex scrip file.