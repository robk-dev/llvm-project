//===-- GNUstepObjCRuntime.cpp --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCRuntime.h"
#include "GNUstepObjCClassDescriptor.h"
#include "GNUstepThreadPlanStepThroughObjCTrampoline.h"

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include "lldb/Core/Address.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Expression/DiagnosticManager.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Expression/UtilityFunction.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadPlanRunToAddress.h"
#include "lldb/Target/ThreadPlanStepOut.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/RegularExpression.h"
#include "lldb/Utility/StructuredData.h"
#include "lldb/ValueObject/ValueObject.h"

#include <limits>

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE(GNUstepObjCRuntime)

char GNUstepObjCRuntime::ID = 0;

void GNUstepObjCRuntime::Initialize() {
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(), "GNUstep Objective-C Language Runtime - libobjc2",
      CreateInstance);
}

void GNUstepObjCRuntime::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

static bool CanModuleBeGNUstepObjCLibrary(const ModuleSP &module_sp,
                                          const llvm::Triple &TT) {
  if (!module_sp)
    return false;
  const FileSpec &module_file_spec = module_sp->GetFileSpec();
  if (!module_file_spec)
    return false;
  llvm::StringRef filename = module_file_spec.GetFilename();
  if (TT.isOSBinFormatELF())
    return filename.starts_with("libobjc.so");
  if (TT.isOSWindows())
    return filename == "objc.dll";
  return false;
}

static bool ScanForGNUstepObjCLibraryCandidate(const ModuleList &modules,
                                               const llvm::Triple &TT) {
  std::lock_guard<std::recursive_mutex> guard(modules.GetMutex());
  size_t num_modules = modules.GetSize();
  for (size_t i = 0; i < num_modules; i++) {
    auto mod = modules.GetModuleAtIndex(i);
    if (CanModuleBeGNUstepObjCLibrary(mod, TT))
      return true;
  }
  return false;
}

LanguageRuntime *GNUstepObjCRuntime::CreateInstance(Process *process,
                                                    LanguageType language) {
  if (language != eLanguageTypeObjC)
    return nullptr;
  if (!process)
    return nullptr;

  Target &target = process->GetTarget();
  const llvm::Triple &TT = target.GetArchitecture().GetTriple();
  if (TT.getVendor() == llvm::Triple::VendorType::Apple)
    return nullptr;

  const ModuleList &images = target.GetImages();
  if (!ScanForGNUstepObjCLibraryCandidate(images, TT))
    return nullptr;

  if (TT.isOSBinFormatELF()) {
    SymbolContextList eh_pers;
    RegularExpression regex("__gnustep_objc[x]*_personality_v[0-9]+");
    images.FindSymbolsMatchingRegExAndType(regex, eSymbolTypeCode, eh_pers);
    if (eh_pers.GetSize() == 0)
      return nullptr;
  } else if (TT.isOSWindows()) {
    SymbolContextList objc_mandatory;
    images.FindSymbolsWithNameAndType(ConstString("__objc_load"),
                                      eSymbolTypeCode, objc_mandatory);
    if (objc_mandatory.GetSize() == 0)
      return nullptr;
  }

  return new GNUstepObjCRuntime(process);
}

GNUstepObjCRuntime::~GNUstepObjCRuntime() = default;

GNUstepObjCRuntime::GNUstepObjCRuntime(Process *process)
    : ObjCLanguageRuntime(process), m_objc_module_sp(nullptr),
      m_tagged_pointer_vendor_up(
          std::make_unique<GNUstepTaggedPointerVendor>(*process)) {
  ReadObjCLibraryIfNeeded(process->GetTarget().GetImages());
}

