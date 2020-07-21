//===- DIASession.cpp - DIA implementation of IPDBSession -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "llvm/DebugInfo/PDB/DIA/DIASession.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumDebugStreams.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumFrameData.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumInjectedSources.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumLineNumbers.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumSectionContribs.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumSourceFiles.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumTables.h"
#include "llvm/DebugInfo/PDB/DIA/DIAError.h"
#include "llvm/DebugInfo/PDB/DIA/DIARawSymbol.h"
#include "llvm/DebugInfo/PDB/DIA/DIASourceFile.h"
#include "llvm/DebugInfo/PDB/DIA/DIASupport.h"
#include "llvm/DebugInfo/PDB/GenericError.h"
#include "llvm/DebugInfo/PDB/PDB.h"
#include "llvm/DebugInfo/PDB/PDBSymbolCompiland.h"
#include "llvm/DebugInfo/PDB/PDBSymbolExe.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

#ifdef __MINGW32__
#define _MSC_VER 1900
#endif

using namespace llvm;
using namespace llvm::pdb;

template <typename... Ts>
static Error ErrorFromHResult(HRESULT Result, const char *Str, Ts &&... Args) {
  SmallString<64> MessageStorage;
  StringRef Context;
  if (sizeof...(Args) > 0) {
    MessageStorage = formatv(Str, std::forward<Ts>(Args)...).str();
    Context = MessageStorage;
  } else
    Context = Str;

  switch (Result) {
  case E_PDB_NOT_FOUND:
    return errorCodeToError(std::error_code(ENOENT, std::generic_category()));
  case E_PDB_FORMAT:
    return make_error<DIAError>(dia_error_code::invalid_file_format, Context);
  case E_INVALIDARG:
    return make_error<DIAError>(dia_error_code::invalid_parameter, Context);
  case E_UNEXPECTED:
    return make_error<DIAError>(dia_error_code::already_loaded, Context);
  case E_PDB_INVALID_SIG:
  case E_PDB_INVALID_AGE:
    return make_error<DIAError>(dia_error_code::debug_info_mismatch, Context);
  default: {
    std::string S;
    raw_string_ostream OS(S);
    OS << "HRESULT: " << format_hex(static_cast<DWORD>(Result), 10, true)
       << ": " << Context;
    return make_error<DIAError>(dia_error_code::unspecified, OS.str());
  }
  }
}

static Error LoadDIA(ComPtr<IDiaDataSource> &DiaDataSource) {
  if (SUCCEEDED(CoCreateInstance(
          __uuidof(DiaSource), nullptr, CLSCTX_INPROC_SERVER,
          __uuidof(IDiaDataSource),
          reinterpret_cast<LPVOID *>(DiaDataSource.GetAddressOf()))))
    return Error::success();

// If the CoCreateInstance call above failed, msdia*.dll is not registered.
// Try loading the DLL corresponding to the #included DIA SDK.
#if !defined(_MSC_VER)
  return llvm::make_error<PDBError>(pdb_error_code::dia_failed_loading);
#else
  const wchar_t *msdia_dll = L"msdia140.dll";
  HRESULT HR;
  if (FAILED(HR = NoRegCoCreate(
                 msdia_dll, __uuidof(DiaSource), __uuidof(IDiaDataSource),
                 reinterpret_cast<LPVOID *>(DiaDataSource.GetAddressOf()))))
    return ErrorFromHResult(HR, "Calling NoRegCoCreate");
  return Error::success();
#endif
}

DIASession::DIASession(ComPtr<IDiaSession> DiaSession) : Session(DiaSession) {}

Error DIASession::createFromPdb(StringRef Path,
                                std::unique_ptr<IPDBSession> &Session) {
  ComPtr<IDiaDataSource> DiaDataSource;
  ComPtr<IDiaSession> DiaSession;

  // We assume that CoInitializeEx has already been called by the executable.
  if (auto E = LoadDIA(DiaDataSource))
    return E;

  llvm::SmallVector<UTF16, 128> Path16;
  if (!llvm::convertUTF8ToUTF16String(Path, Path16))
    return make_error<PDBError>(pdb_error_code::invalid_utf8_path, Path);

  const wchar_t *Path16Str = reinterpret_cast<const wchar_t *>(Path16.data());
  HRESULT HR;
  if (FAILED(HR = DiaDataSource->loadDataFromPdb(Path16Str))) {
    return ErrorFromHResult(HR, "Calling loadDataFromPdb {0}", Path);
  }

  if (FAILED(HR = DiaDataSource->openSession(DiaSession.GetAddressOf())))
    return ErrorFromHResult(HR, "Calling openSession");

  Session.reset(new DIASession(DiaSession));
  return Error::success();
}

