#include "dsound.h"

// --- 全局变量和函数指针 ---
#pragma region Globals and Function Pointers

// 定义所有要从原始 dsound.dll 中加载的函数指针类型
typedef HRESULT(WINAPI* tDirectSoundCreate)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(WINAPI* tDirectSoundCreate8)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);
typedef HRESULT(WINAPI* tDirectSoundEnumerateA)(LPDSENUMCALLBACKA, LPVOID);
typedef HRESULT(WINAPI* tDirectSoundEnumerateW)(LPDSENUMCALLBACKW, LPVOID);
typedef HRESULT(WINAPI* tDirectSoundCaptureCreate)(LPCGUID, LPDIRECTSOUNDCAPTURE*, LPUNKNOWN);
typedef HRESULT(WINAPI* tDirectSoundCaptureCreate8)(LPCGUID, LPDIRECTSOUNDCAPTURE8*, LPUNKNOWN);
typedef HRESULT(WINAPI* tDirectSoundCaptureEnumerateA)(LPDSENUMCALLBACKA, LPVOID);
typedef HRESULT(WINAPI* tDirectSoundCaptureEnumerateW)(LPDSENUMCALLBACKW, LPVOID);
typedef HRESULT(WINAPI* tGetDeviceID)(LPCGUID, LPGUID);
typedef HRESULT(WINAPI* tDllCanUnloadNow)(void);
typedef HRESULT(WINAPI* tDllGetClassObject)(REFCLSID, REFIID, LPVOID*);

// 声明函数指针变量
tDirectSoundCreate              pRealDirectSoundCreate = nullptr;
tDirectSoundCreate8             pRealDirectSoundCreate8 = nullptr;
tDirectSoundEnumerateA          pRealDirectSoundEnumerateA = nullptr;
tDirectSoundEnumerateW          pRealDirectSoundEnumerateW = nullptr;
tDirectSoundCaptureCreate       pRealDirectSoundCaptureCreate = nullptr;
tDirectSoundCaptureCreate8      pRealDirectSoundCaptureCreate8 = nullptr;
tDirectSoundCaptureEnumerateA   pRealDirectSoundCaptureEnumerateA = nullptr;
tDirectSoundCaptureEnumerateW   pRealDirectSoundCaptureEnumerateW = nullptr;
tGetDeviceID                    pRealGetDeviceID = nullptr;
tDllCanUnloadNow                pRealDllCanUnloadNow = nullptr;
tDllGetClassObject              pRealDllGetClassObject = nullptr;

// 模块和状态变量
HMODULE                         hRealDsound = nullptr;
UINT ProxyIDirectSoundBuffer::s_nextBufferId = 0;
bool                            g_enableLogging = false;

// 核心音量控制变量
LONG g_masterSfxVolume = DSBVOLUME_MAX;

// 全局缓冲区列表，用于广播音量变化
std::vector<ProxyIDirectSoundBuffer*> g_activeBuffers;
std::mutex g_bufferMutex;

#pragma endregion

