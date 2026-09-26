# RAHasher file dylib

This user-local implementation exposes a stateless, file-path-based hashing API and
builds a shared library without changing existing src/ code.

## Output layout
- user/release/{platform}/{arch}/{version}/dynamic/libRAHasher.{dylib|so|dll}
- user/release/{platform}/{arch}/{version}/include/rahasher_stream.h
- user/release/{platform}/{arch}/{version}/static/libRAHasher.a
- user/release/{platform}/{arch}/{version}/bin/rahasher_stream_sample
- intermediate objects: user/_build

## Build
- Auto target detect, dynamic by default:
  - ./user/build_rahasher.sh
- Explicit target/type/version:
  - ./user/build_rahasher.sh --target mac --type all --version v1
  - note: version is currently ignored in output path layout
- Windows: run from an MSYS2/MinGW shell with mingw-w64 gcc/g++ on PATH (e.g. the
  "MSYS2 MinGW x64" shortcut). The dynamic build statically links the mingw-w64
  runtime, so libRAHasher.dll has no external dependency on
  libgcc_s_seh-1.dll/libstdc++-6.dll/libwinpthread-1.dll being present on the loading
  machine. x64 builds are capped at AVX2 (not AVX-512), since AVX-512 support depends
  on the CPU that runs the binary, not on whether the build machine's compiler can
  assemble it.

## Supported target/arch mapping
- mac, iphone, android: arm64
- windows, linux: x64

## API summary
Header: user/rahasher_stream.h
- rahasher_hash_file(console_id, file_path, system_dir, expected_total_bytes, out_hash, out_done)
- rahasher_get_last_error()

Stateless: every call opens, reads, and closes file_path fresh -- there is no context
handle to create or destroy. The caller owns file_path's entire lifecycle (creation,
writing, deletion); this library never creates, writes to, or deletes any file. Because
file_path is always a real path in its real directory, sibling files referenced by
relative path (.cue's .bin tracks, .m3u playlists, .iso/.chd disc images) resolve
correctly -- unlike feeding raw bytes through a copy in a throwaway temp directory.

## Progressive hashing (file still being written)
file_path does not need to be fully written yet. Pass expected_total_bytes as the
file's eventual final size (0 if unknown):
- If the file's current on-disk size is below expected_total_bytes, the call returns
  immediately with *out_done = 0 (not an error) without attempting to hash, so a
  whole-file-hash format (Game Boy, NES, SNES, etc. -- these determine their own size
  via a plain seek-to-end on whatever currently exists, so they would otherwise
  "succeed" with a wrong hash computed from partial data).
- Once the file has reached expected_total_bytes, rahasher_hash_file actually attempts
  the hash. If the specific console/format's algorithm still needs to read past what's
  currently on disk for some other reason (e.g. a .cue's referenced .bin track not
  being fully written), *out_done is again set to 0 rather than erroring.
- *out_done = 1 means out_hash is valid and the hash is complete.
- A negative return value is a genuine failure (corrupt or unsupported data); *out_done
  is left false and out_hash is untouched. See rahasher_get_last_error().
- With expected_total_bytes == 0 (unknown), *out_done is never false -- behavior
  matches a plain one-shot hash of whatever currently exists at file_path.

## Behavior notes
- .m3u input is not supported; provide the referenced disc file instead.
- Console id must be specific (<= 90 in current rcheevos mapping path used here).
- Not safe to call concurrently with itself; serialize calls (e.g. one hashing thread
  per file) -- rc_hash_generate relies on process-wide static state inside rcheevos.

## Sample
- Source: user/sample_stream_client.cpp
- Build sample binary:
  - make -f user/Makefile.rahasher bin TARGET_PLATFORM=mac TARGET_ARCH=arm64 VERSION=dev

## Tests
- Tests are in user/tests.
- Run:
  - ./user/tests/run_tests.sh