Error DIASession::createFromExe(StringRef Path,
                                std::unique_ptr<IPDBSession> &Session) {
  ComPtr<IDiaDataSource> DiaDataSource;
  ComPtr<IDiaSession> DiaSession;

  // We assume that CoInitializeEx has already been called by the executable.
  if (auto EC = LoadDIA(DiaDataSource))
    return EC;

  llvm::SmallVector<UTF16, 128> Path16;
  if (!llvm::convertUTF8ToUTF16String(Path, Path16))
    return make_error<PDBError>(pdb_error_code::invalid_utf8_path, Path);

  const wchar_t *Path16Str = reinterpret_cast<const wchar_t *>(Path16.data());
  HRESULT HR;
  if (FAILED(HR = DiaDataSource->loadDataForExe(Path16Str, nullptr, nullptr)))
    return ErrorFromHResult(HR, "Calling loadDataForExe");

  if (FAILED(HR = DiaDataSource->openSession(DiaSession.GetAddressOf())))
    return ErrorFromHResult(HR, "Calling openSession");

  Session.reset(new DIASession(DiaSession));
  return Error::success();
}

uint64_t DIASession::getLoadAddress() const {
  uint64_t LoadAddress;
  bool success = (S_OK == Session->get_loadAddress(&LoadAddress));
  return (success) ? LoadAddress : 0;
}

bool DIASession::setLoadAddress(uint64_t Address) {
  return (S_OK == Session->put_loadAddress(Address));
}

std::unique_ptr<PDBSymbolExe> DIASession::getGlobalScope() {
  ComPtr<IDiaSymbol> GlobalScope;
  if (S_OK != Session->get_globalScope(GlobalScope.GetAddressOf()))
    return nullptr;

  auto RawSymbol = std::make_unique<DIARawSymbol>(*this, GlobalScope);
  auto PdbSymbol(PDBSymbol::create(*this, std::move(RawSymbol)));
  std::unique_ptr<PDBSymbolExe> ExeSymbol(
      static_cast<PDBSymbolExe *>(PdbSymbol.release()));
  return ExeSymbol;
}

bool DIASession::addressForVA(uint64_t VA, uint32_t &Section,
                              uint32_t &Offset) const {
  DWORD ArgSection, ArgOffset = 0;
  if (S_OK == Session->addressForVA(VA, &ArgSection, &ArgOffset)) {
    Section = static_cast<uint32_t>(ArgSection);
    Offset = static_cast<uint32_t>(ArgOffset);
    return true;
  }
  return false;
}

bool DIASession::addressForRVA(uint32_t RVA, uint32_t &Section,
                               uint32_t &Offset) const {
  DWORD ArgSection, ArgOffset = 0;
  if (S_OK == Session->addressForRVA(RVA, &ArgSection, &ArgOffset)) {
    Section = static_cast<uint32_t>(ArgSection);
    Offset = static_cast<uint32_t>(ArgOffset);
    return true;
  }
  return false;
}

std::unique_ptr<PDBSymbol>
DIASession::getSymbolById(SymIndexId SymbolId) const {
  ComPtr<IDiaSymbol> LocatedSymbol;
  if (S_OK != Session->symbolById(SymbolId, LocatedSymbol.GetAddressOf()))
    return nullptr;

  auto RawSymbol = std::make_unique<DIARawSymbol>(*this, LocatedSymbol);
  return PDBSymbol::create(*this, std::move(RawSymbol));
}

std::unique_ptr<PDBSymbol> DIASession::findSymbolByAddress(uint64_t Address,
                                                           PDB_SymType Type) {
  enum SymTagEnum EnumVal = static_cast<enum SymTagEnum>(Type);

  ComPtr<IDiaSymbol> Symbol;
  if (S_OK !=
      Session->findSymbolByVA(Address, EnumVal, Symbol.GetAddressOf())) {
    ULONGLONG LoadAddr = 0;
    if (S_OK != Session->get_loadAddress(&LoadAddr))
      return nullptr;
    DWORD RVA = static_cast<DWORD>(Address - LoadAddr);
    if (S_OK != Session->findSymbolByRVA(RVA, EnumVal, Symbol.GetAddressOf()))
      return nullptr;
  }
  auto RawSymbol = std::make_unique<DIARawSymbol>(*this, Symbol);
  return PDBSymbol::create(*this, std::move(RawSymbol));
}