// --- 日志函数 ---
void Log(const char* fmt, ...)
{
    if (!g_enableLogging) return;
    FILE* f = nullptr;
    fopen_s(&f, "dsound.log", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

// --- DLL 入口点 ---
#pragma region DllMain
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        g_enableLogging = GetPrivateProfileIntA("Debug", "Logging", 0, ".\\dsound.ini");
        if (g_enableLogging) DeleteFileA("dsound.log");

        char szPath[MAX_PATH];
        GetSystemDirectoryA(szPath, MAX_PATH);
        strcat_s(szPath, "\\dsound.dll");
        hRealDsound = LoadLibraryA(szPath);

        if (hRealDsound)
        {
            pRealDirectSoundCreate = (tDirectSoundCreate)GetProcAddress(hRealDsound, "DirectSoundCreate");
            pRealDirectSoundCreate8 = (tDirectSoundCreate8)GetProcAddress(hRealDsound, "DirectSoundCreate8");
            pRealDirectSoundEnumerateA = (tDirectSoundEnumerateA)GetProcAddress(hRealDsound, "DirectSoundEnumerateA");
            pRealDirectSoundEnumerateW = (tDirectSoundEnumerateW)GetProcAddress(hRealDsound, "DirectSoundEnumerateW");
            pRealDirectSoundCaptureCreate = (tDirectSoundCaptureCreate)GetProcAddress(hRealDsound, "DirectSoundCaptureCreate");
            pRealDirectSoundCaptureCreate8 = (tDirectSoundCaptureCreate8)GetProcAddress(hRealDsound, "DirectSoundCaptureCreate8");
            pRealDirectSoundCaptureEnumerateA = (tDirectSoundCaptureEnumerateA)GetProcAddress(hRealDsound, "DirectSoundCaptureEnumerateA");
            pRealDirectSoundCaptureEnumerateW = (tDirectSoundCaptureEnumerateW)GetProcAddress(hRealDsound, "DirectSoundCaptureEnumerateW");
            pRealGetDeviceID = (tGetDeviceID)GetProcAddress(hRealDsound, "GetDeviceID");
            pRealDllCanUnloadNow = (tDllCanUnloadNow)GetProcAddress(hRealDsound, "DllCanUnloadNow");
            pRealDllGetClassObject = (tDllGetClassObject)GetProcAddress(hRealDsound, "DllGetClassObject");
        }
        else {
            MessageBoxA(NULL, "无法加载系统原始的 dsound.dll！", "代理DLL错误", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (hRealDsound) { FreeLibrary(hRealDsound); hRealDsound = nullptr; }
    }
    return TRUE;
}
#pragma endregion

// --- 导出的代理函数 ---
#pragma region Exported Functions
extern "C" {
    HRESULT WINAPI DirectSoundCreate8(_In_opt_ LPCGUID pcGuidDevice, _Out_ LPDIRECTSOUND8* ppDS8, _In_opt_ LPUNKNOWN pUnkOuter) {
        Log("--> Export: DirectSoundCreate8 called.");
        if (!pRealDirectSoundCreate8) return DSERR_GENERIC;
        if (!ppDS8) return DSERR_INVALIDPARAM; *ppDS8 = nullptr;
        IDirectSound8* pRealDS8 = nullptr;
        HRESULT hr = pRealDirectSoundCreate8(pcGuidDevice, &pRealDS8, pUnkOuter);
        if (SUCCEEDED(hr) && pRealDS8) { *ppDS8 = new ProxyIDirectSound8(pRealDS8); }
        return hr;
    }
    HRESULT WINAPI DirectSoundCreate(_In_opt_ LPCGUID pcGuidDevice, _Out_ LPDIRECTSOUND* ppDS, _In_opt_ LPUNKNOWN pUnkOuter) {
        Log("--> Export: DirectSoundCreate (legacy) called.");
        if (!pRealDirectSoundCreate) return DSERR_GENERIC;
        if (!ppDS) return DSERR_INVALIDPARAM; *ppDS = nullptr;
        IDirectSound* pRealDS = nullptr;
        HRESULT hr = pRealDirectSoundCreate(pcGuidDevice, &pRealDS, pUnkOuter);
        if (SUCCEEDED(hr) && pRealDS) { *ppDS = new ProxyIDirectSound(pRealDS); }
        return hr;
    }
    HRESULT WINAPI DirectSoundEnumerateA(_In_ LPDSENUMCALLBACKA pDSEnumCallback, _In_opt_ LPVOID pContext) { if (!pRealDirectSoundEnumerateA) return DSERR_GENERIC; return pRealDirectSoundEnumerateA(pDSEnumCallback, pContext); }
    HRESULT WINAPI DirectSoundEnumerateW(_In_ LPDSENUMCALLBACKW pDSEnumCallback, _In_opt_ LPVOID pContext) { if (!pRealDirectSoundEnumerateW) return DSERR_GENERIC; return pRealDirectSoundEnumerateW(pDSEnumCallback, pContext); }
    HRESULT WINAPI DirectSoundCaptureCreate(_In_opt_ LPCGUID lpcGUID, _Out_ LPDIRECTSOUNDCAPTURE* lplpDSC, _In_opt_ LPUNKNOWN pUnkOuter) { if (!pRealDirectSoundCaptureCreate) return DSERR_GENERIC; return pRealDirectSoundCaptureCreate(lpcGUID, lplpDSC, pUnkOuter); }
    HRESULT WINAPI DirectSoundCaptureCreate8(_In_opt_ LPCGUID lpcGUID, _Out_ LPDIRECTSOUNDCAPTURE8* lplpDSC, _In_opt_ LPUNKNOWN pUnkOuter) { if (!pRealDirectSoundCaptureCreate8) return DSERR_GENERIC; return pRealDirectSoundCaptureCreate8(lpcGUID, lplpDSC, pUnkOuter); }
    HRESULT WINAPI DirectSoundCaptureEnumerateA(_In_ LPDSENUMCALLBACKA lpDSEnumCallback, _In_opt_ LPVOID pContext) { if (!pRealDirectSoundCaptureEnumerateA) return DSERR_GENERIC; return pRealDirectSoundCaptureEnumerateA(lpDSEnumCallback, pContext); }
    HRESULT WINAPI DirectSoundCaptureEnumerateW(_In_ LPDSENUMCALLBACKW lpDSEnumCallback, _In_opt_ LPVOID pContext) { if (!pRealDirectSoundCaptureEnumerateW) return DSERR_GENERIC; return pRealDirectSoundCaptureEnumerateW(lpDSEnumCallback, pContext); }
    HRESULT WINAPI GetDeviceID(_In_opt_ LPCGUID pGuidSrc, _Out_ LPGUID pGuidDest) { if (!pRealGetDeviceID) return DSERR_GENERIC; return pRealGetDeviceID(pGuidSrc, pGuidDest); }
    HRESULT WINAPI DllCanUnloadNow(void) { if (!pRealDllCanUnloadNow) return S_FALSE; return pRealDllCanUnloadNow(); }
    HRESULT WINAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Out_ LPVOID* ppv) { if (!pRealDllGetClassObject) return E_FAIL; return pRealDllGetClassObject(rclsid, riid, ppv); }
}
#pragma endregion

// --- ProxyIDirectSound & ProxyIDirectSound8 实现 ---
#pragma region ProxyIDirectSound Implementations
ProxyIDirectSound::ProxyIDirectSound(IDirectSound* pRealSound) : m_pRealDSound(pRealSound), m_ulRef(1) { Log("[ProxyDS:%p] Created.", this); }
ProxyIDirectSound::~ProxyIDirectSound() { Log("[ProxyDS:%p] Destroyed.", this); }
STDMETHODIMP ProxyIDirectSound::QueryInterface(REFIID riid, _Out_ LPVOID* ppvObj) { if (!ppvObj) return E_POINTER; *ppvObj = nullptr; if (riid == IID_IUnknown || riid == IID_IDirectSound) { *ppvObj = this; AddRef(); return S_OK; } if (riid == IID_IDirectSound8) { IDirectSound8* pRealDS8 = nullptr; HRESULT hr = m_pRealDSound->QueryInterface(riid, (void**)&pRealDS8); if (SUCCEEDED(hr)) { *ppvObj = new ProxyIDirectSound8(pRealDS8); } return hr; } return m_pRealDSound->QueryInterface(riid, ppvObj); }
STDMETHODIMP_(ULONG) ProxyIDirectSound::AddRef() { return ++m_ulRef; }
STDMETHODIMP_(ULONG) ProxyIDirectSound::Release() { m_ulRef--; if (m_ulRef == 0) { m_pRealDSound->Release(); delete this; return 0; } return m_ulRef; }
STDMETHODIMP ProxyIDirectSound::CreateSoundBuffer(_In_ LPCDSBUFFERDESC pcDSBufferDesc, _Out_ LPDIRECTSOUNDBUFFER* ppDSBuffer, _In_opt_ LPUNKNOWN pUnkOuter) { if (!ppDSBuffer || !pcDSBufferDesc) return DSERR_INVALIDPARAM; *ppDSBuffer = nullptr; bool isPrimary = (pcDSBufferDesc->dwFlags & DSBCAPS_PRIMARYBUFFER); IDirectSoundBuffer* pRealBuffer = nullptr; HRESULT hr = m_pRealDSound->CreateSoundBuffer(pcDSBufferDesc, &pRealBuffer, pUnkOuter); if (SUCCEEDED(hr) && pRealBuffer) { *ppDSBuffer = new ProxyIDirectSoundBuffer(pRealBuffer, isPrimary); } return hr; }
STDMETHODIMP ProxyIDirectSound::GetCaps(_Out_ LPDSCAPS pDSCaps) { return m_pRealDSound->GetCaps(pDSCaps); }
STDMETHODIMP ProxyIDirectSound::DuplicateSoundBuffer(_In_ LPDIRECTSOUNDBUFFER pDSBufferOriginal, _Out_ LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate) { if (!pDSBufferOriginal || !ppDSBufferDuplicate) return DSERR_INVALIDPARAM; *ppDSBufferDuplicate = nullptr; auto pProxyBufferOriginal = static_cast<ProxyIDirectSoundBuffer*>(pDSBufferOriginal); IDirectSoundBuffer* pRealBufferDuplicate = nullptr; HRESULT hr = m_pRealDSound->DuplicateSoundBuffer(pProxyBufferOriginal->GetReal(), &pRealBufferDuplicate); if (SUCCEEDED(hr) && pRealBufferDuplicate) { *ppDSBufferDuplicate = new ProxyIDirectSoundBuffer(pRealBufferDuplicate, false); } return hr; }
STDMETHODIMP ProxyIDirectSound::SetCooperativeLevel(HWND hwnd, DWORD dwLevel) { return m_pRealDSound->SetCooperativeLevel(hwnd, dwLevel); }
STDMETHODIMP ProxyIDirectSound::Compact() { return m_pRealDSound->Compact(); }
STDMETHODIMP ProxyIDirectSound::GetSpeakerConfig(_Out_ LPDWORD pdwSpeakerConfig) { return m_pRealDSound->GetSpeakerConfig(pdwSpeakerConfig); }
STDMETHODIMP ProxyIDirectSound::SetSpeakerConfig(DWORD dwSpeakerConfig) { return m_pRealDSound->SetSpeakerConfig(dwSpeakerConfig); }
STDMETHODIMP ProxyIDirectSound::Initialize(_In_opt_ LPCGUID pcGuidDevice) { return m_pRealDSound->Initialize(pcGuidDevice); }

ProxyIDirectSound8::ProxyIDirectSound8(IDirectSound8* pRealSound) : m_pRealDSound(pRealSound), m_ulRef(1) { Log("[ProxyDS8:%p] Created.", this); }
ProxyIDirectSound8::~ProxyIDirectSound8() { Log("[ProxyDS8:%p] Destroyed.", this); }
STDMETHODIMP ProxyIDirectSound8::QueryInterface(REFIID riid, _Out_ LPVOID* ppvObj) { if (!ppvObj) return E_POINTER; *ppvObj = nullptr; if (riid == IID_IUnknown || riid == IID_IDirectSound || riid == IID_IDirectSound8) { *ppvObj = this; AddRef(); return S_OK; } return m_pRealDSound->QueryInterface(riid, ppvObj); }
STDMETHODIMP_(ULONG) ProxyIDirectSound8::AddRef() { return ++m_ulRef; }
STDMETHODIMP_(ULONG) ProxyIDirectSound8::Release() { m_ulRef--; if (m_ulRef == 0) { m_pRealDSound->Release(); delete this; return 0; } return m_ulRef; }
STDMETHODIMP ProxyIDirectSound8::CreateSoundBuffer(_In_ LPCDSBUFFERDESC pcDSBufferDesc, _Out_ LPDIRECTSOUNDBUFFER* ppDSBuffer, _In_opt_ LPUNKNOWN pUnkOuter) { if (!ppDSBuffer || !pcDSBufferDesc) return DSERR_INVALIDPARAM; *ppDSBuffer = nullptr; bool isPrimary = (pcDSBufferDesc->dwFlags & DSBCAPS_PRIMARYBUFFER); IDirectSoundBuffer* pRealBuffer = nullptr; HRESULT hr = m_pRealDSound->CreateSoundBuffer(pcDSBufferDesc, &pRealBuffer, pUnkOuter); if (SUCCEEDED(hr) && pRealBuffer) { *ppDSBuffer = new ProxyIDirectSoundBuffer(pRealBuffer, isPrimary); } return hr; }
STDMETHODIMP ProxyIDirectSound8::GetCaps(_Out_ LPDSCAPS pDSCaps) { return m_pRealDSound->GetCaps(pDSCaps); }
STDMETHODIMP ProxyIDirectSound8::DuplicateSoundBuffer(_In_ LPDIRECTSOUNDBUFFER pDSBufferOriginal, _Out_ LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate) { if (!pDSBufferOriginal || !ppDSBufferDuplicate) return DSERR_INVALIDPARAM; *ppDSBufferDuplicate = nullptr; auto pProxyBufferOriginal = static_cast<ProxyIDirectSoundBuffer*>(pDSBufferOriginal); IDirectSoundBuffer* pRealBufferDuplicate = nullptr; HRESULT hr = m_pRealDSound->DuplicateSoundBuffer(pProxyBufferOriginal->GetReal(), &pRealBufferDuplicate); if (SUCCEEDED(hr) && pRealBufferDuplicate) { *ppDSBufferDuplicate = new ProxyIDirectSoundBuffer(pRealBufferDuplicate, false); } return hr; }
STDMETHODIMP ProxyIDirectSound8::SetCooperativeLevel(HWND hwnd, DWORD dwLevel) { return m_pRealDSound->SetCooperativeLevel(hwnd, dwLevel); }
STDMETHODIMP ProxyIDirectSound8::Compact() { return m_pRealDSound->Compact(); }
STDMETHODIMP ProxyIDirectSound8::GetSpeakerConfig(_Out_ LPDWORD pdwSpeakerConfig) { return m_pRealDSound->GetSpeakerConfig(pdwSpeakerConfig); }
STDMETHODIMP ProxyIDirectSound8::SetSpeakerConfig(DWORD dwSpeakerConfig) { return m_pRealDSound->SetSpeakerConfig(dwSpeakerConfig); }
STDMETHODIMP ProxyIDirectSound8::Initialize(_In_opt_ LPCGUID pcGuidDevice) { return m_pRealDSound->Initialize(pcGuidDevice); }
STDMETHODIMP ProxyIDirectSound8::VerifyCertification(_Out_ LPDWORD pdwCertified) { return m_pRealDSound->VerifyCertification(pdwCertified); }
#pragma endregion

// --- ProxyIDirectSoundBuffer 实现 (核心逻辑) ---
#pragma region ProxyIDirectSoundBuffer Implementation

ProxyIDirectSoundBuffer::ProxyIDirectSoundBuffer(IDirectSoundBuffer* pRealBuffer, bool isPrimary)
    : m_pRealBuffer(pRealBuffer), m_pRealBuffer8(nullptr), m_ulRef(1),
    m_isPrimaryBuffer(isPrimary), m_lIndividualVolume(DSBVOLUME_MAX),
    m_pBackupData(nullptr), m_dwBackupDataSize(0), m_hasBeenBackedUp(false)
{
    m_bufferId = s_nextBufferId++;
    m_pRealBuffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&m_pRealBuffer8);
    Log("[DSB-ID:%u] Created. IsPrimary: %d", m_bufferId, isPrimary);
    if (!m_isPrimaryBuffer) {
        std::lock_guard<std::mutex> lock(g_bufferMutex);
        g_activeBuffers.push_back(this);
    }
}

