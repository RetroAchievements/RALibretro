/*
Copyright (C) 2022 Jamiras

This file is part of RALibretro.

RALibretro is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RALibretro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CHD

#include <rc_hash.h>

#include <libchdr/include/libchdr/chd.h>

#include <memory.h>
#include <stdlib.h>
#include <string.h>

typedef struct chd_track_handle_t
{
  chd_file* file;              /* CHD file handle */
  uint8_t *hunkmem;            /* Loaded hunk data */
  uint32_t hunknum;            /* Loaded hunk number */
  uint32_t frames_per_hunk;    /* Number of frames per hunk */
  uint32_t first_sector;       /* First sector associated to the track */
  uint32_t first_frame;        /* First CHD frame associated to the track */
  uint32_t sector_size;        /* Size of each sector */
  uint32_t sector_header_size; /* Size of header data for each sector */
} chd_track_handle_t;

typedef struct metadata
{
   uint32_t frames;
   uint32_t pad;
   uint32_t pregap;
   uint32_t postgap;
   uint32_t track;
   uint32_t sector_offset;
   uint32_t frame_offset;
   char type[64];
   char subtype[32];
   char pgtype[32];
   char pgsub[32];
} metadata_t;

#ifdef _MSC_VER
 #pragma warning(push)
 #pragma warning(disable : 4996)
#endif

static bool rc_hash_get_chd_metadata(chd_file* file, uint32_t idx, metadata_t* metadata)
{
  char meta[256];
  uint32_t meta_size;
  chd_error err;

  err = chd_get_metadata(file, CDROM_TRACK_METADATA2_TAG, idx, meta, sizeof(meta), &meta_size, NULL, NULL);
  if (err == CHDERR_NONE)
  {
    sscanf(meta, CDROM_TRACK_METADATA2_FORMAT, &metadata->track, metadata->type,
           metadata->subtype, &metadata->frames, &metadata->pregap, metadata->pgtype, metadata->pgsub,
           &metadata->postgap);
    return true;
  }

  err = chd_get_metadata(file, CDROM_TRACK_METADATA_TAG, idx, meta, sizeof(meta), &meta_size, NULL, NULL);
  if (err == CHDERR_NONE)
  {
    sscanf(meta, CDROM_TRACK_METADATA_FORMAT, &metadata->track, metadata->type,
           metadata->subtype, &metadata->frames);
    return true;
  }

  err = chd_get_metadata(file, GDROM_TRACK_METADATA_TAG, idx, meta, sizeof(meta), &meta_size, NULL, NULL);
  if (err == CHDERR_NONE)
  {
    sscanf(meta, GDROM_TRACK_METADATA_FORMAT, &metadata->track, metadata->type,
           metadata->subtype, &metadata->frames, &metadata->pad, &metadata->pregap, metadata->pgtype,
           metadata->pgsub, &metadata->postgap);
    return true;
  }

  return false;
}

#ifdef _MSC_VER
 #pragma warning(pop)
#endif

static bool rc_hash_find_chd_track(chd_file* file, uint32_t track, metadata_t* metadata)
{
  uint32_t largest_size = 0;
  uint32_t largest_idx = 0;
  uint32_t sector_offset = 0;
  uint32_t frame_offset = 0;
  uint32_t padding_frames = 0;
  uint32_t idx;

  /* CHD doesn't keep track of sessions. Assume the first session is a single track and hope for the best */
  if (track == RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION)
    track = 2;

  for (idx = 0; true; idx++)
  {
    if (!rc_hash_get_chd_metadata(file, idx, metadata))
      break;

    /* calculate the actual sector offset of the track */
    metadata->sector_offset = sector_offset;
    sector_offset += metadata->frames;

    /* calculate the frame offset within the CHD. this logic is stolen from the
     * RetroArch implementation. I don't see anything in the CHD documentation
     * that explains the need for it. Apparently each track is padded to a
     * multiple of 4 frames, regardless of the number of frames in a hunk. */
    frame_offset += metadata->pregap;
    metadata->frame_offset = frame_offset;
    padding_frames = ((metadata->frames + 3) & ~3) - metadata->frames;
    frame_offset += metadata->frames + padding_frames;

    if (metadata->track == track)
      return true;

    if (strcmp(metadata->type, "AUDIO") == 0)
      continue;

    if (track == RC_HASH_CDTRACK_FIRST_DATA)
      return true;

    if (metadata->frames > largest_size)
    {
      largest_size = metadata->frames;
      largest_idx = idx;
    }
  }

  switch (track)
  {
    case RC_HASH_CDTRACK_LAST:
      return true;

    case RC_HASH_CDTRACK_LARGEST:
      if (idx == largest_idx)
        return true;

      return rc_hash_get_chd_metadata(file, largest_idx, metadata);

    default:
      return false;
  }
}

static size_t rc_hash_handle_chd_read_sector(void* track_handle, uint32_t sector,
      void* buffer, size_t requested_bytes)
{
  chd_track_handle_t* chd_track = (chd_track_handle_t*)track_handle;
  const chd_header* header = chd_get_header(chd_track->file);
  uint32_t hunk, offset, chd_frame;
  size_t bytes_read = 0;

  if (sector < chd_track->first_sector)
    return 0;

  /* convert the real sector to the chd frame and then use that to find the hunk that contains it */
  chd_frame = chd_track->first_frame + (sector - chd_track->first_sector);
  hunk = chd_frame / chd_track->frames_per_hunk;
  offset = (chd_frame % chd_track->frames_per_hunk) * header->unitbytes + chd_track->sector_header_size;

  do {
    if (hunk != chd_track->hunknum)
    {
      if (chd_read(chd_track->file, hunk, chd_track->hunkmem) != CHDERR_NONE)
        return bytes_read;

      chd_track->hunknum = hunk;
    }

    if (requested_bytes <= chd_track->sector_size)
    {
      memcpy(buffer, &chd_track->hunkmem[offset], requested_bytes);
      bytes_read += requested_bytes;
      break;
    }

    memcpy(buffer, &chd_track->hunkmem[offset], chd_track->sector_size);
    bytes_read += chd_track->sector_size;
    buffer = ((uint8_t*)buffer) + chd_track->sector_size;
    requested_bytes -= chd_track->sector_size;

    offset += header->unitbytes;
    if (offset > header->hunkbytes)
    {
      offset = chd_track->sector_header_size;
      hunk++;
    }
  } while (true);

  return bytes_read;
}

