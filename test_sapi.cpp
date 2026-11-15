#include <windows.h>
#include <sapi.h>
#pragma comment(lib, "ole32.lib")

int main() {
    CoInitialize(NULL);
    ISpVoice* pVoice = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);
    if (SUCCEEDED(hr) && pVoice) {
        pVoice->Speak(L"Тест", SPF_ASYNC, NULL);
        pVoice->Release();
    }
    CoUninitialize();
    return 0;
}
