/*
Copyright (C) 2018 Andre Leiradella

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

#include "Memory.h"

#include "Application.h"

#include <RA_Interface.h>
#include <rcheevos.h>
#include <rcheevos/src/rc_libretro.h>

#include <time.h>

#define TAG "[MEM] "

static rc_libretro_memory_regions_t g_memoryRegions;
static int g_memoryBankFirstRegion[16];

static unsigned char memoryRead(unsigned addr, int firstRegion)
{
  unsigned i;
  for (i = firstRegion; i < g_memoryRegions.count; ++i)
  {
    const size_t size = g_memoryRegions.size[i];
    if (addr < size)
    {
      if (g_memoryRegions.data[i] == NULL)
        break;

      return g_memoryRegions.data[i][addr];
    }

    addr -= size;
  }

  return 0;
}

static unsigned memoryReadBlock(unsigned addr, uint8_t* buffer, unsigned count, int firstRegion)
{
  unsigned provided = 0;
  unsigned i;
  for (i = firstRegion; i < g_memoryRegions.count; ++i)
  {
    const size_t size = g_memoryRegions.size[i];
    if (addr < size)
    {
      if (g_memoryRegions.data[i] == NULL)
        break;

      const size_t avail = std::min(size - addr, (size_t)count);
      memcpy(buffer, &g_memoryRegions.data[i][addr], avail);

      provided += avail;
      count -= avail;
      if (count == 0)
        break;

      buffer += avail;
      addr += avail;
    }

    addr -= size;
  }

  return provided;
}

static void memoryWrite(unsigned addr, int firstRegion, unsigned char value)
{
  unsigned i;
  for (i = firstRegion; i < g_memoryRegions.count; ++i)
  {
    const size_t size = g_memoryRegions.size[i];
    if (addr < size)
    {
      if (g_memoryRegions.data[i])
        g_memoryRegions.data[i][addr] = value;

      break;
    }

    addr -= size;
  }
}

static unsigned char memoryRead0(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[0]); }
static unsigned char memoryRead1(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[1]); }
static unsigned char memoryRead2(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[2]); }
static unsigned char memoryRead3(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[3]); }
static unsigned char memoryRead4(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[4]); }
static unsigned char memoryRead5(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[5]); }
static unsigned char memoryRead6(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[6]); }
static unsigned char memoryRead7(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[7]); }
static unsigned char memoryRead8(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[8]); }
static unsigned char memoryRead9(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[9]); }
static unsigned char memoryRead10(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[10]); }
static unsigned char memoryRead11(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[11]); }
static unsigned char memoryRead12(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[12]); }
static unsigned char memoryRead13(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[13]); }
static unsigned char memoryRead14(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[14]); }
static unsigned char memoryRead15(unsigned addr) { return memoryRead(addr, g_memoryBankFirstRegion[15]); }

static unsigned memoryReadBlock0(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[0]); }
static unsigned memoryReadBlock1(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[1]); }
static unsigned memoryReadBlock2(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[2]); }
static unsigned memoryReadBlock3(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[3]); }
static unsigned memoryReadBlock4(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[4]); }
static unsigned memoryReadBlock5(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[5]); }
static unsigned memoryReadBlock6(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[6]); }
static unsigned memoryReadBlock7(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[7]); }
static unsigned memoryReadBlock8(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[8]); }
static unsigned memoryReadBlock9(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[9]); }
static unsigned memoryReadBlock10(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[10]); }
static unsigned memoryReadBlock11(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[11]); }
static unsigned memoryReadBlock12(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[12]); }
static unsigned memoryReadBlock13(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[13]); }
static unsigned memoryReadBlock14(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[14]); }
static unsigned memoryReadBlock15(unsigned addr, uint8_t* buffer, unsigned count) { return memoryReadBlock(addr, buffer, count, g_memoryBankFirstRegion[15]); }

static void memoryWrite0(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[0], value); }
static void memoryWrite1(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[1], value); }
static void memoryWrite2(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[2], value); }
static void memoryWrite3(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[3], value); }
static void memoryWrite4(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[4], value); }
static void memoryWrite5(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[5], value); }
static void memoryWrite6(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[6], value); }
static void memoryWrite7(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[7], value); }
static void memoryWrite8(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[8], value); }
static void memoryWrite9(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[9], value); }
static void memoryWrite10(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[10], value); }
static void memoryWrite11(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[11], value); }
static void memoryWrite12(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[12], value); }
static void memoryWrite13(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[13], value); }
static void memoryWrite14(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[14], value); }
static void memoryWrite15(unsigned addr, unsigned char value) { memoryWrite(addr, g_memoryBankFirstRegion[15], value); }

static void installMemoryBank(int bankId, int validBankId, int firstRegion, size_t bankSize, libretro::LoggerComponent* logger)
{
  switch (validBankId)
  {
    case 0:
      RA_InstallMemoryBank(bankId, memoryRead0, memoryWrite0, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock0);
      break;
    case 1:
      RA_InstallMemoryBank(bankId, memoryRead1, memoryWrite1, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock1);
      break;
    case 2:
      RA_InstallMemoryBank(bankId, memoryRead2, memoryWrite2, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock2);
      break;
    case 3:
      RA_InstallMemoryBank(bankId, memoryRead3, memoryWrite3, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock3);
      break;
    case 4:
      RA_InstallMemoryBank(bankId, memoryRead4, memoryWrite4, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock4);
      break;
    case 5:
      RA_InstallMemoryBank(bankId, memoryRead5, memoryWrite5, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock5);
      break;
    case 6:
      RA_InstallMemoryBank(bankId, memoryRead6, memoryWrite6, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock6);
      break;
    case 7:
      RA_InstallMemoryBank(bankId, memoryRead7, memoryWrite7, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock7);
      break;
    case 8:
      RA_InstallMemoryBank(bankId, memoryRead8, memoryWrite8, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock8);
      break;
    case 9:
      RA_InstallMemoryBank(bankId, memoryRead9, memoryWrite9, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock9);
      break;
    case 10:
      RA_InstallMemoryBank(bankId, memoryRead10, memoryWrite10, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock10);
      break;
    case 11:
      RA_InstallMemoryBank(bankId, memoryRead11, memoryWrite11, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock11);
      break;
    case 12:
      RA_InstallMemoryBank(bankId, memoryRead12, memoryWrite12, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock12);
      break;
    case 13:
      RA_InstallMemoryBank(bankId, memoryRead13, memoryWrite13, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock13);
      break;
    case 14:
      RA_InstallMemoryBank(bankId, memoryRead14, memoryWrite14, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock14);
      break;
    case 15:
      RA_InstallMemoryBank(bankId, memoryRead15, memoryWrite15, bankSize);
      RA_InstallMemoryBankBlockReader(bankId, memoryReadBlock15);
      break;
    default: logger->warn(TAG "Too many unsupported memory regions"); return;
  }

  g_memoryBankFirstRegion[validBankId] = firstRegion;
}

static uint32_t g_refreshAttempts = 0;
static clock_t g_lastMemoryRefresh = 0;
static unsigned char deferredMemoryRead(unsigned addr)
{
  /* after 4500ms, give up. */
  if (g_refreshAttempts >= 100)
    return '\0';

  const clock_t now = clock();
  const clock_t elapsed = now - g_lastMemoryRefresh;

  /* for the first 100ms, check every 10ms.
   * after 100ms, only check every 20ms.
   * after 300ms, only check every 30ms.
   * after 600ms, only check every 40ms.
   * after 1000ms, only check every 50ms.
   * after 1500ms, only check every 60ms.
   * after 2100ms, only check every 70ms.
   * after 2800ms, only check every 80ms.
   * after 3600ms, only check every 90ms.
   */
  const uint32_t delay_factor = (g_refreshAttempts / 10) + 1;
  if (elapsed < (CLOCKS_PER_SEC / 100) * delay_factor)
    return '\0';

  g_refreshAttempts++;
  g_lastMemoryRefresh = now;

  extern Application app;
  app.refreshMemoryMap();
  return memoryRead0(addr);
}