FunctionCaller *
GNUstepObjCRuntime::GetObjectDescriptionCaller(ExecutionContext &exe_ctx) {
  // gnustep-base's _NSPrintForDebugger (Source/NSDebug.m) is exactly:
  //
  //   if (object && [object respondsToSelector: @selector(description)])
  //     return [[object description] UTF8String];
  //   return NULL;
  //
  // Every runtime function it needs is OBJC_PUBLIC in libobjc2, so rather
  // than depending on Foundation exporting that hook - it does from
  // libgnustep-base.so but not from the MSVC gnustep-base DLL, which is why
  // this used to need a shim on Windows - reproduce it here. As a bonus this
  // makes `po` work against a bare libobjc2 with no Foundation at all, for
  // any object implementing -description.
  //
  // object_getClass() rather than a raw isa read, because it also resolves
  // tagged pointers (classForObject) and skips libobjc2's hidden classes.
  static const char *g_description_name = "$__lldb_gnustep_object_description";
  static const char *g_description_code = R"(
extern "C" {
void *object_getClass(void *object);
void *sel_registerName(const char *name);
signed char class_respondsToSelector(void *cls, void *sel);
void *objc_msg_lookup(void *receiver, void *selector);
}

extern "C" const char *$__lldb_gnustep_object_description(void *object) {
  if (!object)
    return 0;
  void *description_sel = sel_registerName("description");
  if (!class_respondsToSelector(object_getClass(object), description_sel))
    return 0;
  void *(*description_imp)(void *, void *) =
      (void *(*)(void *, void *))objc_msg_lookup(object, description_sel);
  if (!description_imp)
    return 0;
  void *description = description_imp(object, description_sel);
  if (!description)
    return 0;
  void *utf8_sel = sel_registerName("UTF8String");
  if (!class_respondsToSelector(object_getClass(description), utf8_sel))
    return 0;
  const char *(*utf8_imp)(void *, void *) =
      (const char *(*)(void *, void *))objc_msg_lookup(description, utf8_sel);
  if (!utf8_imp)
    return 0;
  return utf8_imp(description, utf8_sel);
}
)";

  std::lock_guard<std::mutex> guard(m_description_mutex);
  if (m_description_caller)
    return m_description_caller;
  // A wrapper that failed to compile will fail again - the runtime does not
  // provide something it calls - so don't pay for that on every `po`.
  // Anything that fails later may be transient and is retried.
  if (m_description_failed)
    return nullptr;

  Log *log = GetLog(LLDBLog::Expressions);

  if (!m_description_utility_up) {
    auto utility_fn_or_error = exe_ctx.GetTargetRef().CreateUtilityFunction(
        g_description_code, g_description_name, eLanguageTypeC, exe_ctx);
    if (!utility_fn_or_error) {
      LLDB_LOG_ERROR(log, utility_fn_or_error.takeError(),
                     "failed to build object description utility: {0}");
      m_description_failed = true;
      return nullptr;
    }
    m_description_utility_up = std::move(*utility_fn_or_error);
  }

  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(GetTargetRef());
  if (!scratch_ts_sp)
    return nullptr;

  Value void_ptr_value;
  void_ptr_value.SetValueType(Value::ValueType::Scalar);
  void_ptr_value.SetCompilerType(
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType());
  ValueList args;
  args.PushValue(void_ptr_value);

  Status error;
  m_description_caller = m_description_utility_up->MakeFunctionCaller(
      scratch_ts_sp->GetCStringType(true), args, exe_ctx.GetThreadSP(), error);
  if (error.Fail()) {
    LLDB_LOG(log, "failed to make object description caller: {0}",
             error.AsCString());
    m_description_caller = nullptr;
    return nullptr;
  }
  return m_description_caller;
}

llvm::Error GNUstepObjCRuntime::GetObjectDescription(Stream &str,
                                                     ValueObject &valobj) {
  CompilerType compiler_type(valobj.GetCompilerType());
  bool is_signed;
  // ObjC objects can only be pointers (or numbers that actually represent
  // pointers but haven't been typecast).
  if (!compiler_type.IsIntegerType(is_signed) && !compiler_type.IsPointerType())
    return llvm::createStringError("not a pointer type");

  Value val;
  if (!valobj.ResolveValue(val.GetScalar()))
    return llvm::createStringError("pointer value could not be resolved");

  // Value objects may not have a process in their ExecutionContextRef. But
  // we need one in the context we pass down to eventually call description.
  ExecutionContext exe_ctx;
  if (valobj.GetProcessSP()) {
    exe_ctx = ExecutionContext(valobj.GetExecutionContextRef());
  } else {
    exe_ctx.SetContext(valobj.GetTargetSP(), true);
    if (!exe_ctx.HasProcessScope())
      return llvm::createStringError("no process");
  }
  return GetObjectDescription(str, val, exe_ctx.GetBestExecutionContextScope());
}

