#pragma once

#include <napi.h>
#include <git2.h>
#include <string>

namespace thunderbringer {

// Convert git_oid to hex string
inline std::string oid_to_hex(const git_oid* oid) {
  char hex[GIT_OID_SHA1_HEXSIZE + 1];
  git_oid_tostr(hex, sizeof(hex), oid);
  return std::string(hex);
}

// Parse hex string to git_oid
inline int hex_to_oid(git_oid* oid, const std::string& hex) {
  return git_oid_fromstr(oid, hex.c_str());
}

// Convert git_time to JS-compatible millisecond timestamp
inline double git_time_to_ms(git_time time) {
  return static_cast<double>(time.time) * 1000.0;
}

// Convert git_signature to JS object
inline Napi::Object signature_to_js(Napi::Env env, const git_signature* sig) {
  auto obj = Napi::Object::New(env);
  obj.Set("name", Napi::String::New(env, sig->name ? sig->name : ""));
  obj.Set("email", Napi::String::New(env, sig->email ? sig->email : ""));
  obj.Set("time", Napi::Number::New(env, git_time_to_ms(sig->when)));
  obj.Set("offset", Napi::Number::New(env, sig->when.offset));
  return obj;
}

// Create git_signature from JS object
inline git_signature* js_to_signature(Napi::Env env, Napi::Object obj) {
  std::string name = obj.Get("name").As<Napi::String>().Utf8Value();
  std::string email = obj.Get("email").As<Napi::String>().Utf8Value();

  git_signature* sig = nullptr;

  if (obj.Has("time")) {
    int64_t time = static_cast<int64_t>(
        obj.Get("time").As<Napi::Number>().DoubleValue() / 1000.0);
    int offset = 0;
    if (obj.Has("offset")) {
      offset = obj.Get("offset").As<Napi::Number>().Int32Value();
    }
    git_signature_new(&sig, name.c_str(), email.c_str(), time, offset);
  } else {
    git_signature_now(&sig, name.c_str(), email.c_str());
  }

  return sig;
}

// Get string from JS value, or empty string if undefined/null
inline std::string get_string_or_empty(Napi::Value val) {
  if (val.IsUndefined() || val.IsNull()) return "";
  return val.As<Napi::String>().Utf8Value();
}

// Get optional string argument
inline std::string get_optional_string(const Napi::CallbackInfo& info, size_t idx,
                                       const std::string& default_val = "") {
  if (idx >= info.Length() || info[idx].IsUndefined() || info[idx].IsNull()) {
    return default_val;
  }
  return info[idx].As<Napi::String>().Utf8Value();
}

}  // namespace thunderbringer
