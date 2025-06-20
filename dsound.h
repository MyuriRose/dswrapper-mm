#pragma once

#include <windows.h>
#include <dsound.h>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstdio>

// --- 前向声明 ---
class ProxyIDirectSound;
class ProxyIDirectSound8;
class ProxyIDirectSoundBuffer;

// --- 日志函数 ---
void Log(const char* fmt, ...);

// --- 代理类定义 ---

// 代理旧版 IDirectSound 接口
class ProxyIDirectSound : public IDirectSound
{
private:
    IDirectSound* m_pRealDSound;
    ULONG m_ulRef;
public:
    ProxyIDirectSound(IDirectSound* pRealDSound);
    virtual ~ProxyIDirectSound();
    IDirectSound* GetReal() { return m_pRealDSound; }

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, _Out_ LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();
    // IDirectSound
    STDMETHOD(CreateSoundBuffer)(_In_ LPCDSBUFFERDESC pcDSBufferDesc, _Out_ LPDIRECTSOUNDBUFFER* ppDSBuffer, _In_opt_ LPUNKNOWN pUnkOuter);
    STDMETHOD(GetCaps)(_Out_ LPDSCAPS pDSCaps);
    STDMETHOD(DuplicateSoundBuffer)(_In_ LPDIRECTSOUNDBUFFER pDSBufferOriginal, _Out_ LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate);
    STDMETHOD(SetCooperativeLevel)(HWND hwnd, DWORD dwLevel);
    STDMETHOD(Compact)();
    STDMETHOD(GetSpeakerConfig)(_Out_ LPDWORD pdwSpeakerConfig);
    STDMETHOD(SetSpeakerConfig)(DWORD dwSpeakerConfig);
    STDMETHOD(Initialize)(_In_opt_ LPCGUID pcGuidDevice);
};

// 代理新版 IDirectSound8 接口
class ProxyIDirectSound8 : public IDirectSound8
{
private:
    IDirectSound8* m_pRealDSound;
    ULONG m_ulRef;
public:
    ProxyIDirectSound8(IDirectSound8* pRealDSound);
    virtual ~ProxyIDirectSound8();
    IDirectSound8* GetReal() { return m_pRealDSound; }

    // IUnknown & IDirectSound
    STDMETHOD(QueryInterface)(REFIID riid, _Out_ LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();
    STDMETHOD(CreateSoundBuffer)(_In_ LPCDSBUFFERDESC pcDSBufferDesc, _Out_ LPDIRECTSOUNDBUFFER* ppDSBuffer, _In_opt_ LPUNKNOWN pUnkOuter);
    STDMETHOD(GetCaps)(_Out_ LPDSCAPS pDSCaps);
    STDMETHOD(DuplicateSoundBuffer)(_In_ LPDIRECTSOUNDBUFFER pDSBufferOriginal, _Out_ LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate);
    STDMETHOD(SetCooperativeLevel)(HWND hwnd, DWORD dwLevel);
    STDMETHOD(Compact)();
    STDMETHOD(GetSpeakerConfig)(_Out_ LPDWORD pdwSpeakerConfig);
    STDMETHOD(SetSpeakerConfig)(DWORD dwSpeakerConfig);
    STDMETHOD(Initialize)(_In_opt_ LPCGUID pcGuidDevice);
    // IDirectSound8
    STDMETHOD(VerifyCertification)(_Out_ LPDWORD pdwCertified);
};

/**
 * @class ProxyIDirectSoundBuffer
 * @brief 代理声音缓冲区接口的核心类，实现了最终的音量修复方案。
 * @remark
 * 此方案的核心是“数据级静音结合混合音量”。它通过在播放前直接操作缓冲区数据来根除爆音，
 * 同时通过备份/恢复机制保护静态音效数据，并通过混合算法保留各音效的独立音量。
 */
class ProxyIDirectSoundBuffer : public IDirectSoundBuffer8
{
private:
    IDirectSoundBuffer* m_pRealBuffer;
    IDirectSoundBuffer8* m_pRealBuffer8;
    ULONG m_ulRef;
    bool m_isPrimaryBuffer;
    UINT m_bufferId;
    static UINT s_nextBufferId;

