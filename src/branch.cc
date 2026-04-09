#include "branch.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/util.h"

namespace thunderbringer {

// Branch.create(repo, name, targetOid, force?) → { name, oid }
static Napi::Value Create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name, targetOid, force?)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  std::string target_str = info[2].As<Napi::String>().Utf8Value();
  bool force = info.Length() > 3 && info[3].IsBoolean() && info[3].As<Napi::Boolean>().Value();

  git_oid target_oid;
  git_check_throw(env, hex_to_oid(&target_oid, target_str));

  git_commit* commit = nullptr;
  int error = git_commit_lookup(&commit, repo_wrap->GetRepo(), &target_oid);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitCommitGuard commit_guard(commit);

  git_reference* ref = nullptr;
  error = git_branch_create(&ref, repo_wrap->GetRepo(), name.c_str(), commit, force ? 1 : 0);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitReferenceGuard ref_guard(ref);

  auto result = Napi::Object::New(env);
  result.Set("name", Napi::String::New(env, git_reference_name(ref)));
  const git_oid* oid = git_reference_target(ref);
  if (oid) result.Set("oid", Napi::String::New(env, oid_to_hex(oid)));
  return result;
}

// Branch.delete(repo, name, type?) → void
static Napi::Value Delete(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  git_branch_t type = GIT_BRANCH_LOCAL;
  if (info.Length() > 2 && info[2].IsNumber()) {
    type = static_cast<git_branch_t>(info[2].As<Napi::Number>().Int32Value());
  }

  git_reference* ref = nullptr;
  int error = git_branch_lookup(&ref, repo_wrap->GetRepo(), name.c_str(), type);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  error = git_branch_delete(ref);
  git_reference_free(ref);
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Branch.list(repo, type?) → [{ name, oid, isHead }]
static Napi::Value List(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  git_branch_t filter = GIT_BRANCH_ALL;
  if (info.Length() > 1 && info[1].IsNumber()) {
    filter = static_cast<git_branch_t>(info[1].As<Napi::Number>().Int32Value());
  }

  git_branch_iterator* iter = nullptr;
  int error = git_branch_iterator_new(&iter, repo_wrap->GetRepo(), filter);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto arr = Napi::Array::New(env);
  uint32_t idx = 0;
  git_reference* ref = nullptr;
  git_branch_t type;

  while (git_branch_next(&ref, &type, iter) == 0) {
    auto obj = Napi::Object::New(env);

    const char* branch_name = nullptr;
    git_branch_name(&branch_name, ref);
    obj.Set("name", Napi::String::New(env, branch_name ? branch_name : ""));
    obj.Set("fullName", Napi::String::New(env, git_reference_name(ref)));
    obj.Set("isLocal", Napi::Boolean::New(env, type == GIT_BRANCH_LOCAL));
    obj.Set("isRemote", Napi::Boolean::New(env, type == GIT_BRANCH_REMOTE));
    obj.Set("isHead", Napi::Boolean::New(env, git_branch_is_head(ref) == 1));

    const git_oid* oid = git_reference_target(ref);
    if (oid) obj.Set("oid", Napi::String::New(env, oid_to_hex(oid)));

    arr.Set(idx++, obj);
    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);
  return arr;
}

// Branch.rename(repo, oldName, newName, force?) → { name, oid }
static Napi::Value Rename(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, oldName, newName, force?)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string old_name = info[1].As<Napi::String>().Utf8Value();
  std::string new_name = info[2].As<Napi::String>().Utf8Value();
  bool force = info.Length() > 3 && info[3].IsBoolean() && info[3].As<Napi::Boolean>().Value();

  git_reference* ref = nullptr;
  int error = git_branch_lookup(&ref, repo_wrap->GetRepo(), old_name.c_str(), GIT_BRANCH_LOCAL);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  git_reference* new_ref = nullptr;
  error = git_branch_move(&new_ref, ref, new_name.c_str(), force ? 1 : 0);
  git_reference_free(ref);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitReferenceGuard guard(new_ref);

  auto result = Napi::Object::New(env);
  result.Set("name", Napi::String::New(env, git_reference_name(new_ref)));
  const git_oid* oid = git_reference_target(new_ref);
  if (oid) result.Set("oid", Napi::String::New(env, oid_to_hex(oid)));
  return result;
}

// Branch.lookup(repo, name, type?) → { name, oid, isHead }
static Napi::Value Lookup(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  git_branch_t type = GIT_BRANCH_LOCAL;
  if (info.Length() > 2 && info[2].IsNumber()) {
    type = static_cast<git_branch_t>(info[2].As<Napi::Number>().Int32Value());
  }

  git_reference* ref = nullptr;
  int error = git_branch_lookup(&ref, repo_wrap->GetRepo(), name.c_str(), type);
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitReferenceGuard guard(ref);

  auto result = Napi::Object::New(env);
  const char* branch_name = nullptr;
  git_branch_name(&branch_name, ref);
  result.Set("name", Napi::String::New(env, branch_name ? branch_name : ""));
  result.Set("fullName", Napi::String::New(env, git_reference_name(ref)));
  result.Set("isHead", Napi::Boolean::New(env, git_branch_is_head(ref) == 1));

  const git_oid* oid = git_reference_target(ref);
  if (oid) result.Set("oid", Napi::String::New(env, oid_to_hex(oid)));

  return result;
}

// Branch.current(repo) → { name, oid } or null
static Napi::Value Current(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_reference* head = nullptr;
  int error = git_repository_head(&head, repo_wrap->GetRepo());
  if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND) {
    return env.Null();
  }
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitReferenceGuard guard(head);

  if (!git_reference_is_branch(head)) {
    // Detached HEAD
    auto result = Napi::Object::New(env);
    result.Set("name", env.Null());
    result.Set("detached", Napi::Boolean::New(env, true));
    const git_oid* oid = git_reference_target(head);
    if (oid) result.Set("oid", Napi::String::New(env, oid_to_hex(oid)));
    return result;
  }

  auto result = Napi::Object::New(env);
  const char* branch_name = nullptr;
  git_branch_name(&branch_name, head);
  result.Set("name", Napi::String::New(env, branch_name ? branch_name : ""));
  result.Set("fullName", Napi::String::New(env, git_reference_name(head)));
  result.Set("detached", Napi::Boolean::New(env, false));

  const git_oid* oid = git_reference_target(head);
  if (oid) result.Set("oid", Napi::String::New(env, oid_to_hex(oid)));

  return result;
}

void InitBranch(Napi::Env env, Napi::Object exports) {
  auto branch = Napi::Object::New(env);
  branch.Set("create", Napi::Function::New(env, Create, "create"));
  branch.Set("_delete", Napi::Function::New(env, Delete, "_delete"));
  branch.Set("list", Napi::Function::New(env, List, "list"));
  branch.Set("rename", Napi::Function::New(env, Rename, "rename"));
  branch.Set("lookup", Napi::Function::New(env, Lookup, "lookup"));
  branch.Set("current", Napi::Function::New(env, Current, "current"));

  // Branch type constants
  branch.Set("LOCAL", Napi::Number::New(env, GIT_BRANCH_LOCAL));
  branch.Set("REMOTE", Napi::Number::New(env, GIT_BRANCH_REMOTE));
  branch.Set("ALL", Napi::Number::New(env, GIT_BRANCH_ALL));

  exports.Set("Branch", branch);
}

}  // namespace thunderbringer