ProxyIDirectSoundBuffer::~ProxyIDirectSoundBuffer() {
    if (!m_isPrimaryBuffer) {
        std::lock_guard<std::mutex> lock(g_bufferMutex);
        g_activeBuffers.erase(std::remove(g_activeBuffers.begin(), g_activeBuffers.end(), this), g_activeBuffers.end());
    }
    if (m_pRealBuffer8) m_pRealBuffer8->Release();
    if (m_pBackupData) {
        free(m_pBackupData);
        m_pBackupData = nullptr;
    }
    Log("[DSB-ID:%u] Destroyed.", m_bufferId);
}

void ProxyIDirectSoundBuffer::TryBackupData(LPVOID pData1, DWORD dwSize1, LPVOID pData2, DWORD dwSize2) {
    if (m_hasBeenBackedUp || (!pData1 && dwSize1 == 0)) return;

    DSBCAPS caps;
    caps.dwSize = sizeof(DSBCAPS);
    if (FAILED(m_pRealBuffer->GetCaps(&caps)) || caps.dwBufferBytes == 0) return;

    m_dwBackupDataSize = caps.dwBufferBytes;
    m_pBackupData = malloc(m_dwBackupDataSize);
    if (!m_pBackupData) return;

    if (pData1 && dwSize1 > 0) memcpy(m_pBackupData, pData1, dwSize1);
    if (pData2 && dwSize2 > 0 && (dwSize1 + dwSize2) <= m_dwBackupDataSize) memcpy(static_cast<BYTE*>(m_pBackupData) + dwSize1, pData2, dwSize2);

    m_hasBeenBackedUp = true;
    Log("    [DSB-ID:%u] Data backed up (%u bytes).", m_bufferId, m_dwBackupDataSize);
}

