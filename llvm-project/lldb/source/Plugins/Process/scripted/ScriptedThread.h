//===-- ScriptedThread.h ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_SCRIPTED_THREAD_H
#define LLDB_SOURCE_PLUGINS_SCRIPTED_THREAD_H

#include <string>

#include "ScriptedProcess.h"

#include "Plugins/Process/Utility/DynamicRegisterInfo.h"
#include "Plugins/Process/Utility/RegisterContextMemory.h"
#include "lldb/Interpreter/ScriptInterpreter.h"
#include "lldb/Target/Thread.h"

namespace lldb_private {
class ScriptedProcess;
}

namespace lldb_private {

class ScriptedThread : public lldb_private::Thread {
public:
  ScriptedThread(ScriptedProcess &process, Status &error);

  ~ScriptedThread() override;

  lldb::RegisterContextSP GetRegisterContext() override;

  lldb::RegisterContextSP
  CreateRegisterContextForFrame(lldb_private::StackFrame *frame) override;

  bool CalculateStopInfo() override;

  const char *GetInfo() override { return nullptr; }

  const char *GetName() override;

  const char *GetQueueName() override;

  void WillResume(lldb::StateType resume_state) override;

  void RefreshStateAfterStop() override;

  void ClearStackFrames() override;

private:
  void CheckInterpreterAndScriptObject() const;
  lldb::ScriptedThreadInterfaceSP GetInterface() const;

  ScriptedThread(const ScriptedThread &) = delete;
  const ScriptedThread &operator=(const ScriptedThread &) = delete;

  std::shared_ptr<DynamicRegisterInfo> GetDynamicRegisterInfo();

  const ScriptedProcess &m_scripted_process;
  std::shared_ptr<DynamicRegisterInfo> m_register_info_sp = nullptr;
  lldb_private::StructuredData::ObjectSP m_script_object_sp = nullptr;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_SCRIPTED_THREAD_H