static uint32_t rc_hash_handle_chd_first_track_sector(void* track_handle)
{
  chd_track_handle_t* chd_track = (chd_track_handle_t*)track_handle;
  return chd_track->first_sector;
}

static void rc_hash_handle_chd_close_track(void* track_handle)
{
  chd_track_handle_t* chd_track = (chd_track_handle_t*)track_handle;
  if (chd_track)
  {
    if (chd_track->hunkmem)
      free(chd_track->hunkmem);

    chd_close(chd_track->file);
    free(chd_track);
  }
}

static void* rc_hash_handle_chd_open_track(const char* path, uint32_t track)
{
  chd_track_handle_t* chd_track;
  chd_file* file;
  const chd_header* header;
  metadata_t metadata;
  uint8_t buffer[32];

  chd_error err = chd_open(path, CHD_OPEN_READ, NULL, &file);
  if (err != CHDERR_NONE)
    return NULL;

  memset(&metadata, 0, sizeof(metadata));
  if (!rc_hash_find_chd_track(file, track, &metadata))
  {
    chd_close(file);
    return NULL;
  }

  header = chd_get_header(file);

  chd_track = (chd_track_handle_t*)calloc(1, sizeof(chd_track_handle_t));
  chd_track->file = file;
  chd_track->hunknum = (uint32_t)-1;
  chd_track->hunkmem = (uint8_t*)malloc(header->hunkbytes);
  chd_track->frames_per_hunk = header->hunkbytes / header->unitbytes;
  chd_track->first_sector = metadata.sector_offset;
  chd_track->first_frame = metadata.frame_offset;

  if (strcmp(metadata.type, "MODE1_RAW") == 0 ||
      strcmp(metadata.type, "MODE2_RAW") == 0)
  {
    chd_track->sector_size = 2352;
  }
  else
  {
    chd_track->sector_size = header->unitbytes;
  }

  /* read the first 32 bytes of sector 16 (TOC) so we can attempt to identify the disc format */
  if (rc_hash_handle_chd_read_sector(chd_track, chd_track->first_sector + 16, buffer, sizeof(buffer)) != sizeof(buffer))
  {
    rc_hash_handle_chd_close_track(chd_track);
    return NULL;
  }

  /* if this is a CDROM-XA data source, the "CD001" tag will be 25 bytes into the sector */
  if (buffer[25] == 0x43 && buffer[26] == 0x44 &&
      buffer[27] == 0x30 && buffer[28] == 0x30 && buffer[29] == 0x31)
  {
    chd_track->sector_size = 2352;
    chd_track->sector_header_size = 24;
  }
  /* otherwise it should be 17 bytes into the sector */
  else if (buffer[17] == 0x43 && buffer[18] == 0x44 &&
      buffer[19] == 0x30 && buffer[20] == 0x30 && buffer[21] == 0x31)
  {
    chd_track->sector_size = 2352;
    chd_track->sector_header_size = 16;
  }
  /* if we didn't find a CD001 tag, this format may predate ISO-9660 */
  /* ISO-9660 says the first twelve bytes of a sector should be the sync pattern 00 FF FF FF FF FF FF FF FF FF FF 00 */
  else if (buffer[0] == 0 && buffer[1] == 0xFF && buffer[2] == 0xFF && buffer[3] == 0xFF &&
      buffer[4] == 0xFF && buffer[5] == 0xFF && buffer[6] == 0xFF && buffer[7] == 0xFF &&
      buffer[8] == 0xFF && buffer[9] == 0xFF && buffer[10] == 0xFF && buffer[11] == 0)
  {
    /* after the 12 byte sync pattern is three bytes identifying the sector and then one byte for the mode (total 16 bytes) */
    chd_track->sector_size = 2352;
    chd_track->sector_header_size = 16;
  }
  else
  {
    /* no header information found, attempt to determine from file size */
    if ((header->logicalbytes % 2352) == 0)
    {
      /* raw tracks use all 2352 bytes and have a 24 byte header */
      chd_track->sector_size = 2352;
      chd_track->sector_header_size = 24;
    }
    else if ((header->logicalbytes % 2048) == 0)
    {
      /* cooked tracks eliminate all header/footer data */
      chd_track->sector_size = 2048;
      chd_track->sector_header_size = 0;
    }
    else if ((header->logicalbytes % 2336) == 0)
    {
      /* MODE 2 format without 16-byte sync data */
      chd_track->sector_size = 2336;
      chd_track->sector_header_size = 8;
    }
  }

  return chd_track;
}

void rc_hash_init_chd_cdreader()
{
  struct rc_hash_cdreader cdreader;

  memset(&cdreader, 0, sizeof(cdreader));
  cdreader.open_track = rc_hash_handle_chd_open_track;
  cdreader.read_sector = rc_hash_handle_chd_read_sector;
  cdreader.close_track = rc_hash_handle_chd_close_track;
  cdreader.first_track_sector = rc_hash_handle_chd_first_track_sector;
  rc_hash_init_custom_cdreader(&cdreader);
}

#endif /* HAVE_CHD */