llvm::Error
GNUstepObjCRuntime::GetObjectDescription(Stream &strm, Value &value,
                                         ExecutionContextScope *exe_scope) {
  ExecutionContext exe_ctx;
  exe_scope->CalculateExecutionContext(exe_ctx);
  Process *process = exe_ctx.GetProcessPtr();
  if (!process)
    return llvm::createStringError("no process");

  Target *target = exe_ctx.GetTargetPtr();
  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(*target);
  if (!scratch_ts_sp)
    return llvm::createStringError("no scratch type system");

  // The call thunk is compiled as plain C (no ObjC machinery needed in the
  // expression parser), so pass the object as `void *` and read back a
  // `const char *`.
  CompilerType void_ptr_type =
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  value.SetCompilerType(void_ptr_type);
  // FunctionCaller writes the argument at the width of its Scalar, which for
  // an address held in an integer is the integer's, not the pointer's.
  const uint64_t object_addr = value.GetScalar().ULongLong();
  if (process->GetAddressByteSize() == 4) {
    if (object_addr > std::numeric_limits<uint32_t>::max())
      return llvm::createStringError("value is not an address");
    value.GetScalar() = Scalar(static_cast<uint32_t>(object_addr));
  } else {
    value.GetScalar() = Scalar(object_addr);
  }

  ValueList arg_value_list;
  arg_value_list.PushValue(value);

  CompilerType return_compiler_type = scratch_ts_sp->GetCStringType(true);
  Value ret;
  ret.SetCompilerType(return_compiler_type);

  if (!exe_ctx.GetFramePtr()) {
    Thread *thread = exe_ctx.GetThreadPtr();
    if (thread == nullptr) {
      exe_ctx.SetThreadSP(process->GetThreadList().GetSelectedThread());
      thread = exe_ctx.GetThreadPtr();
    }
    if (thread)
      exe_ctx.SetFrameSP(thread->GetSelectedFrame(DoNoSelectMostRelevantFrame));
  }

  DiagnosticManager diagnostics;
  lldb::addr_t wrapper_struct_addr = LLDB_INVALID_ADDRESS;

  // Building the caller needs a thread, so this has to follow the frame
  // selection above. MakeFunctionCaller already compiled the wrapper into
  // the inferior, so only a fresh argument struct is needed per call.
  FunctionCaller *caller = GetObjectDescriptionCaller(exe_ctx);
  if (!caller)
    return llvm::createStringError(
        "could not build the object description function");

  if (!caller->WriteFunctionArguments(exe_ctx, wrapper_struct_addr,
                                      arg_value_list, diagnostics)) {
    // The argument block is allocated before the arguments are written, so
    // it may exist even though writing them failed.
    if (wrapper_struct_addr != LLDB_INVALID_ADDRESS)
      caller->DeallocateFunctionResults(exe_ctx, wrapper_struct_addr);
    return llvm::createStringError(
        "could not write the object description arguments");
  }

  EvaluateExpressionOptions options;
  options.SetUnwindOnError(true);
  options.SetTryAllThreads(true);
  options.SetStopOthers(true);
  options.SetIgnoreBreakpoints(true);
  options.SetTimeout(process->GetUtilityExpressionTimeout());
  options.SetIsForUtilityExpr(true);

  ExpressionResults results = caller->ExecuteFunction(
      exe_ctx, &wrapper_struct_addr, options, diagnostics, ret);
  caller->DeallocateFunctionResults(exe_ctx, wrapper_struct_addr);
  if (results != eExpressionCompleted)
    return llvm::createStringError(
        "could not evaluate the object description in the inferior");

  addr_t result_ptr = ret.GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
  if (result_ptr == 0 || result_ptr == LLDB_INVALID_ADDRESS)
    return llvm::createStringError("object returned no description");

  // -description returns inferior memory, so a string that never terminates
  // would otherwise be read until the host runs out of it. No real
  // description approaches this.
  static constexpr size_t g_max_description_length = 1024 * 1024;

  char buf[512];
  size_t cstr_len = 0;
  size_t full_buffer_len = sizeof(buf) - 1;
  size_t curr_len = full_buffer_len;
  while (curr_len == full_buffer_len && cstr_len < g_max_description_length) {
    Status error;
    curr_len = process->ReadCStringFromMemory(result_ptr + cstr_len, buf,
                                              sizeof(buf), error);
    strm.Write(buf, curr_len);
    cstr_len += curr_len;
  }
  if (cstr_len > 0)
    return llvm::Error::success();
  return llvm::createStringError("empty object description");
}

