//===- DIASupport.h - Common header includes for DIA ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Common defines and header includes for all LLVMDebugInfoPDBDIA.  The
// definitions here configure the necessary #defines and include system headers
// in the proper order for using DIA.
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIASUPPORT_H
#define LLVM_DEBUGINFO_PDB_DIA_DIASUPPORT_H

#ifdef __MINGW32__
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#endif

// Require at least Vista
#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define WINVER _WIN32_WINNT_VISTA
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdio>
#include <comdef.h>
#include <windows.h>

// DIA headers must come after windows headers.
#include <cvconst.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif
#include <dia2.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <diacreate.h>

_COM_SMARTPTR_TYPEDEF(IDiaTable, __uuidof(IDiaTable));
_COM_SMARTPTR_TYPEDEF(IDiaEnumDebugStreamData,
                      __uuidof(IDiaEnumDebugStreamData));
_COM_SMARTPTR_TYPEDEF(IDiaEnumDebugStreams, __uuidof(IDiaEnumDebugStreams));
_COM_SMARTPTR_TYPEDEF(IDiaEnumFrameData, __uuidof(IDiaEnumFrameData));
_COM_SMARTPTR_TYPEDEF(IDiaFrameData, __uuidof(IDiaFrameData));
_COM_SMARTPTR_TYPEDEF(IDiaEnumInjectedSources,
                      __uuidof(IDiaEnumInjectedSources));
_COM_SMARTPTR_TYPEDEF(IDiaEnumLineNumbers, __uuidof(IDiaEnumLineNumbers));
_COM_SMARTPTR_TYPEDEF(IDiaLineNumber, __uuidof(IDiaLineNumber));
_COM_SMARTPTR_TYPEDEF(IDiaEnumSectionContribs,
                      __uuidof(IDiaEnumSectionContribs));
_COM_SMARTPTR_TYPEDEF(IDiaSectionContrib, __uuidof(IDiaSectionContrib));
_COM_SMARTPTR_TYPEDEF(IDiaEnumSourceFiles, __uuidof(IDiaEnumSourceFiles));
_COM_SMARTPTR_TYPEDEF(IDiaSourceFile, __uuidof(IDiaSourceFile));
_COM_SMARTPTR_TYPEDEF(IDiaEnumSymbols, __uuidof(IDiaEnumSymbols));
_COM_SMARTPTR_TYPEDEF(IDiaSymbol, __uuidof(IDiaSymbol));
_COM_SMARTPTR_TYPEDEF(IDiaEnumTables, __uuidof(IDiaEnumTables));
_COM_SMARTPTR_TYPEDEF(IDiaInjectedSource, __uuidof(IDiaInjectedSource));
_COM_SMARTPTR_TYPEDEF(IDiaSession, __uuidof(IDiaSession));
_COM_SMARTPTR_TYPEDEF(IDiaDataSource, __uuidof(IDiaDataSource));

#endif // LLVM_DEBUGINFO_PDB_DIA_DIASUPPORT_H
