#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

class GitIndex : public Napi::ObjectWrap<GitIndex> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::Object NewInstance(Napi::Env env, git_index* index);

  explicit GitIndex(const Napi::CallbackInfo& info);
  ~GitIndex();

  git_index* GetIndex() const { return index_; }

 private:
  Napi::Value Add(const Napi::CallbackInfo& info);
  Napi::Value AddAll(const Napi::CallbackInfo& info);
  Napi::Value Remove(const Napi::CallbackInfo& info);
  Napi::Value RemoveAll(const Napi::CallbackInfo& info);
  Napi::Value Clear(const Napi::CallbackInfo& info);
  Napi::Value Write(const Napi::CallbackInfo& info);
  Napi::Value WriteTree(const Napi::CallbackInfo& info);
  Napi::Value GetEntryCount(const Napi::CallbackInfo& info);
  Napi::Value GetEntries(const Napi::CallbackInfo& info);
  Napi::Value GetByPath(const Napi::CallbackInfo& info);

  static Napi::FunctionReference constructor_;
  git_index* index_;
};

void InitIndex(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer
