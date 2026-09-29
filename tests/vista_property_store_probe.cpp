#include <windows.h>
#include <shlobj.h>
#include <propsys.h>
#include <stdio.h>
int wmain(int argc, wchar_t **argv) {
    // Optional third argument "compat" asserts the Inno-specific fallback.
    if (argc < 2 || argc > 3) return 2;
    int failures=0;
    bool compat=argc==3 && !wcscmp(argv[2],L"compat");
    FILE *f = _wfopen(argv[1], L"w");
    if (!f) return 3;
    if(FAILED(CoInitialize(NULL))) {fclose(f);return 4;}
    const DWORD ids[] = {0,5,8,9,12,26,999};
    for (int i=0;i<7;++i) {
        IShellLinkW *link=NULL; IPropertyStore *ps=NULL;
        HRESULT hr;
        if(i%2) {
            MULTI_QI qi={&IID_IShellLinkW,NULL,E_FAIL};
            hr=CoCreateInstanceEx(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,NULL,1,&qi);
            if(SUCCEEDED(hr)) {hr=qi.hr;link=(IShellLinkW*)qi.pItf;}
        } else hr=CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void**)&link);
        if(FAILED(hr)) ++failures;
        fprintf(f,"pid=%lu create=%08lx ",ids[i],hr);
        if (SUCCEEDED(hr)) {
            link->SetPath(L"C:\\Windows\\System32\\notepad.exe");
            hr=link->QueryInterface(IID_IPropertyStore,(void**)&ps);
            if(FAILED(hr)) ++failures;
            fprintf(f,"QI=%08lx ",hr);
            if (SUCCEEDED(hr)) {
                PROPERTYKEY key={{0x9f4c2855,0x9f79,0x4b39,{0xa8,0xd0,0xe1,0xd4,0x2d,0xe1,0xd5,0xf3}},ids[i]};
                PROPVARIANT v; ZeroMemory(&v,sizeof(v));
                GUID guid={0x12345678,0x1234,0x1234,{1,2,3,4,5,6,7,8}};
                if (ids[i]==5) {v.vt=VT_BSTR;v.bstrVal=SysAllocString(L"VxKex.Probe");}
                else if (ids[i]==26) {v.vt=VT_CLSID;v.puuid=&guid;}
                else if (ids[i]==12) {v.vt=VT_UI4;v.ulVal=1;}
                else {v.vt=VT_BOOL;v.boolVal=VARIANT_TRUE;}
                if(ids[i]) fprintf(f,"Set=%08lx ",ps->SetValue(key,v));
                DWORD count=0;ps->GetCount(&count);fprintf(f,"Count=%lu ",count);
                IShellLinkW *owner=NULL;fprintf(f,"OwnerQI=%08lx ",ps->QueryInterface(IID_IShellLinkW,(void**)&owner));if(owner)owner->Release();
                HRESULT commit=ps->Commit();fprintf(f,"Commit=%08lx",commit);
                HRESULT expected=(compat && ids[i] && ids[i]!=999)?S_OK:E_INVALIDARG;
                if(commit!=expected) ++failures;
                IPersistFile *pf=NULL;
                if(SUCCEEDED(link->QueryInterface(IID_IPersistFile,(void**)&pf))) {
                    WCHAR path[MAX_PATH]; swprintf_s(path,MAX_PATH,L"C:\\VxKexProbe\\property-%lu.lnk",ids[i]);
                    HRESULT saved=pf->Save(path,TRUE);fprintf(f," Save=%08lx",saved);
                    if(FAILED(saved)) ++failures;
                    HRESULT invalid=pf->Save(L"Z:\\VxKexMissingDirectory\\fail.lnk",TRUE);
                    fprintf(f," InvalidSave=%08lx",invalid);if(SUCCEEDED(invalid)) ++failures;
                    pf->Release();
                    IShellLinkW *r=NULL;
                    if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void**)&r))) {
                        IPersistFile *rf=NULL; IPropertyStore *rp=NULL;
                        r->QueryInterface(IID_IPersistFile,(void**)&rf);
                        HRESULT lr=rf->Load(path,STGM_READ); fprintf(f," Load=%08lx",lr);
                        WCHAR target[MAX_PATH];WIN32_FIND_DATAW fd;
                        HRESULT tr=r->GetPath(target,MAX_PATH,&fd,SLGP_RAWPATH);
                        if(FAILED(lr)||FAILED(tr)||_wcsicmp(target,L"C:\\Windows\\System32\\notepad.exe")) ++failures;
                        if(SUCCEEDED(lr)&&SUCCEEDED(r->QueryInterface(IID_IPropertyStore,(void**)&rp))) {
                            PROPVARIANT val;ZeroMemory(&val,sizeof(val));
                            HRESULT gr=rp->GetValue(key,&val);fprintf(f," Get=%08lx vt=%u",gr,val.vt);
                            PropVariantClear(&val);rp->Release();
                        }
                        rf->Release();r->Release();
                    }
                }
                if(ids[i]==5) SysFreeString(v.bstrVal);
                ps->Release();
            }
            link->Release();
        }
        fprintf(f,"\n");fflush(f);
    }
    fprintf(f,"Failures=%d\n",failures);
    CoUninitialize();fclose(f);return failures?1:0;
}