bool Memory::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  return true;
}

void Memory::destroy()
{
  rc_libretro_memory_destroy(&g_memoryRegions);
  g_lastMemoryRefresh = 0;
  g_refreshAttempts = 0;
  RA_ClearMemoryBanks();
}

static libretro::Core* s_coreBeingInitialized = nullptr;
static libretro::LoggerComponent* s_logger = nullptr;

void* retro_get_memory_data(unsigned int id)
{
  libretro::Core* core = s_coreBeingInitialized;
  if (core)
    return core->getMemoryData(id);

  return NULL;
}

void retro_get_core_memory(unsigned id, rc_libretro_core_memory_info_t* info)
{
  libretro::Core* core = s_coreBeingInitialized;
  if (core)
  {
    info->data = (unsigned char*)core->getMemoryData(id);
    info->size = core->getMemorySize(id);
  }
  else
  {
    info->data = NULL;
    info->size = 0;
  }
}

static void rc_log_callback(const char* message)
{
  if (s_logger)
    s_logger->info(TAG "%s", message);
}

static void dumpDescriptors(const retro_memory_map* mmap, libretro::LoggerComponent* logger)
{
  const retro_memory_descriptor* desc = mmap->descriptors;
  const retro_memory_descriptor* end = desc + mmap->num_descriptors;
  int i = 0;

  for (; desc < end; desc++, i++)
  {
    /* log these at info level show they show up in the about dialog, but the function is only
     * called if debug level is enabled. */
    logger->info(TAG "desc[%d]: $%06x (%04x): %s%s", i + 1, desc->start, desc->len,
      desc->addrspace ? desc->addrspace : "", desc->ptr ? "" : "(null)");
  }
}