bool GNUstepObjCRuntime::CouldHaveDynamicValue(ValueObject &in_value) {
  static constexpr bool check_cxx = false;
  static constexpr bool check_objc = true;
  return in_value.GetCompilerType().IsPossibleDynamicType(nullptr, check_cxx,
                                                          check_objc);
}

bool GNUstepObjCRuntime::GetDynamicTypeAndAddress(
    ValueObject &in_value, DynamicValueType use_dynamic,
    TypeAndOrName &class_type_or_name, Address &address,
    Value::ValueType &value_type, llvm::ArrayRef<uint8_t> &local_buffer) {
  class_type_or_name.Clear();
  value_type = Value::ValueType::Scalar;

  if (!CouldHaveDynamicValue(in_value))
    return false;

  ClassDescriptorSP objc_class_sp(GetNonKVOClassDescriptor(in_value));
  if (!objc_class_sp)
    return false;

  ConstString class_name(objc_class_sp->GetClassName());
  if (!class_name)
    return false;

  const addr_t object_ptr = in_value.GetPointerValue().address;
  address.SetRawAddress(object_ptr);
  class_type_or_name.SetName(class_name);

  // Upgrade the bare class name to a real type when the inferior's debug
  // info defines the class. LookupInCompleteClassCache keys on an
  // eSymbolTypeObjCClass symbol named exactly after the class, which the
  // Apple ABI emits but the gnustep-2.x ABI does not (its class symbol is
  // "._OBJC_CLASS_<name>"), so on a cache miss query the debug info directly.
  TypeSP type_sp(objc_class_sp->GetType());
  if (!type_sp) {
    type_sp = LookupInCompleteClassCache(class_name);
    if (!type_sp)
      type_sp = LookupClassTypeInDebugInfo(class_name);
    if (type_sp)
      objc_class_sp->SetType(type_sp);
  }
  if (type_sp)
    class_type_or_name.SetTypeSP(type_sp);

  return !class_type_or_name.IsEmpty();
}

lldb::TypeSP
GNUstepObjCRuntime::LookupClassTypeInDebugInfo(ConstString class_name) {
  TypeQuery query(class_name.GetStringRef(), TypeQueryOptions::e_exact_match);
  TypeResults results;
  GetTargetRef().GetImages().FindTypes(nullptr, query, results);
  for (const TypeSP &type_sp : results.GetTypeMap().Types()) {
    if (type_sp && TypeSystemClang::IsObjCObjectOrInterfaceType(
                       type_sp->GetForwardCompilerType()))
      return type_sp;
  }
  return TypeSP();
}

