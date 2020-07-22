//===- DIASourceFile.h - DIA implementation of IPDBSourceFile ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIASOURCEFILE_H
#define LLVM_DEBUGINFO_PDB_DIA_DIASOURCEFILE_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBSourceFile.h"

namespace llvm {
namespace pdb {
class DIASession;

class DIASourceFile : public IPDBSourceFile {
public:
  explicit DIASourceFile(const DIASession &Session,
                         IDiaSourceFilePtr DiaSourceFile);

  std::string getFileName() const override;
  uint32_t getUniqueId() const override;
  std::string getChecksum() const override;
  PDB_Checksum getChecksumType() const override;
  std::unique_ptr<IPDBEnumChildren<PDBSymbolCompiland>>
  getCompilands() const override;

  IDiaSourceFilePtr getDiaFile() const { return SourceFile; }

private:
  const DIASession &Session;
  IDiaSourceFilePtr SourceFile;
};
} // namespace pdb
} // namespace llvm

#endif
