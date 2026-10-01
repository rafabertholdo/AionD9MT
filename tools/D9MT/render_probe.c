#define COBJMACROS
#include <d3d9.h>
#include <stdio.h>
#include <windows.h>
int main(int argumentCount, char **arguments) {
    FILE *log = fopen(
        argumentCount > 1 ? arguments[1] : "render-probe-result.txt", "w");
    if (!log)
        return 1;
    HWND window = CreateWindowA("STATIC", "d9mt presentation test",
                                WS_OVERLAPPEDWINDOW, 100, 100, 320, 200, NULL,
                                NULL, GetModuleHandleA(NULL), NULL);
    ShowWindow(window, SW_SHOW);
    IDirect3D9 *api = Direct3DCreate9(D3D_SDK_VERSION);
    fprintf(log, "Direct3DCreate9: %s\n", api ? "OK" : "FAILED");
    fflush(log);
    if (!api)
        return 2;
    D3DPRESENT_PARAMETERS parameters = {0};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = 320;
    parameters.BackBufferHeight = 200;
    parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    IDirect3DDevice9 *device = NULL;
    HRESULT status = IDirect3D9_CreateDevice(
        api, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &parameters, &device);
    fprintf(log, "CreateDevice: 0x%08lx\n", (unsigned long)status);
    fflush(log);
    if (FAILED(status))
        return 3;
    IDirect3DTexture9 *normalTexture = NULL;
    status =
        IDirect3DDevice9_CreateTexture(device, 64, 64, 0, 0, D3DFMT_X8L8V8U8,
                                       D3DPOOL_MANAGED, &normalTexture, NULL);
    fprintf(log, "X8L8V8U8 texture: 0x%08lx\n", (unsigned long)status);
    fflush(log);
    if (FAILED(status))
        return 5;
    IDirect3DTexture9_Release(normalTexture);
    status = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET,
                                    D3DCOLOR_XRGB(30, 160, 60), 1.0f, 0);
    fprintf(log, "Clear: 0x%08lx\n", (unsigned long)status);
    fflush(log);
    status = IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);
    fprintf(log, "Present: 0x%08lx\n", (unsigned long)status);
    fflush(log);
    Sleep(2000);
    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(api);
    DestroyWindow(window);
    fclose(log);
    return FAILED(status) ? 4 : 0;
}