TypeAndOrName
GNUstepObjCRuntime::FixUpDynamicType(const TypeAndOrName &type_and_or_name,
                                     ValueObject &static_value) {
  CompilerType static_type(static_value.GetCompilerType());
  Flags static_type_flags(static_type.GetTypeInfo());

  TypeAndOrName ret(type_and_or_name);
  if (type_and_or_name.HasType()) {
    // The type will always be the type of the dynamic object.  If our parent's
    // type was a pointer, then our type should be a pointer to the type of the
    // dynamic object.  If a reference, then the original type should be
    // okay...
    CompilerType orig_type = type_and_or_name.GetCompilerType();
    CompilerType corrected_type = orig_type;
    if (static_type_flags.AllSet(eTypeIsPointer))
      corrected_type = orig_type.GetPointerType();
    ret.SetCompilerType(corrected_type);
  } else {
    // If we are here we need to adjust our dynamic type name to include the
    // correct & or * symbol
    std::string corrected_name(type_and_or_name.GetName().GetCString());
    if (static_type_flags.AllSet(eTypeIsPointer))
      corrected_name.append(" *");
    // the parent type should be a correctly pointer'ed or referenc'ed type
    ret.SetCompilerType(static_type);
    ret.SetName(corrected_name.c_str());
  }
  return ret;
}

BreakpointResolverSP
GNUstepObjCRuntime::CreateExceptionResolver(const BreakpointSP &bkpt,
                                            bool catch_bp, bool throw_bp) {
  BreakpointResolverSP resolver_sp;

  if (throw_bp)
    resolver_sp = std::make_shared<BreakpointResolverName>(
        bkpt, "objc_exception_throw", eFunctionNameTypeBase,
        eLanguageTypeUnknown, Breakpoint::Exact, 0,
        /*offset_is_insn_count = */ false, eLazyBoolNo);

  return resolver_sp;
}

llvm::Expected<std::unique_ptr<UtilityFunction>>
GNUstepObjCRuntime::CreateObjectChecker(std::string name,
                                        ExecutionContext &exe_ctx) {
  // TODO: This function is supposed to check whether an ObjC selector is
  // present for an object. Might be implemented similar as in the Apple V2
  // runtime.
  const char *function_template = R"(
    extern "C" void
    %s(void *$__lldb_arg_obj, void *$__lldb_arg_selector) {}
  )";

  char empty_function_code[2048];
  int len = ::snprintf(empty_function_code, sizeof(empty_function_code),
                       function_template, name.c_str());

  assert(len < (int)sizeof(empty_function_code));
  UNUSED_IF_ASSERT_DISABLED(len);

  return GetTargetRef().CreateUtilityFunction(empty_function_code, name,
                                              eLanguageTypeC, exe_ctx);
}