void Memory::attachToCore(libretro::Core* core, int consoleId)
{
  s_logger = _logger;
  s_coreBeingInitialized = core;
  rc_libretro_init_verbose_message_callback(rc_log_callback);

  const retro_memory_map* mmap = core->getMemoryMap();
  if (mmap && _logger->logLevel(RETRO_LOG_DEBUG))
    dumpDescriptors(mmap, _logger);

  /* capture the registered regions */
  rc_libretro_memory_regions_t memoryRegions;
  bool hasValidRegion = rc_libretro_memory_init(&memoryRegions, mmap, retro_get_core_memory, consoleId);

  s_logger = NULL;
  s_coreBeingInitialized = NULL;

  /* if no change is detected, do nothing */
  if (g_memoryRegions.total_size == memoryRegions.total_size && g_memoryRegions.count == memoryRegions.count)
  {
    if (memcmp(memoryRegions.data, g_memoryRegions.data, memoryRegions.count * sizeof(memoryRegions.data[0])) == 0 &&
        memcmp(memoryRegions.size, g_memoryRegions.size, memoryRegions.count * sizeof(memoryRegions.size[0])) == 0)
    {
      _logger->info(TAG "no change detected. using existing memory map");
      return;
    }

    _logger->info(TAG "change detected. updating memory map");
    for (unsigned i = 0; i < memoryRegions.count; i++)
    {
      if (memoryRegions.data[i] != g_memoryRegions.data[i])
        _logger->info(TAG "memoryRegion[%d] changed from %p to %p", i, g_memoryRegions.data[i], memoryRegions.data[i]);
    }
  }

  /* update the global map */
  memcpy(&g_memoryRegions, &memoryRegions, sizeof(memoryRegions));

  if (hasValidRegion)
  {
    installMemoryBanks();
  }
  else if (g_lastMemoryRefresh == 0)
  {
    _logger->info(TAG "delaying memory bank installation");
    g_lastMemoryRefresh = clock();
    g_refreshAttempts = 0;
    RA_ClearMemoryBanks();
    RA_InstallMemoryBank(0, deferredMemoryRead, memoryWrite0, g_memoryRegions.total_size);
  }
}

static bool g_bVersionChecked = false;
static bool g_bVersionSupported = false;

void Memory::installMemoryBanks()
{
  _logger->info(TAG "installing memory banks");
  RA_ClearMemoryBanks();

  // 0.78 DLL will crash if passed NULL as a read function - detect 0.79 DLL by looking for the _RA_SuspendRepaint export
  if (!g_bVersionChecked)
  {
    wchar_t sBuffer[MAX_PATH];
    DWORD iIndex = GetModuleFileNameW(0, sBuffer, MAX_PATH);
    while (iIndex > 0 && sBuffer[iIndex - 1] != '\\' && sBuffer[iIndex - 1] != '/')
      --iIndex;

    wcscpy_s(&sBuffer[iIndex], sizeof(sBuffer) / sizeof(sBuffer[0]) - iIndex, L"RA_Integration.dll");
    HINSTANCE hRADLL = LoadLibraryW(sBuffer);
    if (hRADLL)
    {
      g_bVersionSupported = GetProcAddress(hRADLL, "_RA_SuspendRepaint") != NULL;
      FreeLibrary(hRADLL);
    }

    g_bVersionChecked = true;
  }

  if (!g_bVersionSupported)
  {
    // have an 0.78 DLL - register a read function that will return 0 for the unsupported regions
    RA_InstallMemoryBank(0, memoryRead0, memoryWrite0, g_memoryRegions.total_size);
    return;
  }

  // have an 0.79 DLL - register invalid banks for unsupported regions
  int bankId = 0;
  int validBankId = 0;
  int firstRegion = 0;
  size_t bankSize = 0;
  bool wasValidRegion = false;
  for (size_t i = 0; i < g_memoryRegions.count; i++)
  {
    bool isValidRegion = (g_memoryRegions.data[i] != NULL);
    if (bankSize > 0 && isValidRegion != wasValidRegion)
    {
      if (wasValidRegion)
        installMemoryBank(bankId, validBankId++, firstRegion, bankSize, _logger);
      else
        RA_InstallMemoryBank(bankId, NULL, NULL, bankSize);

      firstRegion = i;
      bankSize = g_memoryRegions.size[i];
      ++bankId;
    }
    else
    {
      bankSize += g_memoryRegions.size[i];
    }

    wasValidRegion = isValidRegion;
  }

  if (bankSize > 0)
  {
    if (wasValidRegion)
      installMemoryBank(bankId, validBankId, firstRegion, bankSize, _logger);
    else
      RA_InstallMemoryBank(bankId, NULL, NULL, bankSize);
  }
}
