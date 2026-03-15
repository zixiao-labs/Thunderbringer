#include "patch.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

struct HunkData {
  std::string header;
  int old_start;
  int old_lines;
  int new_start;
  int new_lines;
  struct LineData {
    int origin;
    std::string content;
    int old_lineno;
    int new_lineno;
  };
  std::vector<LineData> lines;
};

struct PatchResult {
  std::string old_path;
  std::string new_path;
  size_t adds;
  size_t dels;
  size_t context;
  std::vector<HunkData> hunks;
  std::string text;
};

class PatchFromDiffWorker : public ThunderbringerWorker<std::vector<PatchResult>> {
 public:
  PatchFromDiffWorker(Napi::Env env, git_repository* repo, bool index_to_workdir,
                      std::string old_tree_oid, std::string new_tree_oid)
      : ThunderbringerWorker<std::vector<PatchResult>>(env), repo_(repo),
        index_to_workdir_(index_to_workdir),
        old_tree_oid_(std::move(old_tree_oid)),
        new_tree_oid_(std::move(new_tree_oid)) {}

 protected:
  std::vector<PatchResult> DoExecute() override {
    std::vector<PatchResult> results;

    git_diff* diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;

    if (index_to_workdir_) {
      git_check(git_diff_index_to_workdir(&diff, repo_, nullptr, &opts));
    } else {
      git_tree* old_tree = nullptr;
      git_tree* new_tree = nullptr;

      if (!old_tree_oid_.empty()) {
        git_oid oid;
        git_check(hex_to_oid(&oid, old_tree_oid_));
        git_check(git_tree_lookup(&old_tree, repo_, &oid));
      }
      if (!new_tree_oid_.empty()) {
        git_oid oid;
        git_check(hex_to_oid(&oid, new_tree_oid_));
        git_check(git_tree_lookup(&new_tree, repo_, &oid));
      }

      git_check(git_diff_tree_to_tree(&diff, repo_, old_tree, new_tree, &opts));

      if (old_tree) git_tree_free(old_tree);
      if (new_tree) git_tree_free(new_tree);
    }
    GitDiffGuard diff_guard(diff);

    size_t num_deltas = git_diff_num_deltas(diff);
    for (size_t i = 0; i < num_deltas; i++) {
      git_patch* patch = nullptr;
      if (git_patch_from_diff(&patch, diff, i) != 0) continue;
      GitPatchGuard patch_guard(patch);

      PatchResult pr;
      const git_diff_delta* delta = git_patch_get_delta(patch);
      pr.old_path = delta->old_file.path ? delta->old_file.path : "";
      pr.new_path = delta->new_file.path ? delta->new_file.path : "";

      size_t adds = 0, dels = 0, context = 0;
      git_patch_line_stats(&context, &adds, &dels, patch);
      pr.adds = adds;
      pr.dels = dels;
      pr.context = context;

      size_t num_hunks = git_patch_num_hunks(patch);
      for (size_t h = 0; h < num_hunks; h++) {
        const git_diff_hunk* hunk = nullptr;
        size_t lines_in_hunk = 0;
        if (git_patch_get_hunk(&hunk, &lines_in_hunk, patch, h) != 0) continue;

        HunkData hd;
        hd.header = hunk->header;
        hd.old_start = hunk->old_start;
        hd.old_lines = hunk->old_lines;
        hd.new_start = hunk->new_start;
        hd.new_lines = hunk->new_lines;

        for (size_t l = 0; l < lines_in_hunk; l++) {
          const git_diff_line* line = nullptr;
          if (git_patch_get_line_in_hunk(&line, patch, h, l) != 0) continue;

          HunkData::LineData ld;
          ld.origin = line->origin;
          ld.content = std::string(line->content, line->content_len);
          ld.old_lineno = line->old_lineno;
          ld.new_lineno = line->new_lineno;
          hd.lines.push_back(std::move(ld));
        }

        pr.hunks.push_back(std::move(hd));
      }

      // Full patch text
      git_buf buf = GIT_BUF_INIT;
      if (git_patch_to_buf(&buf, patch) == 0) {
        pr.text = std::string(buf.ptr, buf.size);
        git_buf_dispose(&buf);
      }

      results.push_back(std::move(pr));
    }

    return results;
  }

