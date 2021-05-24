//==- DIAEnumFrameData.cpp ---------------------------------------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/DebugInfo/PDB/DIA/DIAEnumFrameData.h"
#include "llvm/DebugInfo/PDB/DIA/DIAFrameData.h"
#include "llvm/DebugInfo/PDB/DIA/DIASession.h"

using namespace llvm::pdb;

DIAEnumFrameData::DIAEnumFrameData(ComPtr<IDiaEnumFrameData> DiaEnumerator)
    : Enumerator(DiaEnumerator) {}

uint32_t DIAEnumFrameData::getChildCount() const {
  LONG Count = 0;
  return (S_OK == Enumerator->get_Count(&Count)) ? Count : 0;
}

std::unique_ptr<IPDBFrameData>
DIAEnumFrameData::getChildAtIndex(uint32_t Index) const {
  ComPtr<IDiaFrameData> Item;
  if (S_OK != Enumerator->Item(Index, Item.GetAddressOf()))
    return nullptr;

  return std::unique_ptr<IPDBFrameData>(new DIAFrameData(Item));
}

std::unique_ptr<IPDBFrameData> DIAEnumFrameData::getNext() {
  ComPtr<IDiaFrameData> Item;
  ULONG NumFetched = 0;
  if (S_OK != Enumerator->Next(1, Item.GetAddressOf(), &NumFetched))
    return nullptr;

  return std::unique_ptr<IPDBFrameData>(new DIAFrameData(Item));
}

void DIAEnumFrameData::reset() { Enumerator->Reset(); }
