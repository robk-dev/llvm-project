//===-- GNUstepObjCDeclVendor.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H

#include "Plugins/ExpressionParser/Clang/ClangDeclVendor.h"
#include "GNUstepObjCRuntimeIntrospector.h"

namespace lldb_private {

class GNUstepObjCDeclVendor : public ClangDeclVendor {
public:
  GNUstepObjCDeclVendor(TypeSystemClang &ast, 
                        GNUstepObjCRuntimeIntrospector &introspector);
  ~GNUstepObjCDeclVendor() override = default;

  uint32_t FindDecls(ConstString name, bool append, uint32_t max_matches,
                     std::vector<CompilerDecl> &decls) override;

private:
  GNUstepObjCRuntimeIntrospector &m_introspector;
  TypeSystemClang &m_ast;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H
