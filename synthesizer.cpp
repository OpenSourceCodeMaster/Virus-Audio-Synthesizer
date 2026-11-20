#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

enum ControlId {
    ID_COMBO_FORMULA = 101,
    ID_SLIDER_P1,
    ID_SLIDER_P2,
    ID_SLIDER_P3,
    ID_SLIDER_RATE,
    ID_BTN_PREVIEW,
    ID_BTN_STOP,
    ID_BTN_EXPORT,
    ID_LBL_STATUS
};

struct SynthParams {
    int formulaIndex = 0;
    int paramA = 12;       // Bit shift A (1 - 16)
    int paramB = 8;        // Bit shift B (1 - 16)
    int paramC = 63;       // Bitwise mask (1 - 255)
    int sampleRate = 8000; // 4000 Hz - 22050 Hz
};

SynthParams g_params;
HWAVEOUT g_hWaveOut = nullptr;
WAVEHDR g_waveHeader = {};
std::vector<uint8_t> g_previewBuffer;
bool g_isPlaying = false;

HWND hComboFormula = nullptr;
HWND hSliderA = nullptr, hSliderB = nullptr, hSliderC = nullptr, hSliderRate = nullptr;
HWND hValA = nullptr, hValB = nullptr, hValC = nullptr, hValRate = nullptr;
HWND hStatus = nullptr;

uint8_t EvaluateFormula(uint32_t t, const SynthParams& p) {
    uint32_t out = 0;
    switch (p.formulaIndex) {
    case 0: // Classic GDI Bytebeat Screech
        out = (t * ((t >> p.paramA | t >> p.paramB) & p.paramC & (t >> 4))) ^ (t >> 7);
        break;
    case 1: // Chaotic Siren / Glitch Alarm
        out = ((t >> p.paramA) * (t >> p.paramB)) | (t >> (p.paramC % 12 + 1));
        break;
    case 2: // Heavy Bit-Crushed Distortion
        out = (t * ((t >> p.paramA) & (t >> p.paramB))) ^ (t * (static_cast<uint32_t>(p.paramC) >> 2));
        break;
    }
    return static_cast<uint8_t>(out & 0xFF);
}

std::vector<uint8_t> GenerateLoopableBuffer(const SynthParams& p, int minDurationSec) {
    const uint32_t period = 65536;
    const uint32_t targetSamples = static_cast<uint32_t>(p.sampleRate) * static_cast<uint32_t>(minDurationSec);
    const uint32_t loops = (targetSamples / period) + 1;
    const uint32_t totalSamples = loops * period;

    std::vector<uint8_t> buffer(totalSamples);
    for (uint32_t t = 0; t < totalSamples; ++t) {
        buffer[t] = EvaluateFormula(t, p);
    }
    return buffer;
}

void StopPreview() {
    if (g_hWaveOut) {
        waveOutReset(g_hWaveOut);
        waveOutUnprepareHeader(g_hWaveOut, &g_waveHeader, sizeof(WAVEHDR));
        waveOutClose(g_hWaveOut);
        g_hWaveOut = nullptr;
    }
    g_isPlaying = false;
    if (hStatus) {
        SetWindowText(hStatus, L"Status: Stopped.");
    }
}

void StartPreview() {
    StopPreview();

    g_previewBuffer = GenerateLoopableBuffer(g_params, 4);

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = static_cast<DWORD>(g_params.sampleRate);
    wfx.wBitsPerSample = 8;
    wfx.nBlockAlign = 1;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec;

    if (waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        SetWindowText(hStatus, L"Status: Failed to open audio device.");
        return;
    }

    ZeroMemory(&g_waveHeader, sizeof(WAVEHDR));
    g_waveHeader.lpData = reinterpret_cast<LPSTR>(g_previewBuffer.data());
    g_waveHeader.dwBufferLength = static_cast<DWORD>(g_previewBuffer.size());
    g_waveHeader.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    g_waveHeader.dwLoops = 100;

    waveOutPrepareHeader(g_hWaveOut, &g_waveHeader, sizeof(WAVEHDR));
    waveOutWrite(g_hWaveOut, &g_waveHeader, sizeof(WAVEHDR));
    g_isPlaying = true;
    SetWindowText(hStatus, L"Status: Playing live loop...");
}

