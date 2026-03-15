#include "index.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/util.h"

namespace thunderbringer {

Napi::FunctionReference GitIndex::constructor_;

static Napi::Object IndexEntryToJS(Napi::Env env, const git_index_entry* entry) {
  auto obj = Napi::Object::New(env);
  obj.Set("path", Napi::String::New(env, entry->path ? entry->path : ""));
  obj.Set("oid", Napi::String::New(env, oid_to_hex(&entry->id)));
  obj.Set("fileSize", Napi::Number::New(env, static_cast<double>(entry->file_size)));
  obj.Set("mode", Napi::Number::New(env, entry->mode));
  obj.Set("flags", Napi::Number::New(env, entry->flags));
  obj.Set("stage", Napi::Number::New(env, git_index_entry_stage(entry)));
  return obj;
}

Napi::Object GitIndex::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Index", {
    InstanceMethod<&GitIndex::Add>("add"),
    InstanceMethod<&GitIndex::AddAll>("addAll"),
    InstanceMethod<&GitIndex::Remove>("remove"),
    InstanceMethod<&GitIndex::RemoveAll>("removeAll"),
    InstanceMethod<&GitIndex::Clear>("clear"),
    InstanceMethod<&GitIndex::Write>("write"),
    InstanceMethod<&GitIndex::WriteTree>("writeTree"),
    InstanceMethod<&GitIndex::GetEntryCount>("entryCount"),
    InstanceMethod<&GitIndex::GetEntries>("entries"),
    InstanceMethod<&GitIndex::GetByPath>("getByPath"),
  });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();
  exports.Set("Index", func);
  return exports;
}

Napi::Object GitIndex::NewInstance(Napi::Env env, git_index* index) {
  Napi::External<git_index> ext = Napi::External<git_index>::New(env, index);
  return constructor_.New({ext});
}

GitIndex::GitIndex(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<GitIndex>(info), index_(nullptr) {
  if (info.Length() > 0 && info[0].IsExternal()) {
    index_ = info[0].As<Napi::External<git_index>>().Data();
  }
}

GitIndex::~GitIndex() {
  if (index_) {
    git_index_free(index_);
    index_ = nullptr;
  }
}

Napi::Value GitIndex::Add(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected file path string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  int error = git_index_add_bypath(index_, path.c_str());
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

Napi::Value GitIndex::AddAll(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  git_strarray paths = {nullptr, 0};
  std::vector<char*> path_ptrs;
  std::vector<std::string> path_strs;

  if (info.Length() > 0 && info[0].IsArray()) {
    auto arr = info[0].As<Napi::Array>();
    path_strs.resize(arr.Length());
    path_ptrs.resize(arr.Length());
    for (uint32_t i = 0; i < arr.Length(); i++) {
      path_strs[i] = arr.Get(i).As<Napi::String>().Utf8Value();
      path_ptrs[i] = const_cast<char*>(path_strs[i].c_str());
    }
    paths.strings = path_ptrs.data();
    paths.count = path_ptrs.size();
  } else {
    // Default: add all
    static char star[] = "*";
    static char* star_ptr = star;
    paths.strings = &star_ptr;
    paths.count = 1;
  }

  int error = git_index_add_all(index_, &paths, 0, nullptr, nullptr);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

Napi::Value GitIndex::Remove(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected file path string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  int error = git_index_remove_bypath(index_, path.c_str());
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

Napi::Value GitIndex::RemoveAll(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  git_strarray paths = {nullptr, 0};
  std::vector<char*> path_ptrs;
  std::vector<std::string> path_strs;

  if (info.Length() > 0 && info[0].IsArray()) {
    auto arr = info[0].As<Napi::Array>();
    path_strs.resize(arr.Length());
    path_ptrs.resize(arr.Length());
    for (uint32_t i = 0; i < arr.Length(); i++) {
      path_strs[i] = arr.Get(i).As<Napi::String>().Utf8Value();
      path_ptrs[i] = const_cast<char*>(path_strs[i].c_str());
    }
    paths.strings = path_ptrs.data();
    paths.count = path_ptrs.size();
  } else {
    static char star[] = "*";
    static char* star_ptr = star;
    paths.strings = &star_ptr;
    paths.count = 1;
  }

  int error = git_index_remove_all(index_, &paths, nullptr, nullptr);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

Napi::Value GitIndex::Clear(const Napi::CallbackInfo& info) {
  int error = git_index_clear(index_);
  if (error < 0) {
    git_check_throw(info.Env(), error);
  }
  return info.Env().Undefined();
}

Napi::Value GitIndex::Write(const Napi::CallbackInfo& info) {
  int error = git_index_write(index_);
  if (error < 0) {
    git_check_throw(info.Env(), error);
  }
  return info.Env().Undefined();
}

Napi::Value GitIndex::WriteTree(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  git_oid oid;
  int error = git_index_write_tree(&oid, index_);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  return Napi::String::New(env, oid_to_hex(&oid));
}

Napi::Value GitIndex::GetEntryCount(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(git_index_entrycount(index_)));
}

Napi::Value GitIndex::GetEntries(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  size_t count = git_index_entrycount(index_);
  auto arr = Napi::Array::New(env, count);

  for (size_t i = 0; i < count; i++) {
    const git_index_entry* entry = git_index_get_byindex(index_, i);
    arr.Set(static_cast<uint32_t>(i), IndexEntryToJS(env, entry));
  }

  return arr;
}

Napi::Value GitIndex::GetByPath(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected file path").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  int stage = 0;
  if (info.Length() > 1 && info[1].IsNumber()) {
    stage = info[1].As<Napi::Number>().Int32Value();
  }

  const git_index_entry* entry = git_index_get_bypath(index_, path.c_str(), stage);
  if (!entry) {
    return env.Null();
  }

  return IndexEntryToJS(env, entry);
}

void InitIndex(Napi::Env env, Napi::Object exports) {
  GitIndex::Init(env, exports);
}

}  // namespace thunderbringer
