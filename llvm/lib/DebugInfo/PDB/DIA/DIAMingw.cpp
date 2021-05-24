#if defined(__MINGW32__)

#include <windows.h>
#include <cstdio>
#include <comdef.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#elif defined(__cplusplus)

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    EXTERN_C const type DECLSPEC_SELECTANY name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#else

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    const type DECLSPEC_SELECTANY name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif

#ifdef __cplusplus
extern "C" {
#endif

MIDL_DEFINE_GUID(IID, IID_IDiaDataSource, 0x79f1bb5f, 0xb66e, 0x48e5, 0xb6,0xa9, 0x15,0x45,0xc3,0x23,0xca,0x3d);
MIDL_DEFINE_GUID(CLSID, CLSID_DiaSource, 0xe6756135, 0x1e65, 0x4d17, 0x85,0x76, 0x61,0x07,0x61,0x39,0x8c,0x3c);

#ifdef __cplusplus
}
#endif

HRESULT STDMETHODCALLTYPE NoRegCoCreate(const wchar_t *dllName, REFCLSID rclsid,
                                        REFIID riid, void **ppv) {
  HMODULE m = LoadLibraryExW(dllName, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (m) {
    decltype(DllGetClassObject) *fDllGetClassObject =
        reinterpret_cast<decltype(DllGetClassObject) *>(
            GetProcAddress(m, "DllGetClassObject"));
    if (fDllGetClassObject != nullptr) {
      IClassFactory *classFactory;
      HRESULT hr =
          fDllGetClassObject(rclsid, __uuidof(IClassFactory),
                             IID_PPV_ARGS_Helper<IClassFactory>(&classFactory));
      if (SUCCEEDED(hr)) {
        hr = classFactory->CreateInstance(nullptr, riid, ppv);
        classFactory->Release();
      }
    }
  }
  return HRESULT_FROM_WIN32(GetLastError());
}

namespace _com_util {
//
// Convert char * to BSTR
//
BSTR __stdcall ConvertStringToBSTR(const char *pSrc) {
  if (pSrc == NULL) {
    return NULL;
  } else {
    int size = strlen(pSrc) + 1;
    if (size <= INT_MAX) {
      int destSize = ::MultiByteToWideChar(CP_ACP, 0, pSrc, size, NULL, 0);
      if (destSize > 0) {
        BSTR pDest =
            (destSize < 0x1000)
                ? static_cast<BSTR>(::_alloca(destSize * sizeof(wchar_t)))
                : static_cast<BSTR>(::malloc(destSize * sizeof(wchar_t)));

        if (pDest == NULL) {
          _com_issue_error(E_OUTOFMEMORY);
          return NULL;
        }

        if (::MultiByteToWideChar(CP_ACP, 0, pSrc, size, pDest, destSize) ==
            0) {
          if (destSize >= 0x1000) {
            free(pDest);
          }
          _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
          return NULL;
        }

        BSTR pRet = ::SysAllocString(pDest);
        if (destSize >= 0x1000) {
          free(pDest);
        }
        if (pRet == NULL) {
          _com_issue_error(E_OUTOFMEMORY);
          return NULL;
        }
        return pRet;
      } else {
        _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
        return NULL;
      }
    } else {
      _com_issue_error(E_INVALIDARG);
      return NULL;
    }
  }
}
} // namespace _com_util
#endif
