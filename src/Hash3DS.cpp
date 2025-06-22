/*
Copyright (C) 2024 Brian Weiss

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

#include "Util.h"

#include <rc_hash.h>
#include <rc_export.h>

#include <libmincrypt/sha256.h>

#include <string.h>

void rhash_log_error_message(const char* message); /* in Hash.c */

#undef DEBUG_AES_KEYS

/* for finding aes_keys.txt and seeddb.bin */
static std::string g_systemDir = ".";

static void rhash_read_128bit_hex(const char* hex, uint8_t key[16])
{
  char pair[4];
  int index;
  pair[2] = '\0';

  for (index = 0; index < 16; ++index)
  {
    pair[0] = *hex++;
    pair[1] = *hex++;

    key[index] = (uint8_t)strtol(pair, NULL, 16);
  }
}

static void rhash_rol_128bit(uint8_t key[16], int amount)
{
  uint8_t copy[16];
  int offset = amount / 8;
  int shift = amount % 8;
  int index;

  memcpy(copy, key, 16);
  for (index = 0; index < 16; ++index)
  {
    key[index] = (uint8_t)(copy[offset] << shift);
    offset = (offset + 1) % 16;
    key[index] |= (uint8_t)(copy[offset] >> (8 - shift));
  }
}

static void rhash_xor_128bit(uint8_t key[16], const uint8_t value[16])
{
  int index;
  for (index = 0; index < 16; ++index)
    key[index] ^= value[index];
}

static void rhash_add_128bit(uint8_t key[16], const uint8_t value[16])
{
  uint16_t carry = 0;
  int index;
  for (index = 15; index >= 0; --index)
  {
    carry += key[index] + value[index];
    key[index] = (uint8_t)(carry & 0xFF);
    carry >>= 8;
  }
}

#ifdef DEBUG_AES_KEYS
static void rhash_print_key(uint8_t key[16])
{
  printf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
    key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
}
#endif

static int rhash_3ds_normalize_keys(uint8_t keyX[16], uint8_t keyY[16], uint8_t keyN[16])
{
  if (keyX[0] && keyY[0])
  {
    uint8_t generator_constant[16] = { 0x1F, 0xF9, 0xE9, 0xAA, 0xC5, 0xFE, 0x04, 0x08, 0x02, 0x45, 0x91, 0xDC, 0x5D, 0x52, 0x76, 0x8A };

    memcpy(keyN, keyX, 16);
    rhash_rol_128bit(keyN, 2);
    rhash_xor_128bit(keyN, keyY);
    rhash_add_128bit(keyN, generator_constant);
    rhash_rol_128bit(keyN, 87);
    return 1;
  }

  return 0;
}

static int rhash_3ds_lookup_cia_normal_key(uint8_t index, uint8_t key[16])
{
  char scan[16];
  size_t scan_len;
  char buffer[128];
  uint8_t keyX[16];
  uint8_t keyY[16];
  char* line;

  FILE* fp = util::openFile(nullptr, g_systemDir + "/aes_keys.txt", "r");
  if (!fp)
  {
    rhash_log_error_message("Could not open aes_keys.txt");
    return 0;
  }

  keyX[0] = 0;
  keyY[0] = 0;
  scan_len = snprintf(scan, sizeof(scan), "common%u=", index);

  while ((line = fgets(buffer, sizeof(buffer), fp)))
  {
    if (memcmp(line, scan, scan_len) == 0)
    {
      rhash_read_128bit_hex(line + scan_len, keyY);
      if (keyX[0])
        break;
    }
    else if (memcmp(line, "slot0x3DKeyX=", 13) == 0)
    {
      rhash_read_128bit_hex(line + 13, keyX);
      if (keyY[0])
        break;
    }
  }

  fclose(fp);

  return rhash_3ds_normalize_keys(keyX, keyY, key);
}

