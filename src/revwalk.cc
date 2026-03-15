#include "revwalk.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

struct RevwalkResult {
  std::vector<std::string> oids;
};

class RevwalkWorker : public ThunderbringerWorker<RevwalkResult> {
 public:
  RevwalkWorker(Napi::Env env, git_repository* repo,
                std::string start_oid, unsigned int sort_mode, int max_count)
      : ThunderbringerWorker<RevwalkResult>(env),
        repo_(repo), start_oid_(std::move(start_oid)),
        sort_mode_(sort_mode), max_count_(max_count) {}

 protected:
  RevwalkResult DoExecute() override {
    RevwalkResult result;

    git_revwalk* walk = nullptr;
    git_check(git_revwalk_new(&walk, repo_));
    GitRevwalkGuard guard(walk);

    git_revwalk_sorting(walk, sort_mode_);

    if (start_oid_ == "HEAD") {
      git_check(git_revwalk_push_head(walk));
    } else {
      git_oid oid;
      git_check(hex_to_oid(&oid, start_oid_));
      git_check(git_revwalk_push(walk, &oid));
    }

    git_oid oid;
    int count = 0;
    while (git_revwalk_next(&oid, walk) == 0) {
      result.oids.push_back(oid_to_hex(&oid));
      count++;
      if (max_count_ > 0 && count >= max_count_) break;
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, RevwalkResult result) override {
    auto arr = Napi::Array::New(env, result.oids.size());
    for (size_t i = 0; i < result.oids.size(); i++) {
      arr.Set(static_cast<uint32_t>(i), Napi::String::New(env, result.oids[i]));
    }
    return arr;
  }

 private:
  git_repository* repo_;
  std::string start_oid_;
  unsigned int sort_mode_;
  int max_count_;
};

// Revwalk.walk(repo, { start, sort, maxCount }) → Promise<string[]>
static Napi::Value Walk(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  std::string start = "HEAD";
  unsigned int sort = GIT_SORT_TIME;
  int max_count = 0;

  if (info.Length() > 1 && info[1].IsObject()) {
    auto opts = info[1].As<Napi::Object>();
    if (opts.Has("start") && opts.Get("start").IsString())
      start = opts.Get("start").As<Napi::String>().Utf8Value();
    if (opts.Has("sort") && opts.Get("sort").IsNumber())
      sort = opts.Get("sort").As<Napi::Number>().Uint32Value();
    if (opts.Has("maxCount") && opts.Get("maxCount").IsNumber())
      max_count = opts.Get("maxCount").As<Napi::Number>().Int32Value();
  }

  auto* worker = new RevwalkWorker(env, repo_wrap->GetRepo(), start, sort, max_count);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

void InitRevwalk(Napi::Env env, Napi::Object exports) {
  auto revwalk = Napi::Object::New(env);
  revwalk.Set("walk", Napi::Function::New(env, Walk, "walk"));

  // Sort constants
  revwalk.Set("SORT_NONE", Napi::Number::New(env, GIT_SORT_NONE));
  revwalk.Set("SORT_TOPOLOGICAL", Napi::Number::New(env, GIT_SORT_TOPOLOGICAL));
  revwalk.Set("SORT_TIME", Napi::Number::New(env, GIT_SORT_TIME));
  revwalk.Set("SORT_REVERSE", Napi::Number::New(env, GIT_SORT_REVERSE));

  exports.Set("Revwalk", revwalk);
}

}  // namespace thunderbringer