void ProxyIDirectSoundBuffer::RestoreData() {
    if (!m_hasBeenBackedUp || !m_pBackupData) return;
    LPVOID pAudioPtr1 = nullptr, pAudioPtr2 = nullptr;
    DWORD dwAudioBytes1 = 0, dwAudioBytes2 = 0;
    if (SUCCEEDED(m_pRealBuffer->Lock(0, m_dwBackupDataSize, &pAudioPtr1, &dwAudioBytes1, &pAudioPtr2, &dwAudioBytes2, DSBLOCK_ENTIREBUFFER))) {
        if (pAudioPtr1) memcpy(pAudioPtr1, m_pBackupData, dwAudioBytes1);
        if (pAudioPtr2) memcpy(pAudioPtr2, static_cast<BYTE*>(m_pBackupData) + dwAudioBytes1, dwAudioBytes2);
        m_pRealBuffer->Unlock(pAudioPtr1, dwAudioBytes1, pAudioPtr2, dwAudioBytes2);
    }
}

void ProxyIDirectSoundBuffer::MuteData() {
    DSBCAPS caps;
    caps.dwSize = sizeof(DSBCAPS);
    if (FAILED(m_pRealBuffer->GetCaps(&caps)) || caps.dwBufferBytes == 0) return;

    LPVOID pAudioPtr1 = nullptr, pAudioPtr2 = nullptr;
    DWORD dwAudioBytes1 = 0, dwAudioBytes2 = 0;
    if (SUCCEEDED(m_pRealBuffer->Lock(0, caps.dwBufferBytes, &pAudioPtr1, &dwAudioBytes1, &pAudioPtr2, &dwAudioBytes2, DSBLOCK_ENTIREBUFFER))) {
        WAVEFORMATEX wfx;
        int silentValue = (SUCCEEDED(GetFormat(&wfx, sizeof(wfx), NULL)) && wfx.wBitsPerSample == 8) ? 128 : 0;
        if (pAudioPtr1) memset(pAudioPtr1, silentValue, dwAudioBytes1);
        if (pAudioPtr2) memset(pAudioPtr2, silentValue, dwAudioBytes2);
        m_pRealBuffer->Unlock(pAudioPtr1, dwAudioBytes1, pAudioPtr2, dwAudioBytes2);
    }
}

