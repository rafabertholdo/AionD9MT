/* Promote the isolated Wine driver so DXMT v0.80 can resolve its bridge.
 * Wine 11 loads Unix libraries with RTLD_LOCAL. SPDX-License-Identifier:
 * LGPL-2.1-or-later
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((constructor)) static void loadCompatibleMacDriver(void) {
    const char *path = getenv("D9MT_MAC_DRIVER_PATH");
    if (!path || !*path) {
        return;
    }
    /* Retain this handle for the process lifetime, as Wine owns the driver. */
    // The launcher supplies the isolated engine path; loading it is
    // intentional. NOLINTBEGIN(clang-analyzer-optin.taint.GenericTaint)
    void *driver = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    // NOLINTEND(clang-analyzer-optin.taint.GenericTaint)
    if (!driver) {
        fputs("d9mt: Mac-driver bridge load failed: ", stderr);
        const char *error = dlerror();
        fputs(error ? error : "unknown error", stderr);
        fputc('\n', stderr);
        return;
    }
    fputs(dlsym(RTLD_DEFAULT, "macdrv_functions")
              ? "d9mt: Mac-driver bridge export: available\n"
              : "d9mt: Mac-driver bridge export: missing\n",
          stderr);
}
