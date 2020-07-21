#if defined(__MINGW32__)

#include <cstdio>

#include <comdef.h>

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
      return hr;
    }
  }
  return HRESULT_FROM_WIN32(GetLastError());
}

namespace _com_util {
//
// Convert char * to BSTR
//
BSTR __stdcall ConvertStringToBSTR(const char *pSrc) {
  if (pSrc == nullptr) {
    return nullptr;
  } else {
    size_t len = strlen(pSrc) + 1;
    if (len > INT_MAX) {
      _com_issue_error(E_INVALIDARG);
      return nullptr;
    }
    int size = ::MultiByteToWideChar(CP_ACP, 0, pSrc, static_cast<int>(len),
                                     nullptr, 0);
    if (size == 0) {
      _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
      return nullptr;
    }
    BSTR pDest;
    pDest = (size < 0x1000)
                ? static_cast<BSTR>(::_alloca(size * sizeof(wchar_t)))
                : static_cast<BSTR>(::malloc(size * sizeof(wchar_t)));

    if (pDest == nullptr) {
      _com_issue_error(E_OUTOFMEMORY);
      return nullptr;
    }

    if (::MultiByteToWideChar(CP_ACP, 0, pSrc, static_cast<int>(len), pDest,
                              size) == 0) {
      if (size >= 0x1000) {
        free(pDest);
      }
      _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
      return nullptr;
    }

    BSTR pRet = ::SysAllocString(pDest);
    if (size >= 0x1000) {
      free(pDest);
    }
    if (pRet == nullptr) {
      _com_issue_error(E_OUTOFMEMORY);
    }
    return pRet;
  }
}
} // namespace _com_util
#endif
