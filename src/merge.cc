#include "merge.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

struct MergeAnalysisResult {
  int analysis;
  int preference;
};

class MergeAnalysisWorker : public ThunderbringerWorker<MergeAnalysisResult> {
 public:
  MergeAnalysisWorker(Napi::Env env, git_repository* repo, std::string their_oid)
      : ThunderbringerWorker<MergeAnalysisResult>(env),
        repo_(repo), their_oid_(std::move(their_oid)) {}

 protected:
  MergeAnalysisResult DoExecute() override {
    git_oid oid;
    git_check(hex_to_oid(&oid, their_oid_));

    git_annotated_commit* annotated = nullptr;
    git_check(git_annotated_commit_lookup(&annotated, repo_, &oid));
    GitAnnotatedCommitGuard guard(annotated);

    const git_annotated_commit* heads[] = {annotated};
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    git_check(git_merge_analysis(&analysis, &preference, repo_, heads, 1));

    return MergeAnalysisResult{static_cast<int>(analysis), static_cast<int>(preference)};
  }

  Napi::Value GetResult(Napi::Env env, MergeAnalysisResult result) override {
    auto obj = Napi::Object::New(env);
    obj.Set("analysis", Napi::Number::New(env, result.analysis));
    obj.Set("preference", Napi::Number::New(env, result.preference));
    obj.Set("isUpToDate", Napi::Boolean::New(env, (result.analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) != 0));
    obj.Set("isFastForward", Napi::Boolean::New(env, (result.analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) != 0));
    obj.Set("isNormal", Napi::Boolean::New(env, (result.analysis & GIT_MERGE_ANALYSIS_NORMAL) != 0));
    return obj;
  }

 private:
  git_repository* repo_;
  std::string their_oid_;
};

// Merge.analysis(repo, theirOid) → Promise<MergeAnalysisResult>
static Napi::Value Analysis(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, theirOid)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string their_oid = info[1].As<Napi::String>().Utf8Value();

  auto* worker = new MergeAnalysisWorker(env, repo_wrap->GetRepo(), their_oid);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

class MergeCommitsWorker : public ThunderbringerWorker<void> {
 public:
  MergeCommitsWorker(Napi::Env env, git_repository* repo, std::string their_oid)
      : ThunderbringerWorker<void>(env),
        repo_(repo), their_oid_(std::move(their_oid)) {}

 protected:
  void DoExecute() override {
    git_oid oid;
    git_check(hex_to_oid(&oid, their_oid_));

    git_annotated_commit* annotated = nullptr;
    git_check(git_annotated_commit_lookup(&annotated, repo_, &oid));
    GitAnnotatedCommitGuard guard(annotated);

    const git_annotated_commit* heads[] = {annotated};
    git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    git_check(git_merge(repo_, heads, 1, &merge_opts, &checkout_opts));
  }

 private:
  git_repository* repo_;
  std::string their_oid_;
};

// Merge.merge(repo, theirOid) → Promise<void>
static Napi::Value MergeFn(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, theirOid)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string their_oid = info[1].As<Napi::String>().Utf8Value();

  auto* worker = new MergeCommitsWorker(env, repo_wrap->GetRepo(), their_oid);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Merge.base(repo, oidOne, oidTwo) → string (oid) or null
static Napi::Value Base(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, oid1, oid2)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_oid oid1, oid2, base;
  git_check_throw(env, hex_to_oid(&oid1, info[1].As<Napi::String>().Utf8Value()));
  git_check_throw(env, hex_to_oid(&oid2, info[2].As<Napi::String>().Utf8Value()));

  int error = git_merge_base(&base, repo_wrap->GetRepo(), &oid1, &oid2);
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::String::New(env, oid_to_hex(&base));
}

void InitMerge(Napi::Env env, Napi::Object exports) {
  auto merge = Napi::Object::New(env);
  merge.Set("analysis", Napi::Function::New(env, Analysis, "analysis"));
  merge.Set("merge", Napi::Function::New(env, MergeFn, "merge"));
  merge.Set("base", Napi::Function::New(env, Base, "base"));

  // Analysis constants
  merge.Set("ANALYSIS_NONE", Napi::Number::New(env, GIT_MERGE_ANALYSIS_NONE));
  merge.Set("ANALYSIS_NORMAL", Napi::Number::New(env, GIT_MERGE_ANALYSIS_NORMAL));
  merge.Set("ANALYSIS_UP_TO_DATE", Napi::Number::New(env, GIT_MERGE_ANALYSIS_UP_TO_DATE));
  merge.Set("ANALYSIS_FASTFORWARD", Napi::Number::New(env, GIT_MERGE_ANALYSIS_FASTFORWARD));
  merge.Set("ANALYSIS_UNBORN", Napi::Number::New(env, GIT_MERGE_ANALYSIS_UNBORN));

  exports.Set("Merge", merge);
}

}  // namespace thunderbringer