    // --- 核心修复逻辑所需的成员变量 ---
    LONG  m_lIndividualVolume; // 记录游戏为本缓冲区设置的独立音量
    void* m_pBackupData;       // 指向备份的原始音频数据的指针
    DWORD m_dwBackupDataSize;  // 备份数据的大小（字节）
    bool  m_hasBeenBackedUp;   // 状态标记，确保只在第一次Unlock时备份数据

    // --- 核心辅助函数 ---
    HRESULT UpdateVolume();
    void TryBackupData(LPVOID pData1, DWORD dwSize1, LPVOID pData2, DWORD dwSize2);
    void RestoreData();
    void MuteData();

public:
    ProxyIDirectSoundBuffer(IDirectSoundBuffer* pRealBuffer, bool isPrimary);
    virtual ~ProxyIDirectSoundBuffer();
    IDirectSoundBuffer* GetReal() { return m_pRealBuffer; }

    // --- COM 接口方法 ---
    STDMETHOD(QueryInterface) (REFIID riid, _Out_ LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef) ();
    STDMETHOD_(ULONG, Release) ();

    // IDirectSoundBuffer
    STDMETHOD(GetCaps)(_Out_ LPDSBCAPS pDSBufferCaps);
    STDMETHOD(GetCurrentPosition)(_Out_opt_ LPDWORD pdwCurrentPlayCursor, _Out_opt_ LPDWORD pdwCurrentWriteCursor);
    STDMETHOD(GetFormat)(_Out_writes_bytes_opt_(dwSizeAllocated) LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, _Out_opt_ LPDWORD pdwSizeWritten);
    STDMETHOD(GetVolume)(_Out_ LPLONG plVolume);
    STDMETHOD(GetPan)(_Out_ LPLONG plPan);
    STDMETHOD(GetFrequency)(_Out_ LPDWORD pdwFrequency);
    STDMETHOD(GetStatus)(_Out_ LPDWORD pdwStatus);
    STDMETHOD(Initialize)(_In_ LPDIRECTSOUND pDirectSound, _In_ LPCDSBUFFERDESC pcDSBufferDesc);
    STDMETHOD(Lock)(DWORD dwOffset, DWORD dwBytes, _Out_writes_bytes_(*pdwAudioBytes1) LPVOID* ppvAudioPtr1, _Out_ LPDWORD pdwAudioBytes1, _Out_writes_bytes_opt_(*pdwAudioBytes2) LPVOID* ppvAudioPtr2, _Out_opt_ LPDWORD pdwAudioBytes2, DWORD dwFlags);
    STDMETHOD(Play)(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags);
    STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition);
    STDMETHOD(SetFormat)(_In_ LPCWAVEFORMATEX pcfxFormat);
    STDMETHOD(SetVolume)(LONG lVolume);
    STDMETHOD(SetPan)(LONG lPan);
    STDMETHOD(SetFrequency)(DWORD dwFrequency);
    STDMETHOD(Stop)();
    STDMETHOD(Unlock)(_In_reads_bytes_(dwAudioBytes1) LPVOID pvAudioPtr1, DWORD dwAudioBytes1, _In_reads_bytes_opt_(dwAudioBytes2) LPVOID pvAudioPtr2, DWORD dwAudioBytes2);
    STDMETHOD(Restore)();

    // IDirectSoundBuffer8
    STDMETHOD(SetFX)(DWORD dwEffectsCount, _In_reads_opt_(dwEffectsCount) LPDSEFFECTDESC pdsfxDesc, _Out_writes_opt_(dwEffectsCount) LPDWORD pdwResultCodes);
    STDMETHOD(AcquireResources)(DWORD dwFlags, DWORD dwEffectsCount, _Out_writes_all_(dwEffectsCount) LPDWORD pdwResultCodes);
    STDMETHOD(GetObjectInPath)(_In_ REFGUID rguidObject, DWORD dwIndex, _In_ REFGUID rguidInterface, _Out_ LPVOID* ppObject);
};
