#include "signature.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/util.h"

namespace thunderbringer {

// Signature.create(name, email) → { name, email, time, offset }
static Napi::Value Create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (name, email)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  std::string email = info[1].As<Napi::String>().Utf8Value();

  git_signature* sig = nullptr;
  int error = git_signature_now(&sig, name.c_str(), email.c_str());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  GitSignatureGuard guard(sig);
  return signature_to_js(env, sig);
}

// Signature.createWithTime(name, email, time, offset) → { name, email, time, offset }
static Napi::Value CreateWithTime(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 4) {
    Napi::TypeError::New(env, "Expected (name, email, time, offset)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  std::string email = info[1].As<Napi::String>().Utf8Value();
  int64_t time = static_cast<int64_t>(info[2].As<Napi::Number>().DoubleValue() / 1000.0);
  int offset = info[3].As<Napi::Number>().Int32Value();

  git_signature* sig = nullptr;
  int error = git_signature_new(&sig, name.c_str(), email.c_str(), time, offset);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  GitSignatureGuard guard(sig);
  return signature_to_js(env, sig);
}

// Signature.default(repo) → { name, email, time, offset }
static Napi::Value Default(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_signature* sig = nullptr;
  int error = git_signature_default(&sig, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  GitSignatureGuard guard(sig);
  return signature_to_js(env, sig);
}

void InitSignature(Napi::Env env, Napi::Object exports) {
  auto signature = Napi::Object::New(env);
  signature.Set("create", Napi::Function::New(env, Create, "create"));
  signature.Set("createWithTime", Napi::Function::New(env, CreateWithTime, "createWithTime"));
  signature.Set("default", Napi::Function::New(env, Default, "default"));
  exports.Set("Signature", signature);
}

}  // namespace thunderbringer
