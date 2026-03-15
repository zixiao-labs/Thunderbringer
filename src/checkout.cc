#include "checkout.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

class CheckoutHeadWorker : public ThunderbringerWorker<void> {
 public:
  CheckoutHeadWorker(Napi::Env env, git_repository* repo, unsigned int strategy)
      : ThunderbringerWorker<void>(env), repo_(repo), strategy_(strategy) {}

 protected:
  void DoExecute() override {
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = strategy_;
    git_check(git_checkout_head(repo_, &opts));
  }

 private:
  git_repository* repo_;
  unsigned int strategy_;
};

// Checkout.head(repo, opts?) → Promise<void>
static Napi::Value Head(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  unsigned int strategy = GIT_CHECKOUT_SAFE;
  if (info.Length() > 1 && info[1].IsObject()) {
    auto opts = info[1].As<Napi::Object>();
    if (opts.Has("strategy")) {
      strategy = opts.Get("strategy").As<Napi::Number>().Uint32Value();
    }
  }

  auto* worker = new CheckoutHeadWorker(env, repo_wrap->GetRepo(), strategy);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

class CheckoutBranchWorker : public ThunderbringerWorker<void> {
 public:
  CheckoutBranchWorker(Napi::Env env, git_repository* repo,
                       std::string branch_name, unsigned int strategy)
      : ThunderbringerWorker<void>(env), repo_(repo),
        branch_name_(std::move(branch_name)), strategy_(strategy) {}

 protected:
  void DoExecute() override {
    // Lookup the branch reference
    git_reference* ref = nullptr;
    git_check(git_branch_lookup(&ref, repo_, branch_name_.c_str(), GIT_BRANCH_LOCAL));
    GitReferenceGuard ref_guard(ref);

    // Get the target commit
    const git_oid* oid = git_reference_target(ref);
    if (!oid) throw GitException(-1, "Branch has no target");

    git_object* target = nullptr;
    git_check(git_object_lookup(&target, repo_, oid, GIT_OBJECT_COMMIT));
    GitObjectGuard target_guard(target);

    // Checkout the tree
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = strategy_;
    git_check(git_checkout_tree(repo_, target, &opts));

    // Update HEAD
    std::string ref_name = "refs/heads/" + branch_name_;
    git_check(git_repository_set_head(repo_, ref_name.c_str()));
  }

 private:
  git_repository* repo_;
  std::string branch_name_;
  unsigned int strategy_;
};

// Checkout.branch(repo, branchName, opts?) → Promise<void>
static Napi::Value Branch(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, branchName)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string branch_name = info[1].As<Napi::String>().Utf8Value();
  unsigned int strategy = GIT_CHECKOUT_SAFE;
  if (info.Length() > 2 && info[2].IsObject()) {
    auto opts = info[2].As<Napi::Object>();
    if (opts.Has("strategy")) {
      strategy = opts.Get("strategy").As<Napi::Number>().Uint32Value();
    }
  }

  auto* worker = new CheckoutBranchWorker(env, repo_wrap->GetRepo(), branch_name, strategy);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

class CheckoutReferenceWorker : public ThunderbringerWorker<void> {
 public:
  CheckoutReferenceWorker(Napi::Env env, git_repository* repo,
                          std::string ref_name, unsigned int strategy)
      : ThunderbringerWorker<void>(env), repo_(repo),
        ref_name_(std::move(ref_name)), strategy_(strategy) {}

 protected:
  void DoExecute() override {
    git_reference* ref = nullptr;
    git_check(git_reference_lookup(&ref, repo_, ref_name_.c_str()));
    GitReferenceGuard ref_guard(ref);

    // Resolve to direct reference
    git_reference* resolved = nullptr;
    git_check(git_reference_resolve(&resolved, ref));
    GitReferenceGuard resolved_guard(resolved);

    const git_oid* oid = git_reference_target(resolved);
    if (!oid) throw GitException(-1, "Reference has no target");

    git_object* target = nullptr;
    git_check(git_object_lookup(&target, repo_, oid, GIT_OBJECT_COMMIT));
    GitObjectGuard target_guard(target);

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = strategy_;
    git_check(git_checkout_tree(repo_, target, &opts));
    git_check(git_repository_set_head(repo_, ref_name_.c_str()));
  }

 private:
  git_repository* repo_;
  std::string ref_name_;
  unsigned int strategy_;
};

// Checkout.reference(repo, refName, opts?) → Promise<void>
static Napi::Value Reference(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, refName)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string ref_name = info[1].As<Napi::String>().Utf8Value();
  unsigned int strategy = GIT_CHECKOUT_SAFE;
  if (info.Length() > 2 && info[2].IsObject()) {
    auto opts = info[2].As<Napi::Object>();
    if (opts.Has("strategy")) {
      strategy = opts.Get("strategy").As<Napi::Number>().Uint32Value();
    }
  }

  auto* worker = new CheckoutReferenceWorker(env, repo_wrap->GetRepo(), ref_name, strategy);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

void InitCheckout(Napi::Env env, Napi::Object exports) {
  auto checkout = Napi::Object::New(env);
  checkout.Set("head", Napi::Function::New(env, Head, "head"));
  checkout.Set("branch", Napi::Function::New(env, Branch, "branch"));
  checkout.Set("reference", Napi::Function::New(env, Reference, "reference"));

  // Strategy constants
  checkout.Set("STRATEGY_NONE", Napi::Number::New(env, GIT_CHECKOUT_NONE));
  checkout.Set("STRATEGY_SAFE", Napi::Number::New(env, GIT_CHECKOUT_SAFE));
  checkout.Set("STRATEGY_FORCE", Napi::Number::New(env, GIT_CHECKOUT_FORCE));

  exports.Set("Checkout", checkout);
}

}  // namespace thunderbringer
