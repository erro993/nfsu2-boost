// NFSU2 Graphics Boost - ASI plugin (x86 / 32-bit)
// Zorlar: Anisotropic Filtering, Trilinear, Mipmap LOD bias, MSAA, VSync
// D3D9 vtable yamasi kullanir; oyun bellek adresi gerektirmez.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <wchar.h>

static HMODULE g_self;
static wchar_t g_dir[MAX_PATH];

static struct {
    int   aniso;      // 0/1 = kapali, 2..16
    int   trilinear;  // 1: POINT mip filtresini LINEAR yap
    float lodBias;    // 0 = dokunma, negatif = daha keskin
    int   msaa;       // 0 = kapali, 2/4/8
    int   vsync;      // -1 = oyuna birak, 0 = kapali, 1 = acik
    int   log;
} cfg;

static DWORD g_maxAniso = 16;

static void Log(const char* fmt, ...)
{
    if (!cfg.log) return;
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%lsNFSU2_GraphicsBoost.log", g_dir);
    FILE* f = _wfopen(path, L"a");
    if (!f) return;
    va_list va; va_start(va, fmt);
    vfprintf(f, fmt, va);
    va_end(va);
    fputc('\n', f);
    fclose(f);
}

static void LoadConfig()
{
    GetModuleFileNameW(g_self, g_dir, MAX_PATH);
    wchar_t* slash = wcsrchr(g_dir, L'\\');
    if (slash) *(slash + 1) = 0;

    wchar_t ini[MAX_PATH];
    swprintf(ini, MAX_PATH, L"%lsNFSU2_GraphicsBoost.ini", g_dir);

    cfg.aniso     = GetPrivateProfileIntW(L"Main", L"Anisotropy", 16, ini);
    cfg.trilinear = GetPrivateProfileIntW(L"Main", L"ForceTrilinear", 1, ini);
    cfg.msaa      = GetPrivateProfileIntW(L"Main", L"MSAA", 0, ini);
    cfg.vsync     = GetPrivateProfileIntW(L"Main", L"VSync", -1, ini);
    cfg.log       = GetPrivateProfileIntW(L"Main", L"Log", 1, ini);

    wchar_t buf[32] = L"0";
    GetPrivateProfileStringW(L"Main", L"MipLodBias", L"0", buf, 32, ini);
    cfg.lodBias = (float)_wtof(buf);

    if (cfg.aniso > 16) cfg.aniso = 16;
    if (cfg.aniso < 0)  cfg.aniso = 0;
}

