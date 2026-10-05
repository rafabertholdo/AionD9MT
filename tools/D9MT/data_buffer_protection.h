#ifndef D9MT_DATA_BUFFER_PROTECTION_H
#define D9MT_DATA_BUFFER_PROTECTION_H

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/* Only dedicated renderer allocations may use this entry. Wine can add
 * execute permission to PAGE_READWRITE memory when the game's DEP is off.
 * Keep those data pages out of Rosetta's executable-memory write tracking. */
static int d9mtProtectDataBuffer(uint64_t address, uint64_t size) {
  long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0 || !address || !size || address > UINTPTR_MAX ||
      size > SIZE_MAX || size > UINTPTR_MAX - address ||
      address % (uint64_t)pageSize || size % (uint64_t)pageSize) {
    return (int)0xc000000du;
  }
  if (mprotect((void *)(uintptr_t)address, (size_t)size,
               PROT_READ | PROT_WRITE) != 0) {
    return (int)0xc0000001u;
  }
  return 0;
}

#endif
