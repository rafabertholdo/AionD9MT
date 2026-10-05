#include "data_buffer_protection.h"
#include <assert.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdio.h>

int main(void) {
  size_t size = (size_t)sysconf(_SC_PAGESIZE);
  void *memory =
      mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert(memory != MAP_FAILED);
  assert(d9mtProtectDataBuffer((uintptr_t)memory, size) == 0);
  mach_vm_address_t address = (uintptr_t)memory;
  mach_vm_size_t regionSize = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object = MACH_PORT_NULL;
  assert(mach_vm_region(mach_task_self(), &address, &regionSize,
                        VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info,
                        &count, &object) == KERN_SUCCESS);
  assert(!((unsigned int)info.protection & (unsigned int)VM_PROT_EXECUTE));
  if (object != MACH_PORT_NULL)
    mach_port_deallocate(mach_task_self(), object);
  puts("PASS executable permission removed from the data allocation");
  ((volatile unsigned char *)memory)[size - 1] = 0x5a;
  assert(((volatile unsigned char *)memory)[size - 1] == 0x5a);
  puts("PASS renderer data remains readable and writable");
  assert(d9mtProtectDataBuffer(0, size) != 0);
  puts("PASS null address rejected");
  assert(d9mtProtectDataBuffer((uintptr_t)memory + 1, size) != 0);
  puts("PASS unaligned address rejected");
  assert(d9mtProtectDataBuffer((uintptr_t)memory, size - 1) != 0);
  puts("PASS partial page rejected");
  assert(d9mtProtectDataBuffer((uintptr_t)memory, 0) != 0);
  puts("PASS empty allocation rejected");
  assert(d9mtProtectDataBuffer(UINT64_MAX - size + 1, size) != 0);
  puts("PASS overflowing allocation rejected");
  assert(munmap(memory, size) == 0);
  puts("7 passed, 0 failed");
  return 0;
}
