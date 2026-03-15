#include "stash.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/util.h"

namespace thunderbringer {

// Stash.save(repo, stasher, message?, flags?) → oid
static Napi::Value Save(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsObject()) {
    Napi::TypeError::New(env, "Expected (Repository, stasher, message?, flags?)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_signature* stasher = js_to_signature(env, info[1].As<Napi::Object>());
  GitSignatureGuard stasher_guard(stasher);

  std::string message = "";
  if (info.Length() > 2 && info[2].IsString()) {
    message = info[2].As<Napi::String>().Utf8Value();
  }

  uint32_t flags = GIT_STASH_DEFAULT;
  if (info.Length() > 3 && info[3].IsNumber()) {
    flags = info[3].As<Napi::Number>().Uint32Value();
  }

  git_oid oid;
  int error = git_stash_save(&oid, repo_wrap->GetRepo(), stasher,
                              message.empty() ? nullptr : message.c_str(), flags);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::String::New(env, oid_to_hex(&oid));
}

// Stash.pop(repo, index?) → void
static Napi::Value Pop(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  size_t index = 0;
  if (info.Length() > 1 && info[1].IsNumber()) {
    index = static_cast<size_t>(info[1].As<Napi::Number>().Uint32Value());
  }

  git_stash_apply_options opts = GIT_STASH_APPLY_OPTIONS_INIT;
  int error = git_stash_pop(repo_wrap->GetRepo(), index, &opts);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Stash.apply(repo, index?) → void
static Napi::Value Apply(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  size_t index = 0;
  if (info.Length() > 1 && info[1].IsNumber()) {
    index = static_cast<size_t>(info[1].As<Napi::Number>().Uint32Value());
  }

  git_stash_apply_options opts = GIT_STASH_APPLY_OPTIONS_INIT;
  int error = git_stash_apply(repo_wrap->GetRepo(), index, &opts);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Stash.drop(repo, index?) → void
static Napi::Value Drop(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  size_t index = 0;
  if (info.Length() > 1 && info[1].IsNumber()) {
    index = static_cast<size_t>(info[1].As<Napi::Number>().Uint32Value());
  }

  int error = git_stash_drop(repo_wrap->GetRepo(), index);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

struct StashEntry {
  size_t index;
  std::string message;
  std::string oid;
};

// Stash.list(repo) → [{ index, message, oid }]
static int stash_list_cb(size_t index, const char* message,
                          const git_oid* stash_id, void* payload) {
  auto* entries = static_cast<std::vector<StashEntry>*>(payload);
  StashEntry entry;
  entry.index = index;
  entry.message = message ? message : "";
  entry.oid = oid_to_hex(stash_id);
  entries->push_back(std::move(entry));
  return 0;
}

static Napi::Value List(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  std::vector<StashEntry> entries;
  int error = git_stash_foreach(repo_wrap->GetRepo(), stash_list_cb, &entries);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto arr = Napi::Array::New(env, entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    auto obj = Napi::Object::New(env);
    obj.Set("index", Napi::Number::New(env, static_cast<double>(entries[i].index)));
    obj.Set("message", Napi::String::New(env, entries[i].message));
    obj.Set("oid", Napi::String::New(env, entries[i].oid));
    arr.Set(static_cast<uint32_t>(i), obj);
  }

  return arr;
}

void InitStash(Napi::Env env, Napi::Object exports) {
  auto stash = Napi::Object::New(env);
  stash.Set("save", Napi::Function::New(env, Save, "save"));
  stash.Set("pop", Napi::Function::New(env, Pop, "pop"));
  stash.Set("apply", Napi::Function::New(env, Apply, "apply"));
  stash.Set("drop", Napi::Function::New(env, Drop, "drop"));
  stash.Set("list", Napi::Function::New(env, List, "list"));

  // Stash flags
  stash.Set("DEFAULT", Napi::Number::New(env, GIT_STASH_DEFAULT));
  stash.Set("KEEP_INDEX", Napi::Number::New(env, GIT_STASH_KEEP_INDEX));
  stash.Set("INCLUDE_UNTRACKED", Napi::Number::New(env, GIT_STASH_INCLUDE_UNTRACKED));
  stash.Set("INCLUDE_IGNORED", Napi::Number::New(env, GIT_STASH_INCLUDE_IGNORED));
  stash.Set("KEEP_ALL", Napi::Number::New(env, GIT_STASH_KEEP_ALL));

  exports.Set("Stash", stash);
}

}  // namespace thunderbringer
