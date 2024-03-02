//===-- Logging.cpp -------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Utility/Logging.h"
#include "lldb/Utility/Log.h"

#include "llvm/ADT/ArrayRef.h"

#include <cstdarg>

using namespace lldb_private;

static constexpr Log::Category g_categories[] = {
  {{"api"}, {"log API calls and return values"}, LIBLLDB_LOG_API},
  {{"ast"}, {"log AST"}, LIBLLDB_LOG_AST},
  {{"break"}, {"log breakpoints"}, LIBLLDB_LOG_BREAKPOINTS},
  {{"commands"}, {"log command argument parsing"}, LIBLLDB_LOG_COMMANDS},
  {{"comm"}, {"log communication activities"}, LIBLLDB_LOG_COMMUNICATION},
  {{"conn"}, {"log connection details"}, LIBLLDB_LOG_CONNECTION},
  {{"demangle"}, {"log mangled names to catch demangler crashes"}, LIBLLDB_LOG_DEMANGLE},
  {{"dyld"}, {"log shared library related activities"}, LIBLLDB_LOG_DYNAMIC_LOADER},
  {{"event"}, {"log broadcaster, listener and event queue activities"}, LIBLLDB_LOG_EVENTS},
  {{"expr"}, {"log expressions"}, LIBLLDB_LOG_EXPRESSIONS},
  {{"formatters"}, {"log data formatters related activities"}, LIBLLDB_LOG_DATAFORMATTERS},
  {{"host"}, {"log host activities"}, LIBLLDB_LOG_HOST},
  {{"jit"}, {"log JIT events in the target"}, LIBLLDB_LOG_JIT_LOADER},
  {{"language"}, {"log language runtime events"}, LIBLLDB_LOG_LANGUAGE},
  {{"mmap"}, {"log mmap related activities"}, LIBLLDB_LOG_MMAP},
  {{"module"}, {"log module activities such as when modules are created, destroyed, replaced, and more"}, LIBLLDB_LOG_MODULES},
  {{"object"}, {"log object construction/destruction for important objects"}, LIBLLDB_LOG_OBJECT},
  {{"os"}, {"log OperatingSystem plugin related activities"}, LIBLLDB_LOG_OS},
  {{"platform"}, {"log platform events and activities"}, LIBLLDB_LOG_PLATFORM},
  {{"process"}, {"log process events and activities"}, LIBLLDB_LOG_PROCESS},
  {{"script"}, {"log events about the script interpreter"}, LIBLLDB_LOG_SCRIPT},
  {{"state"}, {"log private and public process state changes"}, LIBLLDB_LOG_STATE},
  {{"step"}, {"log step related activities"}, LIBLLDB_LOG_STEP},
  {{"symbol"}, {"log symbol related issues and warnings"}, LIBLLDB_LOG_SYMBOLS},
  {{"system-runtime"}, {"log system runtime events"}, LIBLLDB_LOG_SYSTEM_RUNTIME},
  {{"target"}, {"log target events and activities"}, LIBLLDB_LOG_TARGET},
  {{"temp"}, {"log internal temporary debug messages"}, LIBLLDB_LOG_TEMPORARY},
  {{"thread"}, {"log thread events and activities"}, LIBLLDB_LOG_THREAD},
  {{"types"}, {"log type system related activities"}, LIBLLDB_LOG_TYPES},
  {{"unwind"}, {"log stack unwind activities"}, LIBLLDB_LOG_UNWIND},
  {{"watch"}, {"log watchpoint related activities"}, LIBLLDB_LOG_WATCHPOINTS},
};

static Log::Channel g_log_channel(g_categories, LIBLLDB_LOG_DEFAULT);

#ifdef LLDB_ENABLE_SWIFT

static constexpr Log::Category g_swift_categories[] = {
  {{"health"}, {"log all messages related to lldb Swift operational health"}, LIBLLDB_SWIFT_LOG_HEALTH},
};

static Log::Channel g_swift_log_channel(g_swift_categories, LIBLLDB_SWIFT_LOG_HEALTH);

static std::string g_swift_log_buffer;

#endif

void lldb_private::InitializeLldbChannel() {
  Log::Register("lldb", g_log_channel);
#ifdef LLDB_ENABLE_SWIFT
  Log::Register("swift", g_swift_log_channel);

  llvm::raw_null_ostream error_stream;
  Log::EnableLogChannel(
      std::make_shared<llvm::raw_string_ostream>(g_swift_log_buffer),
      LLDB_LOG_OPTION_THREADSAFE, "swift", {"health"}, error_stream);
#endif
}

Log *lldb_private::GetLogIfAllCategoriesSet(uint32_t mask) {
  return g_log_channel.GetLogIfAll(mask);
}

Log *lldb_private::GetLogIfAnyCategoriesSet(uint32_t mask) {
  return g_log_channel.GetLogIfAny(mask);
}

#ifdef LLDB_ENABLE_SWIFT

Log *lldb_private::GetSwiftHealthLog() {
  return g_swift_log_channel.GetLogIfAny(LIBLLDB_SWIFT_LOG_HEALTH);
}

llvm::StringRef lldb_private::GetSwiftHealthLogData() {
  return g_swift_log_buffer;
}

#endif