bool ExportToMP3(const std::wstring& filePath) {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return false;

    const uint32_t outRate = 44100;
    const uint32_t period = 65536;
    const double step = static_cast<double>(g_params.sampleRate) / static_cast<double>(outRate);
    const uint32_t cycleSamples = static_cast<uint32_t>(period / step);

    uint32_t totalOutSamples = cycleSamples;
    while (totalOutSamples < outRate * 4) {
        totalOutSamples += cycleSamples;
    }

    std::vector<int16_t> pcm16(totalOutSamples);
    for (uint32_t i = 0; i < totalOutSamples; ++i) {
        uint32_t t = static_cast<uint32_t>(i * step);
        uint8_t raw8 = EvaluateFormula(t, g_params);
        pcm16[i] = static_cast<int16_t>((static_cast<int>(raw8) - 128) * 256);
    }

    IMFSinkWriter* pWriter = nullptr;
    IMFMediaType* pOutType = nullptr;
    IMFMediaType* pInType = nullptr;
    DWORD streamIndex = 0;
    bool success = false;

    do {
        hr = MFCreateSinkWriterFromURL(filePath.c_str(), nullptr, nullptr, &pWriter);
        if (FAILED(hr)) break;

        hr = MFCreateMediaType(&pOutType);
        if (FAILED(hr)) break;
        pOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_MP3);
        pOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
        pOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, outRate);
        pOutType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000); // 128 kbps

        hr = pWriter->AddStream(pOutType, &streamIndex);
        if (FAILED(hr)) break;

        hr = MFCreateMediaType(&pInType);
        if (FAILED(hr)) break;
        pInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pInType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        pInType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
        pInType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, outRate);
        pInType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        pInType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
        pInType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, outRate * 2);

        hr = pWriter->SetInputMediaType(streamIndex, pInType, nullptr);
        if (FAILED(hr)) break;

        hr = pWriter->BeginWriting();
        if (FAILED(hr)) break;

        DWORD cbBuffer = static_cast<DWORD>(pcm16.size() * sizeof(int16_t));
        IMFMediaBuffer* pBuffer = nullptr;
        hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
        if (SUCCEEDED(hr)) {
            BYTE* pData = nullptr;
            pBuffer->Lock(&pData, nullptr, nullptr);
            std::memcpy(pData, pcm16.data(), cbBuffer);
            pBuffer->Unlock();
            pBuffer->SetCurrentLength(cbBuffer);

            IMFSample* pSample = nullptr;
            if (SUCCEEDED(MFCreateSample(&pSample))) {
                pSample->AddBuffer(pBuffer);
                pSample->SetSampleTime(0);
                LONGLONG duration = (static_cast<LONGLONG>(totalOutSamples) * 10000000LL) / outRate;
                pSample->SetSampleDuration(duration);

                hr = pWriter->WriteSample(streamIndex, pSample);
                pSample->Release();
            }
            pBuffer->Release();
        }
        if (FAILED(hr)) break;

        hr = pWriter->Finalize();
        success = SUCCEEDED(hr);
    } while (false);

    if (pInType) pInType->Release();
    if (pOutType) pOutType->Release();
    if (pWriter) pWriter->Release();
    MFShutdown();

    return success;
}

