#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RAHASHER_FILE_OK 0
#define RAHASHER_FILE_ERR_INVALID_ARG -1
#define RAHASHER_FILE_ERR_IO -2
#define RAHASHER_FILE_ERR_HASH -3
#define RAHASHER_FILE_ERR_UNSUPPORTED -4

/*
 * Attempts to compute the RetroAchievements hash of file_path for the given console.
 *
 * Stateless: opens, reads, and closes file_path fresh on every call. The caller owns
 * file_path's entire lifecycle (creation, writing, deletion) -- this never creates,
 * writes to, or deletes any file, and correctly resolves sibling files for formats
 * that reference them by a relative path (.cue's .bin tracks, .m3u playlists,
 * .iso/.chd disc images), since file_path is always a real path in its real directory.
 *
 * file_path does not need to be fully written yet: pass expected_total_bytes as the
 * file's eventual final size (0 if unknown), then call again later -- after writing
 * more to file_path -- whenever *out_done comes back false.
 *
 * out_hash must point to a 33-byte buffer; only written when *out_done is set to true.
 * *out_done is set to true when the hash completed (out_hash is valid) or false when
 * the format's hash algorithm needed data beyond what's currently on disk. This
 * distinction requires expected_total_bytes != 0; with expected_total_bytes == 0, a
 * short read is always treated as a genuine end of data (*out_done is never false).
 *
 * Returns RAHASHER_FILE_OK (0) for both non-error outcomes above -- check *out_done to
 * tell them apart. A negative return is a genuine failure (corrupt/unsupported data);
 * *out_done is left false and out_hash is untouched. See rahasher_get_last_error().
 *
 * Not safe to call concurrently with itself on the same thread's worth of state (it
 * shares one hashing pipeline with the rest of RAHasher); serialize calls, e.g. one
 * hashing thread per file.
 */
int rahasher_hash_file(
  uint32_t console_id,
  const char* file_path,
  const char* system_dir,
  uint64_t expected_total_bytes,
  char out_hash[33],
  int* out_done);

/* Message for the most recent negative return from rahasher_hash_file on this thread. */
const char* rahasher_get_last_error(void);

#ifdef __cplusplus
}
#endif
