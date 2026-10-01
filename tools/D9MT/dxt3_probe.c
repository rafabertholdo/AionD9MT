/* Pixel regression for D9MT DXT3 (BC2) sampling: explicit 4-bit alpha and
 * 565 color, at the 64x64 one-mip and 512x512 full-chain shapes Aion uses.
 * Build: i686-w64-mingw32-gcc -Wall -Wextra -Werror -mwindows
 *   dxt3_probe.c -ld3d9 -ld3dcompiler -o dxt3.exe
 * Run with D9MT_ASYNC=0; writes dxt3-result.txt next to the executable.
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

static int isClose(int value, int expected) {
    return value >= expected - 3 && value <= expected + 3;
}

int main(void) {
    const UINT extent = 64;
    const char *vertexSource =
        "struct Out { float4 pos : POSITION; float2 uv : TEXCOORD0; };"
        "Out main(float3 pos : POSITION, float2 uv : TEXCOORD0) {"
        "Out result; result.pos=float4(pos,1); result.uv=uv; return result; }";
    /* c0.x selects the channel under test: 0 = color, 1 = alpha as gray */
    const char *pixelSource =
        "float4 mode : register(c0); sampler2D image : register(s0);"
        "float4 main(float2 uv : TEXCOORD0) : COLOR {"
        "float4 value=tex2D(image,uv);"
        "return float4(lerp(value.rgb, value.aaa, mode.x), 1); }";
    output = fopen("dxt3-result.txt", "w");
    if (!output) {
        return 1;
    }
    HWND window = CreateWindowA("STATIC", "d9mt dxt3", WS_OVERLAPPEDWINDOW,
                                100, 100, 160, 160, NULL, NULL,
                                GetModuleHandleA(NULL), NULL);
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
                     "main", "vs_2_0", 0, 0, &vertexCode, NULL));
    CHECK(D3DCompile(pixelSource, strlen(pixelSource), NULL, NULL, NULL, "main",
                     "ps_2_0", 0, 0, &pixelCode, NULL));
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
                                           D3DTEXF_LINEAR));
    CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAGFILTER,
                                           D3DTEXF_LINEAR));
    CHECK(IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MIPFILTER,
                                           D3DTEXF_LINEAR));
    IDirect3DSurface9 *readback = NULL, *backbuffer = NULL;
    CHECK(IDirect3DDevice9_CreateOffscreenPlainSurface(
        device, extent, extent, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &readback,
        NULL));
    CHECK(IDirect3DDevice9_GetBackBuffer(device, 0, 0, D3DBACKBUFFER_TYPE_MONO,
                                         &backbuffer));
    const float vertices[6][5] = {{-1, 1, 0.5f, 0, 0}, {1, 1, 0.5f, 1, 0},
                                  {1, -1, 0.5f, 1, 1}, {-1, 1, 0.5f, 0, 0},
                                  {1, -1, 0.5f, 1, 1}, {-1, -1, 0.5f, 0, 1}};
    const UINT sizes[2] = {64, 512};
    const UINT levels[2] = {1, 0};
    int passed = 0, total = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        const UINT size = sizes[index / 4];
        const DWORD nibble = 2u + 4u * (index % 4); /* alpha 2,6,10,14 */
        const WORD red = (WORD)(4u + 8u * (index % 4)), green = 40, blue = 12;
        const WORD color = (WORD)((red << 11) | (green << 5) | blue);
        IDirect3DTexture9 *texture = NULL;
        CHECK(IDirect3DDevice9_CreateTexture(device, size, size,
                                             levels[index / 4], 0, D3DFMT_DXT3,
                                             D3DPOOL_MANAGED, &texture, NULL));
        const DWORD mipCount = IDirect3DTexture9_GetLevelCount(texture);
        for (DWORD mip = 0; mip < mipCount; ++mip) {
            D3DLOCKED_RECT locked;
            CHECK(IDirect3DTexture9_LockRect(texture, mip, &locked, NULL, 0));
            const UINT blocks = ((size >> mip) + 3) / 4;
            const DWORD alphaWord = nibble * 0x11111111u;
            for (UINT rowIndex = 0; rowIndex < blocks; ++rowIndex) {
                DWORD *row =
                    (DWORD *)((BYTE *)locked.pBits + rowIndex * locked.Pitch);
                for (UINT column = 0; column < blocks; ++column) {
                    row[column * 4 + 0] = alphaWord;
                    row[column * 4 + 1] = alphaWord;
                    row[column * 4 + 2] = color | ((DWORD)color << 16);
                    row[column * 4 + 3] = 0; /* every texel uses color0 */
                }
            }
            CHECK(IDirect3DTexture9_UnlockRect(texture, mip));
        }
        CHECK(IDirect3DDevice9_SetTexture(device, 0,
                                          (IDirect3DBaseTexture9 *)texture));
        for (unsigned int mode = 0; mode < 2; ++mode) {
            const float constant[4] = {(float)mode, 0, 0, 0};
            CHECK(IDirect3DDevice9_SetPixelShaderConstantF(device, 0, constant,
                                                           1));
            CHECK(IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET,
                                         D3DCOLOR_XRGB(16, 16, 16), 1, 0));
            CHECK(IDirect3DDevice9_BeginScene(device));
            CHECK(IDirect3DDevice9_DrawPrimitiveUP(
                device, D3DPT_TRIANGLELIST, 2, vertices, sizeof(vertices[0])));
            CHECK(IDirect3DDevice9_EndScene(device));
            CHECK(IDirect3DDevice9_GetRenderTargetData(device, backbuffer,
                                                       readback));
            D3DLOCKED_RECT locked;
            CHECK(IDirect3DSurface9_LockRect(readback, &locked, NULL,
                                             D3DLOCK_READONLY));
            const DWORD pixel =
                *(const DWORD *)((BYTE *)locked.pBits +
                                 (extent / 2) * locked.Pitch + (extent / 2) * 4);
            CHECK(IDirect3DSurface9_UnlockRect(readback));
            const int r = (int)((pixel >> 16u) & 255u),
                      g = (int)((pixel >> 8u) & 255u), b = (int)(pixel & 255u);
            const int alpha = (int)(nibble * 17u);
            const int er = mode ? alpha : (int)((red << 3) | (red >> 2));
            const int eg = mode ? alpha : (int)((green << 2) | (green >> 4));
            const int eb = mode ? alpha : (int)((blue << 3) | (blue >> 2));
            const int matches = isClose(r, er) && isClose(g, eg) && isClose(b, eb);
            passed += matches;
            ++total;
            fprintf_s(output, "%ux%u %s %u: %d,%d,%d expected %d,%d,%d: %s\n",
                      size, size, mode ? "alpha" : "color", index, r, g, b, er,
                      eg, eb, matches ? "PASS" : "FAIL");
            CHECK(IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL));
        }
        IDirect3DTexture9_Release(texture);
    }
    fprintf_s(output, "%d passed, %d failed\n", passed, total - passed);
    IDirect3DSurface9_Release(backbuffer);
    IDirect3DSurface9_Release(readback);
    IDirect3DVertexShader9_Release(vertexShader);
    IDirect3DPixelShader9_Release(pixelShader);
    ID3D10Blob_Release(vertexCode);
    ID3D10Blob_Release(pixelCode);
    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(api);
    fclose(output);
    return passed == total ? 0 : 2;
}
