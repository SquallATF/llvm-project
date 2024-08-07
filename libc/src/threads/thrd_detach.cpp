//===-- Linux implementation of the thrd_detach function ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/threads/thrd_detach.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/thread.h"

#include <threads.h> // For thrd_* type definitions.

namespace LIBC_NAMESPACE_DECL {

static_assert(sizeof(thrd_t) == sizeof(LIBC_NAMESPACE::Thread),
              "Mismatch between thrd_t and internal Thread.");

LLVM_LIBC_FUNCTION(int, thrd_detach, (thrd_t th)) {
  auto *thread = reinterpret_cast<Thread *>(&th);
  thread->detach();
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