std::unique_ptr<PDBSymbol> DIASession::findSymbolByRVA(uint32_t RVA,
                                                       PDB_SymType Type) {
  enum SymTagEnum EnumVal = static_cast<enum SymTagEnum>(Type);

  ComPtr<IDiaSymbol> Symbol;
  if (S_OK != Session->findSymbolByRVA(RVA, EnumVal, Symbol.GetAddressOf()))
    return nullptr;

  auto RawSymbol = std::make_unique<DIARawSymbol>(*this, Symbol);
  return PDBSymbol::create(*this, std::move(RawSymbol));
}

std::unique_ptr<PDBSymbol>
DIASession::findSymbolBySectOffset(uint32_t Sect, uint32_t Offset,
                                   PDB_SymType Type) {
  enum SymTagEnum EnumVal = static_cast<enum SymTagEnum>(Type);

  ComPtr<IDiaSymbol> Symbol;
  if (S_OK !=
      Session->findSymbolByAddr(Sect, Offset, EnumVal, Symbol.GetAddressOf()))
    return nullptr;

  auto RawSymbol = std::make_unique<DIARawSymbol>(*this, Symbol);
  return PDBSymbol::create(*this, std::move(RawSymbol));
}

std::unique_ptr<IPDBEnumLineNumbers>
DIASession::findLineNumbers(const PDBSymbolCompiland &Compiland,
                            const IPDBSourceFile &File) const {
  const DIARawSymbol &RawCompiland =
      static_cast<const DIARawSymbol &>(Compiland.getRawSymbol());
  const DIASourceFile &RawFile = static_cast<const DIASourceFile &>(File);

  ComPtr<IDiaEnumLineNumbers> LineNumbers;
  if (S_OK != Session->findLines(RawCompiland.getDiaSymbol().Get(),
                                 RawFile.getDiaFile().Get(), &LineNumbers))
    return nullptr;

  return std::make_unique<DIAEnumLineNumbers>(LineNumbers);
}

std::unique_ptr<IPDBEnumLineNumbers>
DIASession::findLineNumbersByAddress(uint64_t Address, uint32_t Length) const {
  ComPtr<IDiaEnumLineNumbers> LineNumbers;
  if (S_OK !=
      Session->findLinesByVA(Address, Length, LineNumbers.GetAddressOf())) {
    ULONGLONG LoadAddr = 0;
    if (S_OK != Session->get_loadAddress(&LoadAddr))
      return nullptr;
    DWORD RVA = static_cast<DWORD>(Address - LoadAddr);
    if (S_OK !=
        Session->findLinesByRVA(RVA, Length, LineNumbers.GetAddressOf()))
      return nullptr;
  }
  return std::make_unique<DIAEnumLineNumbers>(LineNumbers);
}