// ---------------- vtable yardimcisi ----------------
static bool PatchVTable(void** vtbl, int idx, void* hook, void** orig)
{
    if (vtbl[idx] == hook) return true;           // zaten yamali
    DWORD old;
    if (!VirtualProtect(&vtbl[idx], sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = vtbl[idx];
    vtbl[idx] = hook;
    VirtualProtect(&vtbl[idx], sizeof(void*), old, &old);
    return true;
}

typedef HRESULT (WINAPI *CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                         D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT (WINAPI *Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (WINAPI *SetSamplerState_t)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);

static CreateDevice_t    oCreateDevice;
static Reset_t           oReset;
static SetSamplerState_t oSetSamplerState;

enum { VT_D3D_CreateDevice = 16, VT_DEV_Reset = 16, VT_DEV_SetSamplerState = 69 };

// ---------------- hook: SetSamplerState ----------------
static HRESULT WINAPI hkSetSamplerState(IDirect3DDevice9* dev, DWORD sampler,
                                        D3DSAMPLERSTATETYPE type, DWORD value)
{
    if (sampler < 16) // sadece pixel shader sampler'lari
    {
        if (cfg.aniso > 1 && type == D3DSAMP_MINFILTER && value == D3DTEXF_LINEAR)
        {
            DWORD a = (DWORD)cfg.aniso;
            if (a > g_maxAniso) a = g_maxAniso;
            oSetSamplerState(dev, sampler, D3DSAMP_MAXANISOTROPY, a);
            value = D3DTEXF_ANISOTROPIC;
        }
        else if (cfg.trilinear && type == D3DSAMP_MIPFILTER && value == D3DTEXF_POINT)
        {
            value = D3DTEXF_LINEAR;
        }

        if (cfg.lodBias != 0.0f && type == D3DSAMP_MINFILTER)
        {
            union { float f; DWORD d; } u; u.f = cfg.lodBias;
            oSetSamplerState(dev, sampler, D3DSAMP_MIPMAPLODBIAS, u.d);
        }
    }
    return oSetSamplerState(dev, sampler, type, value);
}

// ---------------- present parametreleri (MSAA / VSync) ----------------
static void ApplyParams(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE devType, D3DPRESENT_PARAMETERS* pp)
{
    if (!pp) return;

    if (cfg.vsync == 0)      pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    else if (cfg.vsync == 1) pp->PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (cfg.msaa >= 2 && d3d)
    {
        D3DMULTISAMPLE_TYPE ms = (D3DMULTISAMPLE_TYPE)cfg.msaa;
        D3DFORMAT bb = (pp->BackBufferFormat == D3DFMT_UNKNOWN) ? D3DFMT_X8R8G8B8 : pp->BackBufferFormat;
        DWORD q = 0;
        bool ok = SUCCEEDED(d3d->CheckDeviceMultiSampleType(adapter, devType, bb, pp->Windowed, ms, &q));
        if (ok && pp->EnableAutoDepthStencil)
            ok = SUCCEEDED(d3d->CheckDeviceMultiSampleType(adapter, devType,
                            pp->AutoDepthStencilFormat, pp->Windowed, ms, &q));
        if (ok)
        {
            pp->MultiSampleType    = ms;
            pp->MultiSampleQuality = 0;
            pp->SwapEffect         = D3DSWAPEFFECT_DISCARD;
            pp->Flags             &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
            Log("MSAA %dx uygulandi", cfg.msaa);
        }
        else Log("MSAA %dx desteklenmiyor, atlandi", cfg.msaa);
    }
}

// ---------------- hook: Reset ----------------
static HRESULT WINAPI hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    D3DPRESENT_PARAMETERS backup = {};
    bool have = pp != nullptr;
    if (have) backup = *pp;

    IDirect3D9* d3d = nullptr;
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    if (have && SUCCEEDED(dev->GetDirect3D(&d3d)) && SUCCEEDED(dev->GetCreationParameters(&cp)))
        ApplyParams(d3d, cp.AdapterOrdinal, cp.DeviceType, pp);

    HRESULT hr = oReset(dev, pp);
    if (FAILED(hr) && have)
    {
        Log("Reset basarisiz (0x%08lX), orijinal parametrelerle tekrar", (unsigned long)hr);
        *pp = backup;
        hr = oReset(dev, pp);
    }
    if (d3d) d3d->Release();
    return hr;
}

static void PatchDevice(IDirect3DDevice9* dev)
{
    void** vt = *(void***)dev;
    PatchVTable(vt, VT_DEV_SetSamplerState, (void*)hkSetSamplerState, (void**)&oSetSamplerState);
    PatchVTable(vt, VT_DEV_Reset,           (void*)hkReset,           (void**)&oReset);

    D3DCAPS9 caps;
    if (SUCCEEDED(dev->GetDeviceCaps(&caps)) && caps.MaxAnisotropy > 0)
        g_maxAniso = caps.MaxAnisotropy;
    Log("Device yamalandi (MaxAnisotropy=%lu)", (unsigned long)g_maxAniso);
}

// ---------------- hook: CreateDevice ----------------
static HRESULT WINAPI hkCreateDevice(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, HWND wnd,
                                     DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out)
{
    D3DPRESENT_PARAMETERS backup = {};
    bool have = pp != nullptr;
    if (have) backup = *pp;

    ApplyParams(d3d, adapter, type, pp);
    HRESULT hr = oCreateDevice(d3d, adapter, type, wnd, flags, pp, out);

    if (FAILED(hr) && have)
    {
        Log("CreateDevice basarisiz (0x%08lX), orijinal parametrelerle tekrar", (unsigned long)hr);
        *pp = backup;
        hr = oCreateDevice(d3d, adapter, type, wnd, flags, pp, out);
    }
    if (SUCCEEDED(hr) && out && *out)
        PatchDevice(*out);
    return hr;
}

// ---------------- baslangic ----------------
static DWORD WINAPI InitThread(LPVOID)
{
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) d3d9 = LoadLibraryA("d3d9.dll");
    if (!d3d9) { Log("d3d9.dll bulunamadi"); return 0; }

    typedef IDirect3D9* (WINAPI *Create9_t)(UINT);
    Create9_t create = (Create9_t)GetProcAddress(d3d9, "Direct3DCreate9");
    if (!create) { Log("Direct3DCreate9 bulunamadi"); return 0; }

    IDirect3D9* d3d = create(D3D_SDK_VERSION);
    if (!d3d) { Log("Direct3DCreate9 basarisiz"); return 0; }

    void** vt = *(void***)d3d;
    bool ok = PatchVTable(vt, VT_D3D_CreateDevice, (void*)hkCreateDevice, (void**)&oCreateDevice);
    d3d->Release();
    Log(ok ? "CreateDevice hook kuruldu" : "CreateDevice hook KURULAMADI");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        LoadConfig();
        Log("---- NFSU2 Graphics Boost yuklendi ----");
        Log("Aniso=%d Trilinear=%d LodBias=%.2f MSAA=%d VSync=%d",
            cfg.aniso, cfg.trilinear, cfg.lodBias, cfg.msaa, cfg.vsync);
        HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