ThreadPlanSP
GNUstepObjCRuntime::GetStepThroughTrampolinePlan(Thread &thread,
                                                 bool stop_others) {
  // Only act when stopped at the first instruction of a known libobjc2
  // dispatch entry point (where the argument registers still hold the
  // receiver and selector). Match by resolving each entry point's address by
  // name rather than by the symbol at the PC: libobjc2's hand-written
  // assembly places local labels (e.g. __objc_block_trampoline_end_sret) at
  // the same address as objc_msgSend, so the symbol found at an address is
  // not reliably the dispatch symbol.
  Process *process = thread.GetProcess().get();
  if (!process)
    return {};
  const addr_t pc = thread.GetRegisterContext()->GetPC();
  Target &target = GetTargetRef();

  const DispatchEntryPoint *entry = FindDispatchEntryPoint(pc);
  if (!entry)
    return {};
  const bool is_stret = entry->is_stret;
  const bool is_sender = entry->is_sender;

  ABISP abi_sp = process->GetABI();
  if (!abi_sp)
    return {};
  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(target);
  if (!scratch_ts_sp)
    return {};
  CompilerType void_ptr_type =
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();
  Value void_ptr_value;
  void_ptr_value.SetValueType(Value::ValueType::Scalar);
  void_ptr_value.SetCompilerType(void_ptr_type);

  ValueList argument_values;
  argument_values.PushValue(void_ptr_value);
  argument_values.PushValue(void_ptr_value);
  argument_values.PushValue(void_ptr_value);
  if (!abi_sp->GetArgumentValues(thread, argument_values))
    return {};

  // With struct return the sret pointer occupies the first argument slot.
  const uint32_t receiver_idx = is_stret ? 1 : 0;
  const uint32_t sel_idx = is_stret ? 2 : 1;
  addr_t receiver =
      argument_values.GetValueAtIndex(receiver_idx)->GetScalar().ULongLong();
  const addr_t selector =
      argument_values.GetValueAtIndex(sel_idx)->GetScalar().ULongLong();

  if (is_sender) {
    // objc_msg_lookup_sender takes `id *receiver`.
    llvm::Expected<addr_t> pointee = process->ReadPointerFromMemory(receiver);
    if (!pointee) {
      llvm::consumeError(pointee.takeError());
      return {};
    }
    receiver = *pointee;
  }

  // A message to nil does not dispatch anywhere, so there is no
  // implementation to run to. Declining to provide a plan at all would leave
  // the thread stopped on the dispatch function's first instruction: harmless
  // when the runtime carries no line information for its hand-written
  // assembly, but with a runtime built from source it drops the user into
  // objc_msgSend.S. Step back out to the sender instead, which is where a nil
  // send returns to and what the user asked to step over.
  if (receiver == 0 || receiver == LLDB_INVALID_ADDRESS) {
    const bool continue_to_next_branch = true;
    const bool gather_return_value = false;
    auto step_out_sp = std::make_shared<ThreadPlanStepOut>(
        thread, stop_others, eVoteNo, eVoteNoOpinion, /*frame_idx=*/0,
        continue_to_next_branch, gather_return_value);
    // Nothing further should be stepped through on the way out; the enclosing
    // step-in plan decides where to stop once we are back in the sender.
    step_out_sp->ClearShouldStopHereCallbacks();
    return step_out_sp;
  }

  // Consult the method cache before running anything in the inferior.
  // Tagged pointers skip the cache: their ISA is not the object's first word.
  addr_t isa = LLDB_INVALID_ADDRESS;
  if (!(m_tagged_pointer_vendor_up &&
        m_tagged_pointer_vendor_up->IsPossibleTaggedPointer(receiver))) {
    isa = llvm::expectedToOptional(process->ReadPointerFromMemory(receiver))
              .value_or(LLDB_INVALID_ADDRESS);
  }
  if (isa != LLDB_INVALID_ADDRESS) {
    const addr_t cached_imp = LookupInMethodCache(isa, selector);
    if (cached_imp != LLDB_INVALID_ADDRESS) {
      Address imp_addr;
      imp_addr.SetOpcodeLoadAddress(cached_imp, &target);
      return std::make_shared<ThreadPlanRunToAddress>(thread, imp_addr,
                                                      stop_others);
    }
  }

  if (!GetMsgLookupFunctionCaller(thread))
    return {};

  // FunctionCaller writes each argument at the width of its Scalar, and one
  // made from an addr_t is 64 bits, so narrow them to the target's pointers.
  const bool narrow = process->GetAddressByteSize() == 4;
  auto pointer_scalar = [narrow](addr_t addr) {
    return narrow ? Scalar(static_cast<uint32_t>(addr)) : Scalar(addr);
  };
  ValueList lookup_args;
  Value receiver_value = void_ptr_value;
  receiver_value.GetScalar() = pointer_scalar(receiver);
  lookup_args.PushValue(receiver_value);
  Value selector_value = void_ptr_value;
  selector_value.GetScalar() = pointer_scalar(selector);
  lookup_args.PushValue(selector_value);

  return std::make_shared<GNUstepThreadPlanStepThroughObjCTrampoline>(
      thread, *this, lookup_args, isa, selector);
}

const GNUstepObjCRuntime::DispatchEntryPoint *
GNUstepObjCRuntime::FindDispatchEntryPoint(lldb::addr_t pc) {
  if (!m_dispatch_entry_points_resolved) {
    m_dispatch_entry_points_resolved = true;
    // Dispatch entry points exported by libobjc2 (objc_msgSend.S,
    // sendmsg2.c). The `_super` variants are omitted: super sends compile to
    // a lookup plus a direct call, and the direct call steps normally.
    static const struct {
      const char *name;
      bool is_stret;
      bool is_sender;
    } kEntryPoints[] = {
        {"objc_msgSend", false, false},
        {"objc_msgSend_fpret", false, false},
        {"objc_msgSend_stret", true, false},
        {"objc_msg_lookup", false, false},
        {"objc_msg_lookup_sender", false, true},
    };
    Target &target = GetTargetRef();
    for (const auto &ep : kEntryPoints) {
      SymbolContextList sc_list;
      target.GetImages().FindSymbolsWithNameAndType(ConstString(ep.name),
                                                    eSymbolTypeCode, sc_list);
      for (const SymbolContext &sc : sc_list) {
        if (!sc.symbol)
          continue;
        const addr_t addr = sc.symbol->GetLoadAddress(&target);
        if (addr != LLDB_INVALID_ADDRESS) {
          m_dispatch_entry_points.push_back({addr, ep.is_stret, ep.is_sender});
          break;
        }
      }
    }
  }

  for (const DispatchEntryPoint &ep : m_dispatch_entry_points)
    if (ep.address == pc)
      return &ep;
  return nullptr;
}

