#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

Napi::FunctionReference GitRepository::constructor_;

// ── Async Workers ─────────────────────────────────────────────────

class OpenWorker : public ThunderbringerWorker<git_repository*> {
 public:
  OpenWorker(Napi::Env env, std::string path)
      : ThunderbringerWorker<git_repository*>(env), path_(std::move(path)) {}

 protected:
  git_repository* DoExecute() override {
    git_repository* repo = nullptr;
    git_check(git_repository_open(&repo, path_.c_str()));
    return repo;
  }

  Napi::Value GetResult(Napi::Env env, git_repository* repo) override {
    return GitRepository::NewInstance(env, repo);
  }

 private:
  std::string path_;
};

class InitWorker : public ThunderbringerWorker<git_repository*> {
 public:
  InitWorker(Napi::Env env, std::string path, bool bare)
      : ThunderbringerWorker<git_repository*>(env),
        path_(std::move(path)),
        bare_(bare) {}

 protected:
  git_repository* DoExecute() override {
    git_repository* repo = nullptr;
    git_check(git_repository_init(&repo, path_.c_str(), bare_ ? 1 : 0));
    return repo;
  }

  Napi::Value GetResult(Napi::Env env, git_repository* repo) override {
    return GitRepository::NewInstance(env, repo);
  }

 private:
  std::string path_;
  bool bare_;
};

class DiscoverWorker : public ThunderbringerWorker<std::string> {
 public:
  DiscoverWorker(Napi::Env env, std::string start_path)
      : ThunderbringerWorker<std::string>(env),
        start_path_(std::move(start_path)) {}

 protected:
  std::string DoExecute() override {
    git_buf buf = GIT_BUF_INIT;
    git_check(git_repository_discover(&buf, start_path_.c_str(), 0, nullptr));
    std::string result(buf.ptr, buf.size);
    git_buf_dispose(&buf);
    return result;
  }

  Napi::Value GetResult(Napi::Env env, std::string result) override {
    return Napi::String::New(env, result);
  }

 private:
  std::string start_path_;
};

// ── ObjectWrap Implementation ─────────────────────────────────────

Napi::Object GitRepository::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Repository", {
    // Static methods
    StaticMethod<&GitRepository::Open>("open"),
    StaticMethod<&GitRepository::Init_>("init"),
    StaticMethod<&GitRepository::Discover>("discover"),

    // Instance methods
    InstanceMethod<&GitRepository::GetPath>("path"),
    InstanceMethod<&GitRepository::GetWorkdir>("workdir"),
    InstanceMethod<&GitRepository::IsBare>("isBare"),
    InstanceMethod<&GitRepository::IsEmpty>("isEmpty"),
    InstanceMethod<&GitRepository::GetState>("state"),
    InstanceMethod<&GitRepository::GetHead>("head"),
    InstanceMethod<&GitRepository::HeadDetached>("headDetached"),
    InstanceMethod<&GitRepository::HeadUnborn>("headUnborn"),
    InstanceMethod<&GitRepository::Free>("free"),
  });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();
  exports.Set("Repository", func);
  return exports;
}

Napi::Object GitRepository::NewInstance(Napi::Env env, git_repository* repo) {
  Napi::External<git_repository> ext = Napi::External<git_repository>::New(env, repo);
  return constructor_.New({ext});
}

GitRepository::GitRepository(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<GitRepository>(info), repo_(nullptr) {
  if (info.Length() > 0 && info[0].IsExternal()) {
    repo_ = info[0].As<Napi::External<git_repository>>().Data();
  }
}

GitRepository::~GitRepository() {
  if (repo_) {
    git_repository_free(repo_);
    repo_ = nullptr;
  }
}

// ── Static Methods ────────────────────────────────────────────────

Napi::Value GitRepository::Open(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected path string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* worker = new OpenWorker(env, info[0].As<Napi::String>().Utf8Value());
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

Napi::Value GitRepository::Init_(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected path string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  bool bare = false;
  if (info.Length() > 1 && info[1].IsBoolean()) {
    bare = info[1].As<Napi::Boolean>().Value();
  }

  auto* worker = new InitWorker(env, info[0].As<Napi::String>().Utf8Value(), bare);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

Napi::Value GitRepository::Clone(const Napi::CallbackInfo& info) {
  // Placeholder - will be implemented in Phase 5 with remote support
  Napi::Env env = info.Env();
  auto deferred = Napi::Promise::Deferred::New(env);
  deferred.Reject(Napi::Error::New(env, "Clone not yet implemented").Value());
  return deferred.Promise();
}

Napi::Value GitRepository::Discover(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected start path string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* worker = new DiscoverWorker(env, info[0].As<Napi::String>().Utf8Value());
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// ── Instance Methods ──────────────────────────────────────────────

Napi::Value GitRepository::GetPath(const Napi::CallbackInfo& info) {
  const char* path = git_repository_path(repo_);
  return Napi::String::New(info.Env(), path ? path : "");
}

Napi::Value GitRepository::GetWorkdir(const Napi::CallbackInfo& info) {
  const char* wd = git_repository_workdir(repo_);
  if (!wd) return info.Env().Null();
  return Napi::String::New(info.Env(), wd);
}

Napi::Value GitRepository::IsBare(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), git_repository_is_bare(repo_));
}

Napi::Value GitRepository::IsEmpty(const Napi::CallbackInfo& info) {
  int result = git_repository_is_empty(repo_);
  if (result < 0) {
    git_check_throw(info.Env(), result);
    return info.Env().Undefined();
  }
  return Napi::Boolean::New(info.Env(), result == 1);
}

Napi::Value GitRepository::GetState(const Napi::CallbackInfo& info) {
  int state = git_repository_state(repo_);
  return Napi::Number::New(info.Env(), state);
}

Napi::Value GitRepository::GetHead(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  git_reference* ref = nullptr;
  int error = git_repository_head(&ref, repo_);

  if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND) {
    return env.Null();
  }

  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto obj = Napi::Object::New(env);
  obj.Set("name", Napi::String::New(env, git_reference_name(ref)));

  const git_oid* oid = git_reference_target(ref);
  if (oid) {
    obj.Set("oid", Napi::String::New(env, oid_to_hex(oid)));
  }

  git_reference_free(ref);
  return obj;
}

Napi::Value GitRepository::HeadDetached(const Napi::CallbackInfo& info) {
  int result = git_repository_head_detached(repo_);
  return Napi::Boolean::New(info.Env(), result == 1);
}

Napi::Value GitRepository::HeadUnborn(const Napi::CallbackInfo& info) {
  int result = git_repository_head_unborn(repo_);
  return Napi::Boolean::New(info.Env(), result == 1);
}

void GitRepository::Free(const Napi::CallbackInfo& info) {
  if (repo_) {
    git_repository_free(repo_);
    repo_ = nullptr;
  }
}

void InitRepository(Napi::Env env, Napi::Object exports) {
  GitRepository::Init(env, exports);
}

}  // namespace thunderbringer