std::unique_ptr<IPDBEnumLineNumbers>
DIASession::findLineNumbersByRVA(uint32_t RVA, uint32_t Length) const {
  ComPtr<IDiaEnumLineNumbers> LineNumbers;
  if (S_OK != Session->findLinesByRVA(RVA, Length, LineNumbers.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIAEnumLineNumbers>(LineNumbers);
}

std::unique_ptr<IPDBEnumLineNumbers>
DIASession::findLineNumbersBySectOffset(uint32_t Section, uint32_t Offset,
                                        uint32_t Length) const {
  ComPtr<IDiaEnumLineNumbers> LineNumbers;
  if (S_OK != Session->findLinesByAddr(Section, Offset, Length,
                                       LineNumbers.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIAEnumLineNumbers>(LineNumbers);
}

std::unique_ptr<IPDBEnumSourceFiles>
DIASession::findSourceFiles(const PDBSymbolCompiland *Compiland,
                            llvm::StringRef Pattern,
                            PDB_NameSearchFlags Flags) const {
  IDiaSymbol *DiaCompiland = nullptr;
  _bstr_t Utf16Pattern;
  if (!Pattern.empty())
    Utf16Pattern = _bstr_t(Pattern.data());

  if (Compiland)
    DiaCompiland = static_cast<const DIARawSymbol &>(Compiland->getRawSymbol())
                       .getDiaSymbol()
                       .Get();

  Flags = static_cast<PDB_NameSearchFlags>(
      Flags | PDB_NameSearchFlags::NS_FileNameExtMatch);
  ComPtr<IDiaEnumSourceFiles> SourceFiles;
  if (S_OK != Session->findFile(DiaCompiland, Utf16Pattern.GetBSTR(), Flags,
                                SourceFiles.GetAddressOf()))
    return nullptr;
  return std::make_unique<DIAEnumSourceFiles>(*this, SourceFiles);
}

std::unique_ptr<IPDBSourceFile>
DIASession::findOneSourceFile(const PDBSymbolCompiland *Compiland,
                              llvm::StringRef Pattern,
                              PDB_NameSearchFlags Flags) const {
  auto SourceFiles = findSourceFiles(Compiland, Pattern, Flags);
  if (!SourceFiles || SourceFiles->getChildCount() == 0)
    return nullptr;
  return SourceFiles->getNext();
}

std::unique_ptr<IPDBEnumChildren<PDBSymbolCompiland>>
DIASession::findCompilandsForSourceFile(llvm::StringRef Pattern,
                                        PDB_NameSearchFlags Flags) const {
  auto File = findOneSourceFile(nullptr, Pattern, Flags);
  if (!File)
    return nullptr;
  return File->getCompilands();
}

std::unique_ptr<PDBSymbolCompiland>
DIASession::findOneCompilandForSourceFile(llvm::StringRef Pattern,
                                          PDB_NameSearchFlags Flags) const {
  auto Compilands = findCompilandsForSourceFile(Pattern, Flags);
  if (!Compilands || Compilands->getChildCount() == 0)
    return nullptr;
  return Compilands->getNext();
}

std::unique_ptr<IPDBEnumSourceFiles> DIASession::getAllSourceFiles() const {
  ComPtr<IDiaEnumSourceFiles> Files;
  if (S_OK != Session->findFile(nullptr, nullptr, nsNone, Files.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIAEnumSourceFiles>(*this, Files);
}

std::unique_ptr<IPDBEnumSourceFiles> DIASession::getSourceFilesForCompiland(
    const PDBSymbolCompiland &Compiland) const {
  ComPtr<IDiaEnumSourceFiles> Files;

  const DIARawSymbol &RawSymbol =
      static_cast<const DIARawSymbol &>(Compiland.getRawSymbol());
  if (S_OK != Session->findFile(RawSymbol.getDiaSymbol().Get(), nullptr, nsNone,
                                Files.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIAEnumSourceFiles>(*this, Files);
}

std::unique_ptr<IPDBSourceFile>
DIASession::getSourceFileById(uint32_t FileId) const {
  ComPtr<IDiaSourceFile> LocatedFile;
  if (S_OK != Session->findFileById(FileId, LocatedFile.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIASourceFile>(*this, LocatedFile);
}

std::unique_ptr<IPDBEnumDataStreams> DIASession::getDebugStreams() const {
  ComPtr<IDiaEnumDebugStreams> DiaEnumerator;
  if (S_OK != Session->getEnumDebugStreams(DiaEnumerator.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIAEnumDebugStreams>(DiaEnumerator);
}

std::unique_ptr<IPDBEnumTables> DIASession::getEnumTables() const {
  ComPtr<IDiaEnumTables> DiaEnumerator;
  if (S_OK != Session->getEnumTables(DiaEnumerator.GetAddressOf()))
    return nullptr;

  return std::make_unique<DIAEnumTables>(DiaEnumerator);
}

template <class T> static ComPtr<T> getTableEnumerator(IDiaSession &Session) {
  ComPtr<T> Enumerator;
  ComPtr<IDiaEnumTables> ET;
  ComPtr<IDiaTable> Table;
  ULONG Count = 0;

  if (Session.getEnumTables(ET.GetAddressOf()) != S_OK)
    return nullptr;

  while (ET->Next(1, Table.GetAddressOf(), &Count) == S_OK && Count == 1) {
    // There is only one table that matches the given iid
    if (S_OK ==
        Table->QueryInterface(
            __uuidof(T), reinterpret_cast<void **>(Enumerator.GetAddressOf())))
      break;
    Table.Reset();
  }
  return Enumerator;
}
std::unique_ptr<IPDBEnumInjectedSources>
DIASession::getInjectedSources() const {
  ComPtr<IDiaEnumInjectedSources> Files =
      getTableEnumerator<IDiaEnumInjectedSources>(*Session.Get());
  if (!Files)
    return nullptr;

  return std::make_unique<DIAEnumInjectedSources>(Files);
}

std::unique_ptr<IPDBEnumSectionContribs>
DIASession::getSectionContribs() const {
  ComPtr<IDiaEnumSectionContribs> Sections =
      getTableEnumerator<IDiaEnumSectionContribs>(*Session.Get());
  if (!Sections)
    return nullptr;

  return std::make_unique<DIAEnumSectionContribs>(*this, Sections);
}

std::unique_ptr<IPDBEnumFrameData> DIASession::getFrameData() const {
  ComPtr<IDiaEnumFrameData> FD =
      getTableEnumerator<IDiaEnumFrameData>(*Session.Get());
  if (!FD)
    return nullptr;

  return std::make_unique<DIAEnumFrameData>(FD);
}
