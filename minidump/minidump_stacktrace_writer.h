#ifndef CRASHPAD_MINIDUMP_MINIDUMP_STACKTRACE_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_STACKTRACE_WRITER_H_

#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_thread_id_map.h"
#include "minidump/minidump_writable.h"

namespace crashpad {

namespace internal {

struct Header {
  uint32_t version;
  uint32_t num_threads;
  uint32_t num_frames;
  uint32_t symbol_bytes;
};

struct RawThread {
  uint64_t thread_id;
  uint32_t start_frame;
  uint32_t num_frames;
};

struct RawFrame {
  uint64_t instruction_addr;
  uint32_t symbol_offset;
  uint32_t symbol_len;
};

}  // namespace internal

// TODO: Create a stub that will later return a real stack trace:
// `ThreadSnapshot.StackTrace` would need to return a
// `const std::vector<const FrameSnapshot*>&`, so that followup will also move
// that type to a more appropriate place.
// That would be https://getsentry.atlassian.net/browse/NATIVE-198
class FrameSnapshot {
 public:
  FrameSnapshot(uint64_t instruction_addr, std::string symbol)
      : instruction_addr_(instruction_addr), symbol_(symbol) {}

  uint64_t InstructionAddr() const { return instruction_addr_; };
  const std::string& Symbol() const { return symbol_; };

 private:
  uint64_t instruction_addr_;
  std::string symbol_;
};

class ThreadSnapshot;

//! \brief The writer for our custom client-side stacktraces stream in a
//! minidump file.
class MinidumpStacktraceListWriter final
    : public internal::MinidumpStreamWriter {
 public:
  MinidumpStacktraceListWriter();
  ~MinidumpStacktraceListWriter() override;

  //! \brief  TODO
  //!
  //! \param[in] thread_snapshots The thread snapshots to use as source data.
  //! \param[in] thread_id_map A MinidumpThreadIDMap to be consulted to
  //!     determine the 32-bit minidump thread ID to use for the thread
  //!     identified by \a thread_snapshots.
  void InitializeFromSnapshot(
      const std::vector<const ThreadSnapshot*>& thread_snapshots,
      const MinidumpThreadIDMap& thread_id_map);

 protected:
  // MinidumpWritable:
  // bool Freeze() override;
  size_t SizeOfObject() override;
  size_t Alignment() override;
  // std::vector<MinidumpWritable*> Children() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override;

 private:
  std::vector<internal::RawThread> threads_;
  std::vector<internal::RawFrame> frames_;
  std::vector<uint8_t> symbol_bytes_;
  internal::Header stacktrace_header_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpStacktraceListWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_STACKTRACE_WRITER_H_