FunctionCaller *GNUstepObjCRuntime::GetMsgLookupFunctionCaller(Thread &thread) {
  // Build (once) a utility function that resolves a method implementation by
  // calling libobjc2's objc_msg_lookup, and a FunctionCaller to invoke it.
  // This mirrors AppleObjCTrampolineHandler's dispatch-lookup utility and is
  // the JIT path that works from inside a step's PreResume action.
  static const char *g_lookup_name = "$__lldb_gnustep_objc_msg_lookup";
  static const char *g_lookup_code = R"(
extern "C" void *objc_msg_lookup(void *receiver, void *selector);

extern "C" void *$__lldb_gnustep_objc_msg_lookup(void *receiver,
                                                 void *selector) {
  return objc_msg_lookup(receiver, selector);
}
)";

  if (m_msg_lookup_caller)
    return m_msg_lookup_caller;

  ThreadSP thread_sp(thread.shared_from_this());
  ExecutionContext exe_ctx(thread_sp);
  Log *log = GetLog(LLDBLog::Step);

  auto utility_fn_or_error = exe_ctx.GetTargetRef().CreateUtilityFunction(
      g_lookup_code, g_lookup_name, eLanguageTypeC, exe_ctx);
  if (!utility_fn_or_error) {
    LLDB_LOG_ERROR(log, utility_fn_or_error.takeError(),
                   "[GNUstep] failed to build objc_msg_lookup utility: {0}");
    return nullptr;
  }
  m_msg_lookup_utility_up = std::move(*utility_fn_or_error);

  TypeSystemClangSP scratch_ts_sp =
      ScratchTypeSystemClang::GetForTarget(GetTargetRef());
  if (!scratch_ts_sp)
    return nullptr;
  CompilerType void_ptr_type =
      scratch_ts_sp->GetBasicType(eBasicTypeVoid).GetPointerType();

  Value void_ptr_value;
  void_ptr_value.SetValueType(Value::ValueType::Scalar);
  void_ptr_value.SetCompilerType(void_ptr_type);
  ValueList args;
  args.PushValue(void_ptr_value);
  args.PushValue(void_ptr_value);

  Status error;
  m_msg_lookup_caller = m_msg_lookup_utility_up->MakeFunctionCaller(
      void_ptr_type, args, thread_sp, error);
  if (error.Fail()) {
    LLDB_LOG(log, "[GNUstep] failed to make objc_msg_lookup caller: {0}",
             error.AsCString());
    m_msg_lookup_caller = nullptr;
    return nullptr;
  }
  return m_msg_lookup_caller;
}

