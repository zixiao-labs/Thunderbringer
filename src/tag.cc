#include "tag.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/util.h"

namespace thunderbringer {

// Tag.create(repo, name, targetOid, tagger, message) → oid
static Napi::Value Create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 5 || !info[0].IsObject() || !info[1].IsString() ||
      !info[2].IsString() || !info[3].IsObject() || !info[4].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name, targetOid, tagger, message)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();
  std::string target_str = info[2].As<Napi::String>().Utf8Value();
  std::string message = info[4].As<Napi::String>().Utf8Value();

  git_signature* tagger = js_to_signature(env, info[3].As<Napi::Object>());
  GitSignatureGuard tagger_guard(tagger);

  git_oid target_oid;
  git_check_throw(env, hex_to_oid(&target_oid, target_str));

  git_object* target = nullptr;
  int error = git_object_lookup(&target, repo_wrap->GetRepo(), &target_oid, GIT_OBJECT_ANY);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitObjectGuard target_guard(target);

  git_oid tag_oid;
  error = git_tag_create(&tag_oid, repo_wrap->GetRepo(), name.c_str(),
                         target, tagger, message.c_str(), 0);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::String::New(env, oid_to_hex(&tag_oid));
}

// Tag.createLightweight(repo, name, targetOid, force?) → oid
static Napi::Value CreateLightweight(const Napi::CallbackInfo& info) {
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

  git_object* target = nullptr;
  int error = git_object_lookup(&target, repo_wrap->GetRepo(), &target_oid, GIT_OBJECT_ANY);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitObjectGuard target_guard(target);

  git_oid tag_oid;
  error = git_tag_create_lightweight(&tag_oid, repo_wrap->GetRepo(), name.c_str(),
                                     target, force ? 1 : 0);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::String::New(env, oid_to_hex(&tag_oid));
}

// Tag.delete(repo, name) → void
static Napi::Value Delete(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, name)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string name = info[1].As<Napi::String>().Utf8Value();

  int error = git_tag_delete(repo_wrap->GetRepo(), name.c_str());
  if (error < 0) {
    git_check_throw(env, error);
  }
  return env.Undefined();
}

// Tag.list(repo, pattern?) → string[]
static Napi::Value ListTags(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());

  git_strarray tags = {nullptr, 0};
  int error;

  if (info.Length() > 1 && info[1].IsString()) {
    std::string pattern = info[1].As<Napi::String>().Utf8Value();
    error = git_tag_list_match(&tags, pattern.c_str(), repo_wrap->GetRepo());
  } else {
    error = git_tag_list(&tags, repo_wrap->GetRepo());
  }

  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  auto arr = Napi::Array::New(env, tags.count);
  for (size_t i = 0; i < tags.count; i++) {
    arr.Set(static_cast<uint32_t>(i), Napi::String::New(env, tags.strings[i]));
  }
  git_strarray_dispose(&tags);

  return arr;
}

void InitTag(Napi::Env env, Napi::Object exports) {
  auto tag = Napi::Object::New(env);
  tag.Set("create", Napi::Function::New(env, Create, "create"));
  tag.Set("createLightweight", Napi::Function::New(env, CreateLightweight, "createLightweight"));
  tag.Set("delete", Napi::Function::New(env, Delete, "delete"));
  tag.Set("list", Napi::Function::New(env, ListTags, "list"));
  exports.Set("Tag", tag);
}

}  // namespace thunderbringer