  Napi::Value GetResult(Napi::Env env, std::vector<PatchResult> results) override {
    auto arr = Napi::Array::New(env, results.size());
    for (size_t i = 0; i < results.size(); i++) {
      auto& pr = results[i];
      auto obj = Napi::Object::New(env);
      obj.Set("oldPath", Napi::String::New(env, pr.old_path));
      obj.Set("newPath", Napi::String::New(env, pr.new_path));
      obj.Set("additions", Napi::Number::New(env, static_cast<double>(pr.adds)));
      obj.Set("deletions", Napi::Number::New(env, static_cast<double>(pr.dels)));
      obj.Set("context", Napi::Number::New(env, static_cast<double>(pr.context)));
      obj.Set("text", Napi::String::New(env, pr.text));

      auto hunks = Napi::Array::New(env, pr.hunks.size());
      for (size_t h = 0; h < pr.hunks.size(); h++) {
        auto& hd = pr.hunks[h];
        auto hobj = Napi::Object::New(env);
        hobj.Set("header", Napi::String::New(env, hd.header));
        hobj.Set("oldStart", Napi::Number::New(env, hd.old_start));
        hobj.Set("oldLines", Napi::Number::New(env, hd.old_lines));
        hobj.Set("newStart", Napi::Number::New(env, hd.new_start));
        hobj.Set("newLines", Napi::Number::New(env, hd.new_lines));

        auto lines = Napi::Array::New(env, hd.lines.size());
        for (size_t l = 0; l < hd.lines.size(); l++) {
          auto& ld = hd.lines[l];
          auto lobj = Napi::Object::New(env);
          lobj.Set("origin", Napi::Number::New(env, ld.origin));
          lobj.Set("content", Napi::String::New(env, ld.content));
          lobj.Set("oldLineno", Napi::Number::New(env, ld.old_lineno));
          lobj.Set("newLineno", Napi::Number::New(env, ld.new_lineno));
          lines.Set(static_cast<uint32_t>(l), lobj);
        }
        hobj.Set("lines", lines);
        hunks.Set(static_cast<uint32_t>(h), hobj);
      }
      obj.Set("hunks", hunks);

      arr.Set(static_cast<uint32_t>(i), obj);
    }
    return arr;
  }

 private:
  git_repository* repo_;
  bool index_to_workdir_;
  std::string old_tree_oid_;
  std::string new_tree_oid_;
};

// Patch.fromWorkdir(repo) → Promise<PatchResult[]>
static Napi::Value FromWorkdir(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  auto* worker = new PatchFromDiffWorker(env, repo_wrap->GetRepo(), true, "", "");
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Patch.fromTrees(repo, oldTreeOid, newTreeOid) → Promise<PatchResult[]>
static Napi::Value FromTrees(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 3 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected (Repository, oldTreeOid, newTreeOid)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string old_oid = get_string_or_empty(info[1]);
  std::string new_oid = get_string_or_empty(info[2]);

  auto* worker = new PatchFromDiffWorker(env, repo_wrap->GetRepo(), false, old_oid, new_oid);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

void InitPatch(Napi::Env env, Napi::Object exports) {
  auto patch = Napi::Object::New(env);
  patch.Set("fromWorkdir", Napi::Function::New(env, FromWorkdir, "fromWorkdir"));
  patch.Set("fromTrees", Napi::Function::New(env, FromTrees, "fromTrees"));

  // Line origin constants
  patch.Set("LINE_CONTEXT", Napi::Number::New(env, GIT_DIFF_LINE_CONTEXT));
  patch.Set("LINE_ADDITION", Napi::Number::New(env, GIT_DIFF_LINE_ADDITION));
  patch.Set("LINE_DELETION", Napi::Number::New(env, GIT_DIFF_LINE_DELETION));
  patch.Set("LINE_CONTEXT_EOFNL", Napi::Number::New(env, GIT_DIFF_LINE_CONTEXT_EOFNL));
  patch.Set("LINE_ADD_EOFNL", Napi::Number::New(env, GIT_DIFF_LINE_ADD_EOFNL));
  patch.Set("LINE_DEL_EOFNL", Napi::Number::New(env, GIT_DIFF_LINE_DEL_EOFNL));

  exports.Set("Patch", patch);
}

}  // namespace thunderbringer
