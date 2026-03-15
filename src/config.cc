#include "config.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/util.h"

namespace thunderbringer {

// Config.get(repo, key) → string | null
static Napi::Value Get(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, key)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  git_buf buf = GIT_BUF_INIT;
  error = git_config_get_string_buf(&buf, config, key.c_str());
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto result = Napi::String::New(env, buf.ptr, buf.size);
  git_buf_dispose(&buf);
  return result;
}

// Config.set(repo, key, value) → void
static Napi::Value Set(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, key, value)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();
  std::string value = info[2].As<Napi::String>().Utf8Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  error = git_config_set_string(config, key.c_str(), value.c_str());
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Config.getInt(repo, key) → number | null
static Napi::Value GetInt(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, key)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  int32_t val = 0;
  error = git_config_get_int32(&val, config, key.c_str());
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::Number::New(env, val);
}

// Config.getBool(repo, key) → boolean | null
static Napi::Value GetBool(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, key)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  int val = 0;
  error = git_config_get_bool(&val, config, key.c_str());
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::Boolean::New(env, val != 0);
}

// Config.setInt(repo, key, value) → void
static Napi::Value SetInt(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsNumber()) {
    Napi::TypeError::New(env, "Expected (Repository, key, value)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();
  int32_t value = info[2].As<Napi::Number>().Int32Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  error = git_config_set_int32(config, key.c_str(), value);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Config.setBool(repo, key, value) → void
static Napi::Value SetBool(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsBoolean()) {
    Napi::TypeError::New(env, "Expected (Repository, key, value)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();
  bool value = info[2].As<Napi::Boolean>().Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  error = git_config_set_bool(config, key.c_str(), value ? 1 : 0);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Config.deleteEntry(repo, key) → void
static Napi::Value DeleteEntry(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, key)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string key = info[1].As<Napi::String>().Utf8Value();

  git_config* config = nullptr;
  int error = git_repository_config(&config, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitConfigGuard guard(config);

  error = git_config_delete_entry(config, key.c_str());
  if (error < 0 && error != GIT_ENOTFOUND) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

void InitConfig(Napi::Env env, Napi::Object exports) {
  auto config = Napi::Object::New(env);
  config.Set("get", Napi::Function::New(env, Get, "get"));
  config.Set("set", Napi::Function::New(env, Set, "set"));
  config.Set("getInt", Napi::Function::New(env, GetInt, "getInt"));
  config.Set("getBool", Napi::Function::New(env, GetBool, "getBool"));
  config.Set("setInt", Napi::Function::New(env, SetInt, "setInt"));
  config.Set("setBool", Napi::Function::New(env, SetBool, "setBool"));
  config.Set("deleteEntry", Napi::Function::New(env, DeleteEntry, "deleteEntry"));
  exports.Set("Config", config);
}

}  // namespace thunderbringer