void GNUstepObjCRuntime::UpdateISAToDescriptorMapIfNeeded() {
  if (!m_process)
    return;
  const uint32_t stop_id = m_process->GetStopID();
  if (!m_isa_map_dirty) {
    m_isa_to_descriptor_stop_id = stop_id;
    return;
  }

  // The gnustep-2.x ABI emits every compiled class as a `._OBJC_CLASS_<name>`
  // data symbol whose address is the class object itself (the ISA of its
  // instances), so the map can be seeded from symbol tables alone - without
  // running any code in the inferior. Classes created dynamically at runtime
  // are handled by the create-on-miss path in GetClassDescriptorFromISA.
  Target &target = GetTargetRef();
  const ModuleList &images = target.GetImages();

  SymbolContextList sc_list;
  RegularExpression regex(llvm::StringRef("^\\._OBJC_CLASS_"));
  images.FindSymbolsMatchingRegExAndType(regex, eSymbolTypeAny, sc_list);

  static constexpr llvm::StringLiteral g_class_prefix("._OBJC_CLASS_");
  for (const SymbolContext &sc : sc_list) {
    if (!sc.symbol)
      continue;
    const addr_t isa = sc.symbol->GetAddress().GetLoadAddress(&target);
    if (isa == 0 || isa == LLDB_INVALID_ADDRESS || ISAIsCached(isa))
      continue;
    llvm::StringRef name = sc.symbol->GetName().GetStringRef();
    name.consume_front(g_class_prefix);
    auto descriptor_sp = std::make_shared<GNUstepObjCClassDescriptor>(
        m_process->shared_from_this(), isa);
    if (descriptor_sp->IsValid())
      AddClass(isa, descriptor_sp, name.str().c_str());
  }

  m_isa_map_dirty = false;
  m_isa_to_descriptor_stop_id = stop_id;
}

ObjCLanguageRuntime::TaggedPointerVendor *
GNUstepObjCRuntime::GetTaggedPointerVendor() {
  return m_tagged_pointer_vendor_up.get();
}

ObjCLanguageRuntime::ClassDescriptorSP
GNUstepObjCRuntime::GetClassDescriptor(ValueObject &in_value) {
  const addr_t ptr = in_value.GetPointerValue().address;
  if (ptr != LLDB_INVALID_ADDRESS && m_tagged_pointer_vendor_up &&
      m_tagged_pointer_vendor_up->IsPossibleTaggedPointer(ptr))
    return m_tagged_pointer_vendor_up->GetClassDescriptor(ptr);
  return ObjCLanguageRuntime::GetClassDescriptor(in_value);
}

ObjCLanguageRuntime::ClassDescriptorSP
GNUstepObjCRuntime::GetClassDescriptorFromISA(ObjCISA isa) {
  if (ClassDescriptorSP descriptor_sp =
          ObjCLanguageRuntime::GetClassDescriptorFromISA(isa))
    return descriptor_sp;

  // The symbol sweep only sees classes with static definitions. Fall back to
  // parsing the class structure directly so classes registered at runtime
  // (e.g. via objc_allocateClassPair) resolve as well.
  if (!m_process || isa == 0 || isa == LLDB_INVALID_ADDRESS)
    return ClassDescriptorSP();
  auto descriptor_sp = std::make_shared<GNUstepObjCClassDescriptor>(
      m_process->shared_from_this(), isa);
  if (!descriptor_sp->IsValid())
    return ClassDescriptorSP();
  AddClass(isa, descriptor_sp, descriptor_sp->GetClassName().GetCString());
  return descriptor_sp;
}

bool GNUstepObjCRuntime::IsModuleObjCLibrary(const ModuleSP &module_sp) {
  const llvm::Triple &TT = GetTargetRef().GetArchitecture().GetTriple();
  return CanModuleBeGNUstepObjCLibrary(module_sp, TT);
}

bool GNUstepObjCRuntime::ReadObjCLibrary(const ModuleSP &module_sp) {
  assert(m_objc_module_sp == nullptr && "Check HasReadObjCLibrary() first");
  m_objc_module_sp = module_sp;

  // Right now we don't use this, but we might want to check for debugger
  // runtime support symbols like 'gdb_object_getClass' in the future.
  return true;
}

StructuredData::ObjectSP
GNUstepObjCRuntime::GetLanguageSpecificData(SymbolContext sc) {
  auto dict_up = std::make_unique<StructuredData::Dictionary>();
  dict_up->AddItem("Objective-C runtime version",
                   std::make_unique<StructuredData::UnsignedInteger>(2));
  return dict_up;
}

void GNUstepObjCRuntime::ModulesDidLoad(const ModuleList &module_list) {
  ReadObjCLibraryIfNeeded(module_list);
  m_isa_map_dirty = true;
}
