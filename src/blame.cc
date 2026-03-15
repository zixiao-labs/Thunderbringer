#include "blame.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

struct BlameHunkData {
  size_t lines_in_hunk;
  std::string final_commit_id;
  size_t final_start_line_number;
  std::string final_signature_name;
  std::string final_signature_email;
  std::string orig_commit_id;
  std::string orig_path;
  size_t orig_start_line_number;
  std::string orig_signature_name;
  std::string orig_signature_email;
  bool boundary;
};

struct BlameResult {
  std::vector<BlameHunkData> hunks;
};

class BlameFileWorker : public ThunderbringerWorker<BlameResult> {
 public:
  BlameFileWorker(Napi::Env env, git_repository* repo, std::string path)
      : ThunderbringerWorker<BlameResult>(env), repo_(repo), path_(std::move(path)) {}

 protected:
  BlameResult DoExecute() override {
    BlameResult result;

    git_blame_options opts = GIT_BLAME_OPTIONS_INIT;
    git_blame* blame = nullptr;
    git_check(git_blame_file(&blame, repo_, path_.c_str(), &opts));
    GitBlameGuard guard(blame);

    uint32_t hunk_count = git_blame_get_hunk_count(blame);
    for (uint32_t i = 0; i < hunk_count; i++) {
      const git_blame_hunk* hunk = git_blame_get_hunk_byindex(blame, i);
      if (!hunk) continue;

      BlameHunkData hd;
      hd.lines_in_hunk = hunk->lines_in_hunk;
      hd.final_commit_id = oid_to_hex(&hunk->final_commit_id);
      hd.final_start_line_number = hunk->final_start_line_number;
      hd.orig_commit_id = oid_to_hex(&hunk->orig_commit_id);
      hd.orig_path = hunk->orig_path ? hunk->orig_path : "";
      hd.orig_start_line_number = hunk->orig_start_line_number;
      hd.boundary = hunk->boundary != 0;

      if (hunk->final_signature) {
        hd.final_signature_name = hunk->final_signature->name ? hunk->final_signature->name : "";
        hd.final_signature_email = hunk->final_signature->email ? hunk->final_signature->email : "";
      }
      if (hunk->orig_signature) {
        hd.orig_signature_name = hunk->orig_signature->name ? hunk->orig_signature->name : "";
        hd.orig_signature_email = hunk->orig_signature->email ? hunk->orig_signature->email : "";
      }

      result.hunks.push_back(std::move(hd));
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, BlameResult result) override {
    auto arr = Napi::Array::New(env, result.hunks.size());
    for (size_t i = 0; i < result.hunks.size(); i++) {
      auto& hd = result.hunks[i];
      auto obj = Napi::Object::New(env);
      obj.Set("linesInHunk", Napi::Number::New(env, static_cast<double>(hd.lines_in_hunk)));
      obj.Set("finalCommitId", Napi::String::New(env, hd.final_commit_id));
      obj.Set("finalStartLineNumber", Napi::Number::New(env, static_cast<double>(hd.final_start_line_number)));
      obj.Set("origCommitId", Napi::String::New(env, hd.orig_commit_id));
      obj.Set("origPath", Napi::String::New(env, hd.orig_path));
      obj.Set("origStartLineNumber", Napi::Number::New(env, static_cast<double>(hd.orig_start_line_number)));
      obj.Set("boundary", Napi::Boolean::New(env, hd.boundary));

      auto finalSig = Napi::Object::New(env);
      finalSig.Set("name", Napi::String::New(env, hd.final_signature_name));
      finalSig.Set("email", Napi::String::New(env, hd.final_signature_email));
      obj.Set("finalSignature", finalSig);

      auto origSig = Napi::Object::New(env);
      origSig.Set("name", Napi::String::New(env, hd.orig_signature_name));
      origSig.Set("email", Napi::String::New(env, hd.orig_signature_email));
      obj.Set("origSignature", origSig);

      arr.Set(static_cast<uint32_t>(i), obj);
    }
    return arr;
  }

 private:
  git_repository* repo_;
  std::string path_;
};

// Blame.file(repo, path) → Promise<BlameHunk[]>
static Napi::Value File(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, path)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string path = info[1].As<Napi::String>().Utf8Value();

  auto* worker = new BlameFileWorker(env, repo_wrap->GetRepo(), path);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Blame.getHunkByLine(repo, path, line) → Promise<BlameHunk>
static Napi::Value GetHunkByLine(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsNumber()) {
    Napi::TypeError::New(env, "Expected (Repository, path, lineNumber)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string path = info[1].As<Napi::String>().Utf8Value();
  uint32_t line = info[2].As<Napi::Number>().Uint32Value();

  git_blame_options opts = GIT_BLAME_OPTIONS_INIT;
  git_blame* blame = nullptr;
  int error = git_blame_file(&blame, repo_wrap->GetRepo(), path.c_str(), &opts);
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }
  GitBlameGuard guard(blame);

  const git_blame_hunk* hunk = git_blame_get_hunk_byline(blame, line);
  if (!hunk) return env.Null();

  auto obj = Napi::Object::New(env);
  obj.Set("linesInHunk", Napi::Number::New(env, static_cast<double>(hunk->lines_in_hunk)));
  obj.Set("finalCommitId", Napi::String::New(env, oid_to_hex(&hunk->final_commit_id)));
  obj.Set("finalStartLineNumber", Napi::Number::New(env, static_cast<double>(hunk->final_start_line_number)));
  obj.Set("origCommitId", Napi::String::New(env, oid_to_hex(&hunk->orig_commit_id)));
  obj.Set("origPath", Napi::String::New(env, hunk->orig_path ? hunk->orig_path : ""));
  obj.Set("origStartLineNumber", Napi::Number::New(env, static_cast<double>(hunk->orig_start_line_number)));
  obj.Set("boundary", Napi::Boolean::New(env, hunk->boundary != 0));

  if (hunk->final_signature) {
    auto sig = Napi::Object::New(env);
    sig.Set("name", Napi::String::New(env, hunk->final_signature->name ? hunk->final_signature->name : ""));
    sig.Set("email", Napi::String::New(env, hunk->final_signature->email ? hunk->final_signature->email : ""));
    obj.Set("finalSignature", sig);
  }

  if (hunk->orig_signature) {
    auto sig = Napi::Object::New(env);
    sig.Set("name", Napi::String::New(env, hunk->orig_signature->name ? hunk->orig_signature->name : ""));
    sig.Set("email", Napi::String::New(env, hunk->orig_signature->email ? hunk->orig_signature->email : ""));
    obj.Set("origSignature", sig);
  }

  return obj;
}

void InitBlame(Napi::Env env, Napi::Object exports) {
  auto blame = Napi::Object::New(env);
  blame.Set("file", Napi::Function::New(env, File, "file"));
  blame.Set("getHunkByLine", Napi::Function::New(env, GetHunkByLine, "getHunkByLine"));
  exports.Set("Blame", blame);
}

}  // namespace thunderbringer