HRESULT ProxyIDirectSoundBuffer::UpdateVolume() {
    if (m_isPrimaryBuffer) return DS_OK;
    const LONG GAME_VOL_MIN = -2500;
    if (g_masterSfxVolume <= GAME_VOL_MIN) {
        return m_pRealBuffer->SetVolume(DSBVOLUME_MIN);
    }
    LONG combined_volume = g_masterSfxVolume + m_lIndividualVolume;
    if (combined_volume < DSBVOLUME_MIN) combined_volume = DSBVOLUME_MIN;
    if (combined_volume > DSBVOLUME_MAX) combined_volume = DSBVOLUME_MAX;
    return m_pRealBuffer->SetVolume(combined_volume);
}

// --- COM 接口实现 (关键部分) ---
STDMETHODIMP ProxyIDirectSoundBuffer::QueryInterface(REFIID riid, _Out_ LPVOID* ppvObj) { if (!ppvObj) return E_POINTER; *ppvObj = nullptr; if (riid == IID_IUnknown || riid == IID_IDirectSoundBuffer) { *ppvObj = this; AddRef(); return S_OK; } if (riid == IID_IDirectSoundBuffer8 && m_pRealBuffer8) { *ppvObj = this; AddRef(); return S_OK; } return m_pRealBuffer->QueryInterface(riid, ppvObj); }
STDMETHODIMP_(ULONG) ProxyIDirectSoundBuffer::AddRef() { return ++m_ulRef; }
STDMETHODIMP_(ULONG) ProxyIDirectSoundBuffer::Release() { m_ulRef--; if (m_ulRef == 0) { m_pRealBuffer->Release(); delete this; return 0; } return m_ulRef; }
STDMETHODIMP ProxyIDirectSoundBuffer::GetCaps(_Out_ LPDSBCAPS pDSBufferCaps) { return m_pRealBuffer->GetCaps(pDSBufferCaps); }
STDMETHODIMP ProxyIDirectSoundBuffer::GetCurrentPosition(_Out_opt_ LPDWORD pdwCurrentPlayCursor, _Out_opt_ LPDWORD pdwCurrentWriteCursor) { return m_pRealBuffer->GetCurrentPosition(pdwCurrentPlayCursor, pdwCurrentWriteCursor); }
STDMETHODIMP ProxyIDirectSoundBuffer::GetFormat(_Out_writes_bytes_opt_(dwSizeAllocated) LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, _Out_opt_ LPDWORD pdwSizeWritten) { return m_pRealBuffer->GetFormat(pwfxFormat, dwSizeAllocated, pdwSizeWritten); }
STDMETHODIMP ProxyIDirectSoundBuffer::GetPan(_Out_ LPLONG plPan) { return m_pRealBuffer->GetPan(plPan); }
STDMETHODIMP ProxyIDirectSoundBuffer::GetFrequency(_Out_ LPDWORD pdwFrequency) { return m_pRealBuffer->GetFrequency(pdwFrequency); }
STDMETHODIMP ProxyIDirectSoundBuffer::GetStatus(_Out_ LPDWORD pdwStatus) { return m_pRealBuffer->GetStatus(pdwStatus); }
STDMETHODIMP ProxyIDirectSoundBuffer::Initialize(_In_ LPDIRECTSOUND pDirectSound, _In_ LPCDSBUFFERDESC pcDSBufferDesc) { if (auto pProxy = dynamic_cast<ProxyIDirectSound*>(pDirectSound)) { return m_pRealBuffer->Initialize(pProxy->GetReal(), pcDSBufferDesc); } if (auto pProxy8 = dynamic_cast<ProxyIDirectSound8*>(pDirectSound)) { return m_pRealBuffer->Initialize(pProxy8->GetReal(), pcDSBufferDesc); } return m_pRealBuffer->Initialize(pDirectSound, pcDSBufferDesc); }
STDMETHODIMP ProxyIDirectSoundBuffer::Lock(DWORD dwOffset, DWORD dwBytes, _Out_writes_bytes_(*pdwAudioBytes1) LPVOID* ppvAudioPtr1, _Out_ LPDWORD pdwAudioBytes1, _Out_writes_bytes_opt_(*pdwAudioBytes2) LPVOID* ppvAudioPtr2, _Out_opt_ LPDWORD pdwAudioBytes2, DWORD dwFlags) { return m_pRealBuffer->Lock(dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2, pdwAudioBytes2, dwFlags); }
STDMETHODIMP ProxyIDirectSoundBuffer::SetCurrentPosition(DWORD dwNewPosition) { return m_pRealBuffer->SetCurrentPosition(dwNewPosition); }
STDMETHODIMP ProxyIDirectSoundBuffer::SetFormat(_In_ LPCWAVEFORMATEX pcfxFormat) { return m_pRealBuffer->SetFormat(pcfxFormat); }
STDMETHODIMP ProxyIDirectSoundBuffer::SetPan(LONG lPan) { return m_pRealBuffer->SetPan(lPan); }
STDMETHODIMP ProxyIDirectSoundBuffer::SetFrequency(DWORD dwFrequency) { return m_pRealBuffer->SetFrequency(dwFrequency); }
STDMETHODIMP ProxyIDirectSoundBuffer::Stop() { return m_pRealBuffer->Stop(); }
STDMETHODIMP ProxyIDirectSoundBuffer::Restore() { return m_pRealBuffer->Restore(); }
STDMETHODIMP ProxyIDirectSoundBuffer::SetFX(DWORD dwEffectsCount, _In_reads_opt_(dwEffectsCount) LPDSEFFECTDESC pdsfxDesc, _Out_writes_opt_(dwEffectsCount) LPDWORD pdwResultCodes) { if (!m_pRealBuffer8) return DSERR_UNSUPPORTED; return m_pRealBuffer8->SetFX(dwEffectsCount, pdsfxDesc, pdwResultCodes); }
STDMETHODIMP ProxyIDirectSoundBuffer::AcquireResources(DWORD dwFlags, DWORD dwEffectsCount, _Out_writes_all_(dwEffectsCount) LPDWORD pdwResultCodes) { if (!m_pRealBuffer8) return DSERR_UNSUPPORTED; return m_pRealBuffer8->AcquireResources(dwFlags, dwEffectsCount, pdwResultCodes); }
STDMETHODIMP ProxyIDirectSoundBuffer::GetObjectInPath(_In_ REFGUID rguidObject, DWORD dwIndex, _In_ REFGUID rguidInterface, _Out_ LPVOID* ppObject) { if (!m_pRealBuffer8) return DSERR_UNSUPPORTED; return m_pRealBuffer8->GetObjectInPath(rguidObject, dwIndex, rguidInterface, ppObject); }

