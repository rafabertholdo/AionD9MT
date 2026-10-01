/* Pixel regression for D9MT's X8L8V8U8 compute conversion.
 * Build: i686-w64-mingw32-gcc -Wall -Wextra -Werror -mwindows
 *   texture_conversion_probe.c -ld3d9 -ld3dcompiler -o conversion.exe
 * Run with D9MT_ASYNC=0 and Metal API validation enabled.
 */
#define COBJMACROS
#include <d3d9.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static FILE *output;
#define CHECK(call)                                                            \
    do {                                                                       \
        HRESULT status = (call);                                               \
        if (FAILED(status)) {                                                  \
            fprintf_s(output, "%s failed: %08lx\n", #call,                     \
                      (unsigned long)status);                                  \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(void) {
    const UINT extent = 64;
    const char *vertexSource =
        "struct Out { float4 pos : POSITION; float2 uv : TEXCOORD0; };"
        "Out main(float3 pos : POSITION, float2 uv : TEXCOORD0) {"
        "Out result; result.pos=float4(pos,1); result.uv=uv; return result; }";
    const char *pixelSource = "sampler2D image : register(s0);"
                              "float4 main(float2 uv : TEXCOORD0) : COLOR {"
                              "float4 value=tex2D(image,uv);"
                              "return float4(value.xy*0.5+0.5,value.z,1); }";
    output = fopen("texture-conversion-result.txt", "w");
    if (!output) {
        return 1;
    }
    HWND window = CreateWindowA("STATIC", "d9mt texture conversion",
                                WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL,
                                NULL, GetModuleHandleA(NULL), NULL);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    IDirect3D9 *api = Direct3DCreate9(D3D_SDK_VERSION);
    if (!api) {
        return 1;
    }
    D3DPRESENT_PARAMETERS parameters = {.SwapEffect = D3DSWAPEFFECT_DISCARD};
    parameters.Windowed = TRUE;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = extent;
    parameters.BackBufferHeight = extent;
    parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    IDirect3DDevice9 *device = NULL;
    CHECK(IDirect3D9_CreateDevice(api, 0, D3DDEVTYPE_HAL, window,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                  &parameters, &device));
    ID3DBlob *vertexCode = NULL, *pixelCode = NULL;
    CHECK(D3DCompile(vertexSource, strlen(vertexSource), NULL, NULL, NULL,
                     "main", "vs_3_0", 0, 0, &vertexCode, NULL));
    CHECK(D3DCompile(pixelSource, strlen(pixelSource), NULL, NULL, NULL, "main",
                     "ps_3_0", 0, 0, &pixelCode, NULL));
    IDirect3DVertexShader9 *vertexShader = NULL;
    IDirect3DPixelShader9 *pixelShader = NULL;
    CHECK(IDirect3DDevice9_CreateVertexShader(
        device, ID3D10Blob_GetBufferPointer(vertexCode), &vertexShader));
    CHECK(IDirect3DDevice9_CreatePixelShader(
        device, ID3D10Blob_GetBufferPointer(pixelCode), &pixelShader));
    CHECK(IDirect3DDevice9_SetVertexShader(device, vertexShader));
    CHECK(IDirect3DDevice9_SetPixelShader(device, pixelShader));
    CHECK(IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ | D3DFVF_TEX1));
    CHECK(IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, FALSE));
    CHECK(
        IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE));
    CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MINFILTER,
                                           D3DTEXF_POINT));
    CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAGFILTER,
                                           D3DTEXF_POINT));
    IDirect3DSurface9 *readback = NULL, *backbuffer = NULL;
    CHECK(IDirect3DDevice9_CreateOffscreenPlainSurface(
        device, extent, extent, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &readback,
        NULL));
    CHECK(IDirect3DDevice9_GetBackBuffer(device, 0, 0, D3DBACKBUFFER_TYPE_MONO,
                                         &backbuffer));
    const float vertices[6][5] = {
        {-1, 1, 0.5f, 0.5f, 0.5f}, {1, 1, 0.5f, 0.5f, 0.5f},
        {1, -1, 0.5f, 0.5f, 0.5f}, {-1, 1, 0.5f, 0.5f, 0.5f},
        {1, -1, 0.5f, 0.5f, 0.5f}, {-1, -1, 0.5f, 0.5f, 0.5f}};
    int passed = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        IDirect3DTexture9 *texture = NULL;
        CHECK(IDirect3DDevice9_CreateTexture(device, extent, extent, 1, 0,
                                             D3DFMT_X8L8V8U8, D3DPOOL_MANAGED,
                                             &texture, NULL));
        D3DLOCKED_RECT locked;
        CHECK(IDirect3DTexture9_LockRect(texture, 0, &locked, NULL, 0));
        const DWORD luminance = 16 * index + 8;
        const DWORD packed = (luminance << 16u) | 0x0000c040u;
        for (UINT rowIndex = 0; rowIndex < extent; ++rowIndex) {
            DWORD *row =
                (DWORD *)((BYTE *)locked.pBits + rowIndex * locked.Pitch);
            for (UINT column = 0; column < extent; ++column) {
                row[column] = packed;
            }
        }
        CHECK(IDirect3DTexture9_UnlockRect(texture, 0));
        CHECK(IDirect3DDevice9_SetTexture(device, 0,
                                          (IDirect3DBaseTexture9 *)texture));
        CHECK(IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET,
                                     D3DCOLOR_XRGB(16, 16, 16), 1, 0));
        CHECK(IDirect3DDevice9_BeginScene(device));
        CHECK(IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLELIST, 2,
                                               vertices, sizeof(vertices[0])));
        CHECK(IDirect3DDevice9_EndScene(device));
        CHECK(
            IDirect3DDevice9_GetRenderTargetData(device, backbuffer, readback));
        CHECK(IDirect3DSurface9_LockRect(readback, &locked, NULL,
                                         D3DLOCK_READONLY));
        const DWORD pixel =
            *(const DWORD *)((BYTE *)locked.pBits +
                             (extent / 2) * locked.Pitch + (extent / 2) * 4);
        const int red = (int)((pixel >> 16u) & 255u),
                  green = (int)((pixel >> 8u) & 255u);
        const int blue = (int)(pixel & 255u);
        const int matches = red >= 189 && red <= 195 && green >= 60 &&
                            green <= 66 && blue == (int)luminance;
        passed += matches;
        fprintf_s(output, "case %u: %d,%d,%d expected 192,63,%lu: %s\n", index,
                  red, green, blue, (unsigned long)luminance,
                  matches ? "PASS" : "FAIL");
        CHECK(IDirect3DSurface9_UnlockRect(readback));
        CHECK(IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL));
        IDirect3DTexture9_Release(texture);
    }
    fprintf_s(output, "%d passed, %d failed\n", passed, 8 - passed);
    IDirect3DSurface9_Release(backbuffer);
    IDirect3DSurface9_Release(readback);
    IDirect3DVertexShader9_Release(vertexShader);
    IDirect3DPixelShader9_Release(pixelShader);
    ID3D10Blob_Release(vertexCode);
    ID3D10Blob_Release(pixelCode);
    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(api);
    DestroyWindow(window);
    fclose(output);
    return passed == 8 ? 0 : 1;
}