static int rhash_3ds_lookup_ncch_normal_key(uint8_t primaryKeyY[16],
  uint8_t secondaryKeyXSlot, uint8_t* programId,
  uint8_t primaryKeyOut[16], uint8_t secondaryKeyOut[16])
{
  char scan[16];
  char buffer[128];
  uint8_t primaryKeyX[16];
  uint8_t secondaryKeyX[16];
  uint8_t secondaryKeyY[16];
  char* line;

  FILE* fp = util::openFile(nullptr, g_systemDir + "/aes_keys.txt", "r");
  if (!fp)
  {
    rhash_log_error_message("Could not open aes_keys.txt");
    return 0;
  }

  primaryKeyX[0] = 0;
  secondaryKeyX[0] = 0;
  snprintf(scan, sizeof(scan), "slot0x%02XKeyX=", secondaryKeyXSlot);

  while ((line = fgets(buffer, sizeof(buffer), fp)))
  {
    if (memcmp(line, "slot0x2CKeyX=", 13) == 0)
    {
      rhash_read_128bit_hex(line + 13, primaryKeyX);
      if (secondaryKeyX[0])
        break;

      if (secondaryKeyXSlot == 0x2C)
      {
        memcpy(secondaryKeyX, primaryKeyX, sizeof(secondaryKeyX));
        break;
      }
    }
    else if (memcmp(line, scan, 13) == 0)
    {
      rhash_read_128bit_hex(line + 13, secondaryKeyX);
      if (primaryKeyX[0])
        break;
    }
  }

  fclose(fp);

  if (!rhash_3ds_normalize_keys(primaryKeyX, primaryKeyY, primaryKeyOut))
    return 0;

#ifdef DEBUG_AES_KEYS
  printf("Primary key: "); rhash_print_key(primaryKeyOut); printf("\n");
#endif

  if (!programId)
  {
    memcpy(secondaryKeyY, primaryKeyY, sizeof(secondaryKeyY));
  }
  else
  {
    uint32_t count;
    SHA256_CTX ctx;

    /* find the seed for the programId */
    fp = util::openFile(nullptr, g_systemDir + "/seeddb.bin", "rb");
    if (!fp)
    {
      rhash_log_error_message("Could not open seeddb.bin");
      return 0;
    }

    /* seeddb.bin's layout is simply the first 4 bytes indicate the amount of seeds in the
     * file, followed by 12 bytes of padding. Then a collection of seeds in the format of
     * 8 bytes for the program id, then 16 bytes for the seed, then 8 bytes of padding */
    fread(&count, sizeof(count), 1, fp);
    fseek(fp, 12, SEEK_CUR);

    for (; count > 0; count--)
    {
      fread(buffer, sizeof(uint8_t), 8, fp);
      if (memcmp(buffer, programId, 8) == 0)
      {
        fread(secondaryKeyY, sizeof(uint8_t), sizeof(secondaryKeyY), fp);
        break;
      }
      fseek(fp, 16 + 8, SEEK_CUR);
    }

    fclose(fp);

    if (count == 0) /* did not find programId in seeddb.bin */
      return 0;

    /* the actual secondaryKeyY used to generate the normalized key is the first 16 bytes
     * of the SHA256 of the primaryKeyY and the seed pulled from seeddb.bin */
    SHA256_init(&ctx);
    SHA256_update(&ctx, primaryKeyY, 16);
    SHA256_update(&ctx, secondaryKeyY, 16);
    memcpy(secondaryKeyY, SHA256_final(&ctx), sizeof(secondaryKeyY));
  }

  if (!rhash_3ds_normalize_keys(secondaryKeyX, secondaryKeyY, secondaryKeyOut))
    return 0;

#ifdef DEBUG_AES_KEYS
  printf("Secondary key: "); rhash_print_key(secondaryKeyOut); printf("\n");
#endif

  return 1;
}

void initHash3DS(const std::string& systemDir)
{
  g_systemDir = systemDir;

  rc_hash_init_3ds_get_cia_normal_key_func(rhash_3ds_lookup_cia_normal_key);
  rc_hash_init_3ds_get_ncch_normal_keys_func(rhash_3ds_lookup_ncch_normal_key);
}
