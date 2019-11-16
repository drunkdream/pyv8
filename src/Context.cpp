#include "Context.h"

#include "Wrapper.h"
#include "Engine.h"

void CContext::Expose(void)
{
  py::class_<CContext, boost::noncopyable>("JSContext", "JSContext is an execution context.", py::no_init)
      .def(py::init<const CContext &>("create a new context base on a exists context"))
      .def(py::init<py::object, py::list>((py::arg("global") = py::object(),
                                           py::arg("extensions") = py::list()),
                                          "create a new context base on global object"))

      .add_property("securityToken", &CContext::GetSecurityToken, &CContext::SetSecurityToken)

      .add_property("locals", &CContext::GetGlobal, "Local variables within context")

      //.add_static_property("entered", &CContext::GetEntered,
      //                     "The last entered context.")
      .add_static_property("current", &CContext::GetCurrent,
                           "The context that is on the top of the stack.")
      .add_static_property("calling", &CContext::GetCalling,
                           "The context of the calling JavaScript code.")
      .add_static_property("inContext", &CContext::InContext,
                           "Returns true if V8 has a current context.")

      .def("eval", &CContext::Evaluate, (py::arg("source"),
                                         py::arg("name") = std::string(),
                                         py::arg("line") = -1,
                                         py::arg("col") = -1))
      .def("eval", &CContext::EvaluateW, (py::arg("source"),
                                          py::arg("name") = std::string(),
                                          py::arg("line") = -1,
                                          py::arg("col") = -1))

      .def("enter", &CContext::Enter, "Enter this context. "
                                      "After entering a context, all code compiled and "
                                      "run is compiled and run in this context.")
      .def("leave", &CContext::Leave, "Exit this context. "
                                      "Exiting the current context restores the context "
                                      "that was in place when entering the current context.")
      .def("dispose", &CContext::Dispose, "Dispose this context.",
           "Force to dispose a context, release all resources.")

      .def("__nonzero__", &CContext::IsEntered, "the context has been entered.");

  py::objects::class_value_wrapper<CContextPtr,
                                   py::objects::make_ptr_instance<CContext,
                                                                  py::objects::pointer_holder<CContextPtr, CContext>>>();
}

CContext::CContext(v8::Handle<v8::Context> context, v8::Isolate *isolate) : m_context(isolate, context)
{
  BOOST_LOG_SEV(logger(), trace) << "context wrapped";
}

CContext::CContext(const CContext &context, v8::Isolate *isolate) : m_context(isolate, context.m_context)
{
  BOOST_LOG_SEV(logger(), trace) << "context copied";
}

CContext::CContext(py::object global, py::list extensions, v8::Isolate *isolate)
{
  v8::HandleScope handle_scope(isolate);

  std::auto_ptr<v8::ExtensionConfiguration> cfg;
  std::vector<std::string> ext_names;
  std::vector<const char *> ext_ptrs;

  for (Py_ssize_t i = 0; i < PyList_Size(extensions.ptr()); i++)
  {
    py::extract<const std::string> extractor(::PyList_GetItem(extensions.ptr(), i));

    if (extractor.check())
    {
      ext_names.push_back(extractor());
    }
  }

  for (size_t i = 0; i < ext_names.size(); i++)
  {
    ext_ptrs.push_back(ext_names[i].c_str());
  }

  if (!ext_ptrs.empty())
    cfg.reset(new v8::ExtensionConfiguration(ext_ptrs.size(), &ext_ptrs[0]));

  v8::TryCatch try_catch(isolate);
  v8::Handle<v8::Context> context = v8::Context::New(isolate, cfg.get());

  if (context.IsEmpty())
  {
    if (try_catch.HasCaught())
      CJavascriptException::ThrowIf(isolate, try_catch);

    BOOST_LOG_SEV(CIsolate::Current().Logger(), warning) << "failed to create context";
  }
  else
  {
    m_context.Reset(isolate, context);

    BOOST_LOG_SEV(logger(), trace) << "context created";

    if (!global.is_none())
    {
      v8::Context::Scope context_scope(context);

      context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "__proto__"), CPythonObject::Wrap(global));

      m_global = global;

      Py_DECREF(global.ptr());
    }
  }
}

void CContext::Dispose(bool disposed)
{
  if (m_context.IsEmpty())
    return;

  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  v8::HandleScope handle_scope(isolate);

  auto context = m_context.Get(isolate);

  BOOST_LOG_SEV(logger(), trace) << "context " << (disposed ? "disposed" : "destroyed");

  delete GetEmbedderData<logger_t>(context, EmbedderDataFields::LoggerIndex);

  m_context.Reset();
}

logger_t &CContext::GetLogger(v8::Handle<v8::Context> context)
{
  auto logger = GetEmbedderData<logger_t>(context, EmbedderDataFields::LoggerIndex, [context]() {
    auto logger = new logger_t();

    logger->add_attribute(ISOLATE_ATTR, attrs::constant<const v8::Isolate *>(context->GetIsolate()));
    logger->add_attribute(CONTEXT_ATTR, attrs::constant<const v8::Context *>(*context));

    return logger;
  });

  return *logger;
}

py::object CContext::GetGlobal(void) const
{
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  return CJavascriptObject::Wrap(Context()->Global());
}

py::str CContext::GetSecurityToken(void) const
{
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::Value> token = Context()->GetSecurityToken();

  if (token.IsEmpty())
    return py::str();

  v8::String::Utf8Value str(isolate, token->ToString(isolate));

  return py::str(*str, str.length());
}

void CContext::SetSecurityToken(py::str token)
{
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  if (token.is_none())
  {
    BOOST_LOG_SEV(logger(), trace) << "clear security token";

    Context()->UseDefaultSecurityToken();
  }
  else
  {
    BOOST_LOG_SEV(logger(), trace) << "set security token " << py::extract<const char *>(token);

    Context()->SetSecurityToken(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), py::extract<const char *>(token)()));
  }
}

void CContext::Enter(void)
{
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  Context()->Enter();

  BOOST_LOG_SEV(logger(), trace) << "context entered";
}
void CContext::Leave(void)
{
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  Context()->Exit();

  BOOST_LOG_SEV(logger(), trace) << "context exited";
}

py::object CContext::GetEntered()
{
  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::Context> entered = isolate->GetEnteredContext();

  return (!isolate->InContext() || entered.IsEmpty()) ? py::object() : py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CContext>(CContextPtr(new CContext(entered)))));
}

py::object CContext::GetCurrent()
{
  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::Context> current = isolate->GetCurrentContext();

  return (current.IsEmpty()) ? py::object() : py::object(py::handle<>(py::converter::shared_ptr_to_python<CContext>(CContextPtr(new CContext(current)))));
}

py::object CContext::GetCalling()
{
  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::Context> calling = isolate->GetCurrentContext();

  return (!isolate->InContext() || calling.IsEmpty()) ? py::object() : py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CContext>(CContextPtr(new CContext(calling)))));
}

py::object CContext::Evaluate(const std::string &src,
                              const std::string name,
                              int line, int col)
{
  CEngine engine(v8::Isolate::GetCurrent());

  CScriptPtr script = engine.Compile(src, name, line, col);

  BOOST_LOG_SEV(logger(), trace) << "eval script: " << src;

  return script->Run();
}

py::object CContext::EvaluateW(const std::wstring &src,
                               const std::string name,
                               int line, int col)
{
  CEngine engine(v8::Isolate::GetCurrent());

  CScriptPtr script = engine.CompileW(src, name, line, col);

  BOOST_LOG_SEV(logger(), trace) << "eval script: " << src;

  return script->Run();
}