void UpdateLabels() {
    SetWindowText(hValA, std::to_wstring(g_params.paramA).c_str());
    SetWindowText(hValB, std::to_wstring(g_params.paramB).c_str());
    SetWindowText(hValC, std::to_wstring(g_params.paramC).c_str());
    SetWindowText(hValRate, (std::to_wstring(g_params.sampleRate) + L" Hz").c_str());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        auto MakeLabel = [&](const wchar_t* text, int x, int y, int w, int h) {
            HWND lbl = CreateWindow(L"STATIC", text, WS_VISIBLE | WS_CHILD,
                x, y, w, h, hwnd, nullptr, nullptr, nullptr);
            SendMessage(lbl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            return lbl;
            };

        MakeLabel(L"Algorithm Formula:", 20, 20, 130, 20);
        hComboFormula = CreateWindow(L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_VISIBLE | WS_CHILD,
            160, 16, 250, 120, hwnd,
            reinterpret_cast<HMENU>(ID_COMBO_FORMULA), nullptr, nullptr);
        SendMessage(hComboFormula, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SendMessage(hComboFormula, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1. Classic GDI Bytebeat Screech"));
        SendMessage(hComboFormula, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2. Chaotic Siren / Glitch Alarm"));
        SendMessage(hComboFormula, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"3. Heavy Bit-Crushed Distortion"));
        SendMessage(hComboFormula, CB_SETCURSEL, 0, 0);

        auto MakeSlider = [&](const wchar_t* name, int y, int id, int minV, int maxV, int initV,
            HWND& outSlider, HWND& outVal) {
                MakeLabel(name, 20, y, 130, 20);
                outSlider = CreateWindow(TRACKBAR_CLASS, nullptr,
                    WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS,
                    155, y - 4, 190, 28, hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                SendMessage(outSlider, TBM_SETRANGE, TRUE, MAKELPARAM(minV, maxV));
                SendMessage(outSlider, TBM_SETPOS, TRUE, initV);
                outVal = MakeLabel(L"0", 355, y, 65, 20);
            };

        MakeSlider(L"Bit Shift A:", 60, ID_SLIDER_P1, 1, 16, g_params.paramA, hSliderA, hValA);
        MakeSlider(L"Bit Shift B:", 100, ID_SLIDER_P2, 1, 16, g_params.paramB, hSliderB, hValB);
        MakeSlider(L"Bitwise Mask:", 140, ID_SLIDER_P3, 1, 255, g_params.paramC, hSliderC, hValC);
        MakeSlider(L"Sample Rate:", 180, ID_SLIDER_RATE, 4000, 22050, g_params.sampleRate, hSliderRate, hValRate);

        HWND btnPlay = CreateWindow(L"BUTTON", L"Preview Loop", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, 230, 120, 35, hwnd, reinterpret_cast<HMENU>(ID_BTN_PREVIEW), nullptr, nullptr);
        HWND btnStop = CreateWindow(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            150, 230, 100, 35, hwnd, reinterpret_cast<HMENU>(ID_BTN_STOP), nullptr, nullptr);
        HWND btnExport = CreateWindow(L"BUTTON", L"Export Looping MP3", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            260, 230, 150, 35, hwnd, reinterpret_cast<HMENU>(ID_BTN_EXPORT), nullptr, nullptr);

        SendMessage(btnPlay, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SendMessage(btnStop, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SendMessage(btnExport, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        hStatus = MakeLabel(L"Status: Ready (Lower volume before previewing!)", 20, 280, 390, 25);
        UpdateLabels();
        break;
    }

    case WM_HSCROLL: {
        g_params.paramA = static_cast<int>(SendMessage(hSliderA, TBM_GETPOS, 0, 0));
        g_params.paramB = static_cast<int>(SendMessage(hSliderB, TBM_GETPOS, 0, 0));
        g_params.paramC = static_cast<int>(SendMessage(hSliderC, TBM_GETPOS, 0, 0));
        g_params.sampleRate = static_cast<int>(SendMessage(hSliderRate, TBM_GETPOS, 0, 0));
        UpdateLabels();

        if (g_isPlaying && LOWORD(wParam) == TB_ENDTRACK) {
            StartPreview();
        }
        break;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id == ID_COMBO_FORMULA && HIWORD(wParam) == CBN_SELCHANGE) {
            g_params.formulaIndex = static_cast<int>(SendMessage(hComboFormula, CB_GETCURSEL, 0, 0));
            if (g_isPlaying) {
                StartPreview();
            }
        }
        else if (id == ID_BTN_PREVIEW) {
            StartPreview();
        }
        else if (id == ID_BTN_STOP) {
            StopPreview();
        }
        else if (id == ID_BTN_EXPORT) {
            StopPreview();

            wchar_t fileName[MAX_PATH] = L"virus_loop.mp3";
            OPENFILENAME ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"MP3 Audio (*.mp3)\0*.mp3\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = L"mp3";

            if (GetSaveFileName(&ofn)) {
                SetWindowText(hStatus, L"Status: Encoding MP3...");
                if (ExportToMP3(fileName)) {
                    SetWindowText(hStatus, L"Status: Exported MP3 successfully!");
                }
                else {
                    SetWindowText(hStatus, L"Status: MP3 export failed.");
                }
            }
        }
        break;
    }

    case WM_DESTROY:
        StopPreview();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VirusSynthWindow";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        L"VirusSynthWindow",
        L"Virus Audio Synthesizer (Bytebeat Studio)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 355,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

int main() {
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}
