#include "reference.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

static Napi::Object ReferenceToJS(Napi::Env env, git_reference* ref) {
  auto obj = Napi::Object::New(env);
  obj.Set("name", Napi::String::New(env, git_reference_name(ref)));
  obj.Set("isBranch", Napi::Boolean::New(env, git_reference_is_branch(ref)));
  obj.Set("isRemote", Napi::Boolean::New(env, git_reference_is_remote(ref)));
  obj.Set("isTag", Napi::Boolean::New(env, git_reference_is_tag(ref)));
  obj.Set("isNote", Napi::Boolean::New(env, git_reference_is_note(ref)));

  git_reference_t type = git_reference_type(ref);
  obj.Set("type", Napi::Number::New(env, type));

  if (type == GIT_REFERENCE_DIRECT) {
    const git_oid* oid = git_reference_target(ref);
    if (oid) obj.Set("target", Napi::String::New(env, oid_to_hex(oid)));
  } else if (type == GIT_REFERENCE_SYMBOLIC) {
    const char* target = git_reference_symbolic_target(ref);
    if (target) obj.Set("symbolicTarget", Napi::String::New(env, target));
  }

  // Shorthand name
  const char* shorthand = git_reference_shorthand(ref);
  if (shorthand) obj.Set("shorthand", Napi::String::New(env, shorthand));

  return obj;
}

// Reference.lookup(repo, name) → reference info object
static Napi::Value Lookup(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();

  git_reference* ref = nullptr;
  int error = git_reference_lookup(&ref, repo_wrap->GetRepo(), name.c_str());
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  GitReferenceGuard guard(ref);
  return ReferenceToJS(env, ref);
}

// Reference.resolve(repo, name) → resolved reference info (follows symbolic refs)
static Napi::Value Resolve(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();

  git_reference* ref = nullptr;
  int error = git_reference_lookup(&ref, repo_wrap->GetRepo(), name.c_str());
  if (error == GIT_ENOTFOUND) return env.Null();
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitReferenceGuard ref_guard(ref);

  git_reference* resolved = nullptr;
  error = git_reference_resolve(&resolved, ref);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitReferenceGuard resolved_guard(resolved);

  return ReferenceToJS(env, resolved);
}

// Reference.list(repo) → string[]
static Napi::Value List(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_strarray ref_list = {nullptr, 0};
  int error = git_reference_list(&ref_list, repo_wrap->GetRepo());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto arr = Napi::Array::New(env, ref_list.count);
  for (size_t i = 0; i < ref_list.count; i++) {
    arr.Set(static_cast<uint32_t>(i), Napi::String::New(env, ref_list.strings[i]));
  }
  git_strarray_dispose(&ref_list);

  return arr;
}

// Reference.create(repo, name, target, force?) → reference info
static Napi::Value Create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name, target, force?)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  std::string target = info[2].As<Napi::String>().Utf8Value();
  bool force = info.Length() > 3 && info[3].IsBoolean() && info[3].As<Napi::Boolean>().Value();

  git_oid oid;
  int error = hex_to_oid(&oid, target);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  git_reference* ref = nullptr;
  error = git_reference_create(&ref, repo_wrap->GetRepo(), name.c_str(), &oid, force ? 1 : 0, nullptr);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  GitReferenceGuard guard(ref);
  return ReferenceToJS(env, ref);
}

// Reference.createSymbolic(repo, name, target, force?) → reference info
static Napi::Value CreateSymbolic(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name, target, force?)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  std::string target = info[2].As<Napi::String>().Utf8Value();
  bool force = info.Length() > 3 && info[3].IsBoolean() && info[3].As<Napi::Boolean>().Value();

  git_reference* ref = nullptr;
  int error = git_reference_symbolic_create(&ref, repo_wrap->GetRepo(), name.c_str(), target.c_str(), force ? 1 : 0, nullptr);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  GitReferenceGuard guard(ref);
  return ReferenceToJS(env, ref);
}

// Reference.delete(repo, name)
static Napi::Value Delete(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();

  git_reference* ref = nullptr;
  int error = git_reference_lookup(&ref, repo_wrap->GetRepo(), name.c_str());
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  error = git_reference_delete(ref);
  git_reference_free(ref);

  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

void InitReference(Napi::Env env, Napi::Object exports) {
  auto reference = Napi::Object::New(env);
  reference.Set("lookup", Napi::Function::New(env, Lookup, "lookup"));
  reference.Set("resolve", Napi::Function::New(env, Resolve, "resolve"));
  reference.Set("list", Napi::Function::New(env, List, "list"));
  reference.Set("create", Napi::Function::New(env, Create, "create"));
  reference.Set("createSymbolic", Napi::Function::New(env, CreateSymbolic, "createSymbolic"));
  reference.Set("_delete", Napi::Function::New(env, Delete, "_delete"));

  // Reference type constants
  reference.Set("TYPE_INVALID", Napi::Number::New(env, GIT_REFERENCE_INVALID));
  reference.Set("TYPE_DIRECT", Napi::Number::New(env, GIT_REFERENCE_DIRECT));
  reference.Set("TYPE_SYMBOLIC", Napi::Number::New(env, GIT_REFERENCE_SYMBOLIC));

  exports.Set("Reference", reference);
}

}  // namespace thunderbringer
