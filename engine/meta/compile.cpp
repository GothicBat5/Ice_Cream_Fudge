#include "CompileJS.h"
#include "hermes/BCGen/HBC/HBC.h"
#include "hermes/SourceMap/SourceMapParser.h"
#include "hermes/Support/Algorithms.h"
#include "llvh/Support/SHA1.h"

namespace hermes {

static void diagHandlerAdapter(const llvh::SMDiagnostic &d, void *context) 
{
  auto diagHandler = static_cast<DiagnosticHandler *>(context);
  DiagnosticHandler::Kind kind = DiagnosticHandler::Note;
  
  switch (d.getKind()) {
    case llvh::SourceMgr::DK_Error:
      kind = DiagnosticHandler::Error;
      break;
    case llvh::SourceMgr::DK_Warning:
      kind = DiagnosticHandler::Warning;
      break;
    case llvh::SourceMgr::DK_Note:
      kind = DiagnosticHandler::Note;
      break;
    default:
      assert(false && "Hermes compiler produced unexpected diagnostic kind");
  };
  diagHandler->handle({kind,
       d.getLineNo(),
       d.getColumnNo() + 1,
       d.getMessage(),
       d.getRanges()});
}

bool compileJS(const std::string &str, const std::string &sourceURL,
    std::string &bytecode,
    const CompileJSOptions &compileJSOptions,
    DiagnosticHandler *diagHandler,
    std::optional<std::string_view> sourceMapBuf) 
{
  hbc::CompileFlags flags{};
  flags.debug = false;
  flags.format = EmitBundle;
  flags.emitAsyncBreakCheck = compileJSOptions.emitAsyncBreakCheck;
  flags.inlineMaxSize = compileJSOptions.inlineMaxSize;
  flags.enableES6BlockScoping = compileJSOptions.enableES6BlockScoping;
  flags.enableAsyncGenerators = compileJSOptions.enableAsyncGenerators;

  std::string smCopy;
  llvh::StringRef smRef;
  if (sourceMapBuf.has_value()) 
  {
    if (sourceMapBuf->back() != '\0') 
    {
      smCopy = *sourceMapBuf;
      smRef = {smCopy.data(), smCopy.size() + 1};
    }
    else {
      smRef = {sourceMapBuf->data(), sourceMapBuf->size()};
    }
  }

  auto res = hbc::createBCProviderFromSrc(std::make_unique<hermes::Buffer>((const uint8_t *)str.data(), str.size()),
      sourceURL, smRef,
      flags, "global",
      diagHandler ? diagHandlerAdapter : nullptr,
      diagHandler,
      compileJSOptions.optimize ? hbc::fullOptimizationPipeline : nullptr);
  if (!res.first)
    return false;

  llvh::raw_string_ostream bcstream(bytecode);

  BytecodeGenerationOptions opts(::hermes::EmitBundle);
  opts.optimizationEnabled = compileJSOptions.optimize;

  assert(res.first->getBytecodeModule() &&
      "BCProviderFromSrc must have a bytecode module");
  hbc::serializeBytecodeModule(*res.first->getBytecodeModule(),
      llvh::SHA1::hash(llvh::makeArrayRef(reinterpret_cast<const uint8_t *>(str.data()), str.size())),
      bcstream,
      opts);

  bcstream.flush();
  return true;
}

bool compileJS(const std::string &str,
    const std::string &sourceURL,
    std::string &bytecode,
    bool optimize, bool emitAsyncBreakCheck,
    DiagnosticHandler *diagHandler,
    std::optional<std::string_view> sourceMapBuf,bool debug) 
{
  CompileJSOptions options;
  options.optimize = optimize;
  options.emitAsyncBreakCheck = emitAsyncBreakCheck;
  options.debug = debug;
  return compileJS(str, sourceURL, bytecode, options, diagHandler, sourceMapBuf);
}

bool compileJS(const std::string &str, std::string &bytecode, bool optimize) {
  return compileJS(str, "", bytecode, optimize, false, nullptr);
}

bool compileJS(const std::string &str, const std::string &sourceURL,
    std::string &bytecode,
    bool optimize) 
{
  return compileJS(str, sourceURL, bytecode, optimize, false, nullptr);
}

} // namespace hermes
