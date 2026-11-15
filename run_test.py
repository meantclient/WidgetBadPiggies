import subprocess, os

code = '''
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <iostream>

int main() {
    CoInitialize(NULL);
    ISpRecognizer* pRec = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_ALL, IID_ISpRecognizer, (void**)&pRec);
    if (FAILED(hr)) { std::cout << "pRec fail: 0x" << std::hex << hr << std::endl; return 1; }
    pRec->SetInput(NULL, TRUE);
    ISpRecoContext* pCtx = NULL;
    hr = pRec->CreateRecoContext(&pCtx);
    if (FAILED(hr)) { std::cout << "pCtx fail: 0x" << std::hex << hr << std::endl; return 2; }
    ISpRecoGrammar* pGrammar = NULL;
    hr = pCtx->CreateGrammar(1, &pGrammar);
    if (FAILED(hr)) return 3;

    SPSTATEHANDLE hInitial = NULL;
    pGrammar->GetRule(L"TestRule", 1, SPRAF_TopLevel | SPRAF_Active, TRUE, &hInitial);

    HRESULT hrEn = pGrammar->AddWordTransition(hInitial, NULL, L"hello", L" ", SPWT_LEXICAL, 1.0f, NULL);
    HRESULT hrRu = pGrammar->AddWordTransition(hInitial, NULL, L"свин", L" ", SPWT_LEXICAL, 1.0f, NULL);
    HRESULT hrTrans = pGrammar->AddWordTransition(hInitial, NULL, L"svin", L" ", SPWT_LEXICAL, 1.0f, NULL);

    std::cout << "English 'hello': 0x" << std::hex << hrEn << std::endl;
    std::cout << "Cyrillic 'свин': 0x" << std::hex << hrRu << std::endl;
    std::cout << "Translit 'svin': 0x" << std::hex << hrTrans << std::endl;
    return 0;
}
'''
with open('test_sapi_trans.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

with open('compile_test.bat', 'w') as f:
    f.write(r'''@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=C:\Windows\System32;%PATH%"
set "INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared"
set "LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
cl.exe /nologo /std:c++17 /EHsc /utf-8 /Iinclude test_sapi_trans.cpp ole32.lib /link /OUT:test_sapi_trans.exe >nul
test_sapi_trans.exe
''')

os.system('compile_test.bat')
