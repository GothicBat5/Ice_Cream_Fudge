
#pragma once

#include <exception>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <hermes/Public/HermesExport.h>
#include <hermes/Public/RuntimeConfig.h>
#include <hermes/Public/SamplingProfiler.h>
#include <jsi/hermes-interfaces.h>
#include <jsi/jsi.h>
#include <unordered_map>

struct HermesTestHelper;
struct SHUnit;
struct SHRuntime;

namespace hermes {
namespace vm {
class GCExecTrace;
class Runtime;
class SerializedValue;
} // namespace vm
} // namespace hermes

namespace facebook {
namespace jsi {

class ThreadSafeRuntime;

}

namespace hermes {

namespace debugger {
class Debugger;
}

class HermesRuntime;

class HERMES_EXPORT IHermesRootAPI : public jsi::ICast 
{
 public:
  static constexpr jsi::UUID uuid{
      0xb654d898,
      0xdfad,
      0x11ef,
      0x859a,
      0x325096b39f47};

  // Returns an instance of Hermes Runtime.
  virtual std::unique_ptr<HermesRuntime> makeHermesRuntime(const ::hermes::vm::RuntimeConfig &runtimeConfig) = 0;
  virtual bool isHermesBytecode(const uint8_t *data, size_t len) = 0;
  virtual uint32_t getBytecodeVersion() = 0;
  virtual void prefetchHermesBytecode(const uint8_t *data, size_t len) = 0;
  virtual bool hermesBytecodeSanityCheck(const uint8_t *data, size_t len, std::string *errorMessage = nullptr) = 0;
  virtual void setFatalHandler(void (*handler)(const std::string &)) = 0;
  virtual std::pair<const uint8_t *, size_t> getBytecodeEpilogue(const uint8_t *data, size_t len) = 0;
  virtual void enableSamplingProfiler(double meanHzFreq = 100) = 0;
  virtual void disableSamplingProfiler() = 0;
  virtual void dumpSampledTraceToFile(const std::string &fileName) = 0;
  virtual void dumpSampledTraceToStream(std::ostream &stream) = 0;
  virtual std::unordered_map<std::string, std::vector<std::string>>
  getExecutedFunctions() = 0;
  virtual bool isCodeCoverageProfilerEnabled() = 0;
  virtual void enableCodeCoverageProfiler() = 0;
  virtual void disableCodeCoverageProfiler() = 0;

 protected:
  ~IHermesRootAPI() {}
};


class HERMES_EXPORT ISetFatalHandler : public jsi::ICast {
 public:
  static constexpr jsi::UUID uuid{
      0xda98a610,
      0x09cb,
      0x11f0,
      0x87bf,
      0x325096b39f47};

  virtual void setFatalHandler(void (*handler)(const std::string &)) = 0;

 protected:
  ~ISetFatalHandler() = default;
};

/// Interface for methods that are exposed for test purposes.
class HERMES_EXPORT IHermesTestHelpers : public jsi::ICast 
{
 public:
  static constexpr jsi::UUID uuid{
      0x664e489a,
      0xf941,
      0x11ef,
      0xa44c,
      0x325096b39f47};

  virtual size_t rootsListLengthForTests() const = 0;

 protected:
  ~IHermesTestHelpers() = default;
};

#ifdef JSI_UNSTABLE
// Interface for methods that are exposed for tracing purposes.
class IHermesTracingHelpers : public jsi::ICast {
 public:
  static constexpr jsi::UUID uuid{
      0x74ac2c4e,
      0xc660,
      0x11f0,
      0x8de9,
      0x0242ac120002};

  virtual const ::hermes::vm::SerializedValue *getHermesSerializedValue(const jsi::Serialized &serialized) const = 0;
  virtual const std::shared_ptr<jsi::Serialized> makeSerialized(::hermes::vm::SerializedValue &value) const = 0;

 protected:
  ~IHermesTracingHelpers() = default;
};
#endif

class HermesRuntime : public jsi::Runtime,
                      public IHermes,
                      public IHermesSHUnit 
{
 public:
  ~HermesRuntime() override = default;

  using jsi::Runtime::castInterface;
};

HERMES_EXPORT jsi::ICast *makeHermesRootAPI();
HERMES_EXPORT::hermes::vm::RuntimeConfig hardenedHermesRuntimeConfig();
HERMES_EXPORT std::unique_ptr<HermesRuntime> makeHermesRuntime(const::hermes::vm::RuntimeConfig &runtimeConfig = ::hermes::vm::RuntimeConfig());
HERMES_EXPORT std::unique_ptr<HermesRuntime> makeHermesRuntimeNoThrow(const ::hermes::vm::RuntimeConfig &runtimeConfig = ::hermes::vm::RuntimeConfig()) noexcept;
HERMES_EXPORT std::unique_ptr<jsi::ThreadSafeRuntime>

makeThreadSafeHermesRuntime(const ::hermes::vm::RuntimeConfig &runtimeConfig = ::hermes::vm::RuntimeConfig());
} // namespace hermes
} // namespace facebook
