#include <fstream>
#include <streambuf>
#include <string>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#elif _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127 4100 4512 4800 4267 4251)
#endif
#include <jsapi.h>

#ifdef USE_REMOTE_DEBUGGER
#include <jsrdbg/jsrdbg.h>
#endif

namespace {
// The class of the global object
JSClass global_class = {"global",         JSCLASS_GLOBAL_FLAGS,
                        JS_PropertyStub,  JS_DeletePropertyStub,
                        JS_PropertyStub,  JS_StrictPropertyStub,
                        JS_EnumerateStub, JS_ResolveStub,
                        JS_ConvertStub,   nullptr,
                        nullptr,          nullptr,
                        nullptr};

// The error reporter callback
void report_error(JSContext *cx, const char *message, JSErrorReport *report) {
  fprintf(stderr, "%s:%u:%s\n", report->filename ? report->filename : "[no filename]",
          (unsigned int)report->lineno, message);
}

JSBool log_impl(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  for (unsigned i = 0; i < args.length(); i++) {
    JS::RootedString str(cx);

    if (args[i].isObject() && !args[i].isNull()) {
      str = args[i].toString();
    } else if (args[i].isString()) {
      str = args[i].toString();
    } else {
      return false;
    }

    if (!str) {
      return false;
    }

    char *utf8_encoded = JS_EncodeStringToUTF8(cx, str);
    if (!utf8_encoded) {
      return false;
    }

    fprintf(stdout, "%s%s", i ? " " : "", utf8_encoded);
    JS_free(cx, utf8_encoded);
  }

  fputc('\n', stdout);
  fflush(stdout);

  args.rval().setUndefined();
  return true;
}

JSFunctionSpec global_functions[] = {JS_FS("log", log_impl, 0, 0), JS_FS_END};

#ifdef USE_REMOTE_DEBUGGER
// Component responsible for loading script's source code if the JS engine
// cannot provide it.
//
// Scripts are loaded twice, by the actual JS interpreter and by the debugger
// to show the source code when debugging. This is about the latter
class ScriptLoader final : public JSR::IJSScriptLoader {
public:
  int load(JSContext *cx, const std::string &path, std::string &script) {
    if (!path.empty()) {
      std::ifstream ifstr(path);
      ifstr.seekg(0, std::ios::end);
      script.reserve(ifstr.tellg());
      ifstr.seekg(0, std::ios::beg);

      script.assign((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
      return JSR_ERROR_NO_ERROR;
    }

    return JSR_ERROR_FILE_NOT_FOUND;
  }
};
#endif

int run(JSContext *cx, const std::string &filename) {
#ifdef USE_REMOTE_DEBUGGER
  // Initialize debugger
  ScriptLoader script_loader;
  JSR::JSRemoteDebuggerCfg debugger_config;
  // JSR_DEFAULT_TCP_BINDING_IP is localhost
  debugger_config.setTcpHost("0.0.0.0");
  debugger_config.setTcpPort(JSR_DEFAULT_TCP_PORT);
  debugger_config.setScriptLoader(&script_loader);

  // Configure debugger engine
  JSR::JSDbgEngineOptions engine_options;
  engine_options.setSourceCodeDisplacement(-1);

  // Create debugger and install it. Scripts are suspended as soon as first
  // 'debugger;' statement is reached
  JSR::JSRemoteDebugger debugger(debugger_config);
  if (debugger.install(cx, filename, engine_options) != JSR_ERROR_NO_ERROR) {
    return 1;
  }

  if (debugger.start() != JSR_ERROR_NO_ERROR) {
    debugger.uninstall(cx);
    return 1;
  }
#endif

  // Enter a request before running anything in the context
  JSAutoRequest ar(cx);

  // Create the global object and a new compartment for it
  JS::RootedObject global(cx);
  JS::CompartmentOptions compartment_options;
  compartment_options.setVersion(JSVERSION_LATEST);
  global = JS_NewGlobalObject(cx, &global_class, nullptr, compartment_options);
  if (!global) {
    return 1;
  }

  // Enter the new global object's compartment
  JSAutoCompartment ac(cx, global);

  // Populate the global object with the standard globals, like Object and
  // Array
  if (!JS_InitStandardClasses(cx, global)) {
    return 1;
  }

  // Your application code here. This may include JSAPI calls to create your
  // own custom JS objects and run scripts

  if (!JS_DefineFunctions(cx, global, global_functions)) {
    return 1;
  }

#ifdef USE_REMOTE_DEBUGGER
  // Register newly created global object into the debugger in order to make it
  // debug-able.
  if (debugger.addDebuggee(cx, global) != JSR_ERROR_NO_ERROR) {
    return 1;
  }
#endif

  // Execute a given script and print results
  JS::CompileOptions compile_options(cx);
  compile_options.setUTF8(true).setCompileAndGo(true);
  JS::RootedScript script(cx, JS::Compile(cx, global, compile_options, filename.c_str()));
  if (!script) {
    return 1;
  }

  JS::RootedValue result(cx);
  const auto success = JS_ExecuteScript(cx, global, script, result.address());
#ifdef USE_REMOTE_DEBUGGER
  debugger.stop();
  debugger.uninstall(cx);
#endif
  if (!success) {
    return 1;
  }

  if (!result.isUndefined()) {
    // Print
    JS::RootedString str(cx);
    str = JS_ValueToSource(cx, result);
    if (!str) {
      return 1;
    }

    char *utf8_encoded = JS_EncodeStringToUTF8(cx, str);
    if (!utf8_encoded) {
      return 1;
    }

    fprintf(stdout, "%s\n", utf8_encoded);
    JS_free(cx, utf8_encoded);
  }

  return 0;
}
}

int main(int argc, const char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s [script]\n", argv[0]);
    exit(1);
  }

  const auto filename = std::string(argv[1]);

  // Create a JS runtime
  const uint32_t maxbytes = 8L * 1024L * 1024L;
  JSRuntime *rt = JS_NewRuntime(maxbytes, JS_USE_HELPER_THREADS);
  if (!rt) {
    return 1;
  }

#ifdef USE_REMOTE_DEBUGGER

  // Setting a higher stack (recursion) limit is needed in order to have
  // sufficient stack size when objects are registered into the debugger.
  // Otherwise we could get a "too much recursion" error when calling
  // debugger.addDebuggee()
  const std::size_t max_stack_size = 128 * sizeof(std::size_t) * 1024;
  JS_SetNativeStackQuota(rt, max_stack_size);

#endif

  // Create a context
  JSContext *cx = JS_NewContext(rt, 8192);
  if (!cx) {
    return 1;
  }

  JS_SetErrorReporter(cx, report_error);

  int status = run(cx, filename);

  // Shut everything down
  JS_DestroyContext(cx);
  JS_DestroyRuntime(rt);
  JS_ShutDown();

  return status;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif _MSC_VER
#pragma warning(pop)
#endif

// vim:et ts=4 sw=4