/**
* @brief [核心修复] 获取音量 (UI稳定性的基石)
* @remark 永远返回用户设置的真实音量(`g_masterSfxVolume`)，以欺骗游戏UI，使其滑块位置正确。
*/
STDMETHODIMP ProxyIDirectSoundBuffer::GetVolume(_Out_ LPLONG plVolume) {
    if (!plVolume) return DSERR_INVALIDPARAM;
    *plVolume = g_masterSfxVolume;
    return DS_OK;
}

/**
* @brief [核心修复] 设置音量
* @remark 主缓冲区调用会更新全局音量并广播；次级缓冲区调用会记录其独立音量。
*/
STDMETHODIMP ProxyIDirectSoundBuffer::SetVolume(LONG lVolume) {
    if (m_isPrimaryBuffer) {
        if (lVolume != g_masterSfxVolume) {
            g_masterSfxVolume = lVolume;
            Log("[DSB-ID:%u] Primary. Master volume set to: %ld. Broadcasting...", m_bufferId, g_masterSfxVolume);
            std::lock_guard<std::mutex> lock(g_bufferMutex);
            for (auto& buffer : g_activeBuffers) {
                buffer->UpdateVolume();
            }
        }
        return DS_OK;
    }
    else {
        m_lIndividualVolume = lVolume;
        return UpdateVolume();
    }
}

/**
* @brief [核心修复] 播放 (数据完整性的守门员)
* @remark 这是消除爆音的最终防线。它在播放前，强制将缓冲区的数据写入为原始数据或静音数据。
*/
STDMETHODIMP ProxyIDirectSoundBuffer::Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags)
{
    if (!m_isPrimaryBuffer) {
        const LONG GAME_VOL_MIN = -2500;
        if (g_masterSfxVolume <= GAME_VOL_MIN) {
            MuteData();
        }
        else {
            RestoreData();
            UpdateVolume();
        }
    }
    return m_pRealBuffer->Play(dwReserved1, dwPriority, dwFlags);
}

/**
* @brief [核心修复] 解锁缓冲区 (数据备份的唯一时机)
* @remark 在游戏调用 Unlock 后，代表它已将有效的音频数据写入缓冲区，此时是我们进行一次性备份的最佳时机。
*/
STDMETHODIMP ProxyIDirectSoundBuffer::Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) {
    TryBackupData(pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
    return m_pRealBuffer->Unlock(pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
}

#pragma endregion
