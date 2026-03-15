#include "remote.h"
#include "credential.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"
#include <mutex>

namespace thunderbringer {

// Remote.list(repo) → string[]
static Napi::Value List(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_strarray list = {nullptr, 0};
  int error = git_remote_list(&list, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto arr = Napi::Array::New(env, list.count);
  for (size_t i = 0; i < list.count; i++) {
    arr.Set(static_cast<uint32_t>(i), Napi::String::New(env, list.strings[i]));
  }
  git_strarray_dispose(&list);

  return arr;
}

// Remote.add(repo, name, url) → void
static Napi::Value Add(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name, url)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  std::string url = info[2].As<Napi::String>().Utf8Value();

  git_remote* remote = nullptr;
  int error = git_remote_create(&remote, repo_wrap->GetRepo(), name.c_str(), url.c_str());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  git_remote_free(remote);
  return env.Undefined();
}

// Remote.delete(repo, name) → void
static Napi::Value Delete(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();

  int error = git_remote_delete(repo_wrap->GetRepo(), name.c_str());
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Remote.getUrl(repo, name) → { url, pushUrl }
static Napi::Value GetUrl(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();

  git_remote* remote = nullptr;
  int error = git_remote_lookup(&remote, repo_wrap->GetRepo(), name.c_str());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitRemoteGuard guard(remote);

  auto result = Napi::Object::New(env);
  const char* url = git_remote_url(remote);
  if (url) result.Set("url", Napi::String::New(env, url));

  const char* push_url = git_remote_pushurl(remote);
  if (push_url) result.Set("pushUrl", Napi::String::New(env, push_url));

  result.Set("name", Napi::String::New(env, name));
  return result;
}

// Fetch worker
class FetchWorker : public ThunderbringerWorker<void> {
 public:
  FetchWorker(Napi::Env env, git_repository* repo, std::string remote_name)
      : ThunderbringerWorker<void>(env), repo_(repo),
        remote_name_(std::move(remote_name)) {}

 protected:
  void DoExecute() override {
    git_remote* remote = nullptr;
    git_check(git_remote_lookup(&remote, repo_, remote_name_.c_str()));
    GitRemoteGuard guard(remote);

    git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
    git_check(git_remote_fetch(remote, nullptr, &opts, nullptr));
  }

 private:
  git_repository* repo_;
  std::string remote_name_;
};

// Remote.fetch(repo, remoteName) → Promise<void>
static Napi::Value Fetch(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, remoteName)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string remote_name = info[1].As<Napi::String>().Utf8Value();

  auto* worker = new FetchWorker(env, repo_wrap->GetRepo(), remote_name);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Push worker
class PushWorker : public ThunderbringerWorker<void> {
 public:
  PushWorker(Napi::Env env, git_repository* repo, std::string remote_name,
             std::vector<std::string> refspecs)
      : ThunderbringerWorker<void>(env), repo_(repo),
        remote_name_(std::move(remote_name)),
        refspecs_(std::move(refspecs)) {}

 protected:
  void DoExecute() override {
    git_remote* remote = nullptr;
    git_check(git_remote_lookup(&remote, repo_, remote_name_.c_str()));
    GitRemoteGuard guard(remote);

    std::vector<char*> refspec_ptrs;
    for (auto& r : refspecs_) {
      refspec_ptrs.push_back(const_cast<char*>(r.c_str()));
    }
    git_strarray refspecs = {refspec_ptrs.data(), refspec_ptrs.size()};

    git_push_options opts = GIT_PUSH_OPTIONS_INIT;
    git_check(git_remote_push(remote, &refspecs, &opts));
  }

 private:
  git_repository* repo_;
  std::string remote_name_;
  std::vector<std::string> refspecs_;
};

// Remote.push(repo, remoteName, refspecs) → Promise<void>
static Napi::Value Push(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsArray()) {
    Napi::TypeError::New(env, "Expected (Repository, remoteName, refspecs[])").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string remote_name = info[1].As<Napi::String>().Utf8Value();

  auto refspec_arr = info[2].As<Napi::Array>();
  std::vector<std::string> refspecs;
  for (uint32_t i = 0; i < refspec_arr.Length(); i++) {
    refspecs.push_back(refspec_arr.Get(i).As<Napi::String>().Utf8Value());
  }

  auto* worker = new PushWorker(env, repo_wrap->GetRepo(), remote_name, refspecs);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

void InitRemote(Napi::Env env, Napi::Object exports) {
  auto remote = Napi::Object::New(env);
  remote.Set("list", Napi::Function::New(env, List, "list"));
  remote.Set("add", Napi::Function::New(env, Add, "add"));
  remote.Set("delete", Napi::Function::New(env, Delete, "delete"));
  remote.Set("getUrl", Napi::Function::New(env, GetUrl, "getUrl"));
  remote.Set("fetch", Napi::Function::New(env, Fetch, "fetch"));
  remote.Set("push", Napi::Function::New(env, Push, "push"));
  exports.Set("Remote", remote);
}

}  // namespace thunderbringer
