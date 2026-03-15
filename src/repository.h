#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

class GitRepository : public Napi::ObjectWrap<GitRepository> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::Object NewInstance(Napi::Env env, git_repository* repo);

  explicit GitRepository(const Napi::CallbackInfo& info);
  ~GitRepository();

  git_repository* GetRepo() const { return repo_; }

 private:
  // Static factory methods (async)
  static Napi::Value Open(const Napi::CallbackInfo& info);
  static Napi::Value Init_(const Napi::CallbackInfo& info);
  static Napi::Value Clone(const Napi::CallbackInfo& info);
  static Napi::Value Discover(const Napi::CallbackInfo& info);

  // Instance methods
  Napi::Value GetPath(const Napi::CallbackInfo& info);
  Napi::Value GetWorkdir(const Napi::CallbackInfo& info);
  Napi::Value IsBare(const Napi::CallbackInfo& info);
  Napi::Value IsEmpty(const Napi::CallbackInfo& info);
  Napi::Value GetState(const Napi::CallbackInfo& info);
  Napi::Value GetHead(const Napi::CallbackInfo& info);
  Napi::Value HeadDetached(const Napi::CallbackInfo& info);
  Napi::Value HeadUnborn(const Napi::CallbackInfo& info);
  void Free(const Napi::CallbackInfo& info);

  static Napi::FunctionReference constructor_;
  git_repository* repo_;
};

void InitRepository(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer
