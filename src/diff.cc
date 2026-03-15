#include "diff.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

struct DiffFileData {
  std::string path;
  std::string oid;
  uint16_t mode;
};

struct DiffDeltaData {
  int status;
  uint32_t flags;
  DiffFileData old_file;
  DiffFileData new_file;
};

struct DiffStatsData {
  size_t files_changed;
  size_t insertions;
  size_t deletions;
};

struct DiffResult {
  std::vector<DiffDeltaData> deltas;
  DiffStatsData stats;
};

static DiffDeltaData DeltaToData(const git_diff_delta* delta) {
  DiffDeltaData data;
  data.status = delta->status;
  data.flags = delta->flags;
  data.old_file.path = delta->old_file.path ? delta->old_file.path : "";
  data.old_file.oid = oid_to_hex(&delta->old_file.id);
  data.old_file.mode = delta->old_file.mode;
  data.new_file.path = delta->new_file.path ? delta->new_file.path : "";
  data.new_file.oid = oid_to_hex(&delta->new_file.id);
  data.new_file.mode = delta->new_file.mode;
  return data;
}

static Napi::Object DiffDeltaToJS(Napi::Env env, const DiffDeltaData& data) {
  auto obj = Napi::Object::New(env);
  obj.Set("status", Napi::Number::New(env, data.status));
  obj.Set("flags", Napi::Number::New(env, data.flags));

  auto old_file = Napi::Object::New(env);
  old_file.Set("path", Napi::String::New(env, data.old_file.path));
  old_file.Set("oid", Napi::String::New(env, data.old_file.oid));
  old_file.Set("mode", Napi::Number::New(env, data.old_file.mode));
  obj.Set("oldFile", old_file);

  auto new_file = Napi::Object::New(env);
  new_file.Set("path", Napi::String::New(env, data.new_file.path));
  new_file.Set("oid", Napi::String::New(env, data.new_file.oid));
  new_file.Set("mode", Napi::Number::New(env, data.new_file.mode));
  obj.Set("newFile", new_file);

  return obj;
}

class IndexToWorkdirWorker : public ThunderbringerWorker<DiffResult> {
 public:
  IndexToWorkdirWorker(Napi::Env env, git_repository* repo)
      : ThunderbringerWorker<DiffResult>(env), repo_(repo) {}

 protected:
  DiffResult DoExecute() override {
    DiffResult result;

    git_diff* diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    git_check(git_diff_index_to_workdir(&diff, repo_, nullptr, &opts));
    GitDiffGuard guard(diff);

    size_t num_deltas = git_diff_num_deltas(diff);
    for (size_t i = 0; i < num_deltas; i++) {
      const git_diff_delta* delta = git_diff_get_delta(diff, i);
      result.deltas.push_back(DeltaToData(delta));
    }

    git_diff_stats* stats = nullptr;
    if (git_diff_get_stats(&stats, diff) == 0) {
      result.stats.files_changed = git_diff_stats_files_changed(stats);
      result.stats.insertions = git_diff_stats_insertions(stats);
      result.stats.deletions = git_diff_stats_deletions(stats);
      git_diff_stats_free(stats);
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, DiffResult result) override {
    auto obj = Napi::Object::New(env);

    auto deltas = Napi::Array::New(env, result.deltas.size());
    for (size_t i = 0; i < result.deltas.size(); i++) {
      deltas.Set(static_cast<uint32_t>(i), DiffDeltaToJS(env, result.deltas[i]));
    }
    obj.Set("deltas", deltas);

    auto stats = Napi::Object::New(env);
    stats.Set("filesChanged", Napi::Number::New(env, static_cast<double>(result.stats.files_changed)));
    stats.Set("insertions", Napi::Number::New(env, static_cast<double>(result.stats.insertions)));
    stats.Set("deletions", Napi::Number::New(env, static_cast<double>(result.stats.deletions)));
    obj.Set("stats", stats);

    return obj;
  }

 private:
  git_repository* repo_;
};

class TreeToTreeWorker : public ThunderbringerWorker<DiffResult> {
 public:
  TreeToTreeWorker(Napi::Env env, git_repository* repo,
                   std::string old_tree_oid, std::string new_tree_oid)
      : ThunderbringerWorker<DiffResult>(env), repo_(repo),
        old_tree_oid_(std::move(old_tree_oid)),
        new_tree_oid_(std::move(new_tree_oid)) {}

 protected:
  DiffResult DoExecute() override {
    DiffResult result;

    git_tree* old_tree = nullptr;
    git_tree* new_tree = nullptr;

    if (!old_tree_oid_.empty()) {
      git_oid oid;
      git_check(hex_to_oid(&oid, old_tree_oid_));
      git_check(git_tree_lookup(&old_tree, repo_, &oid));
    }
    GitTreeGuard old_guard(old_tree);

    if (!new_tree_oid_.empty()) {
      git_oid oid;
      git_check(hex_to_oid(&oid, new_tree_oid_));
      git_check(git_tree_lookup(&new_tree, repo_, &oid));
    }
    GitTreeGuard new_guard(new_tree);

    git_diff* diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    git_check(git_diff_tree_to_tree(&diff, repo_, old_tree, new_tree, &opts));
    GitDiffGuard diff_guard(diff);

    size_t num_deltas = git_diff_num_deltas(diff);
    for (size_t i = 0; i < num_deltas; i++) {
      const git_diff_delta* delta = git_diff_get_delta(diff, i);
      result.deltas.push_back(DeltaToData(delta));
    }

    git_diff_stats* stats = nullptr;
    if (git_diff_get_stats(&stats, diff) == 0) {
      result.stats.files_changed = git_diff_stats_files_changed(stats);
      result.stats.insertions = git_diff_stats_insertions(stats);
      result.stats.deletions = git_diff_stats_deletions(stats);
      git_diff_stats_free(stats);
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, DiffResult result) override {
    auto obj = Napi::Object::New(env);

    auto deltas = Napi::Array::New(env, result.deltas.size());
    for (size_t i = 0; i < result.deltas.size(); i++) {
      deltas.Set(static_cast<uint32_t>(i), DiffDeltaToJS(env, result.deltas[i]));
    }
    obj.Set("deltas", deltas);

    auto stats = Napi::Object::New(env);
    stats.Set("filesChanged", Napi::Number::New(env, static_cast<double>(result.stats.files_changed)));
    stats.Set("insertions", Napi::Number::New(env, static_cast<double>(result.stats.insertions)));
    stats.Set("deletions", Napi::Number::New(env, static_cast<double>(result.stats.deletions)));
    obj.Set("stats", stats);

    return obj;
  }

 private:
  git_repository* repo_;
  std::string old_tree_oid_;
  std::string new_tree_oid_;
};

class TreeToIndexWorker : public ThunderbringerWorker<DiffResult> {
 public:
  TreeToIndexWorker(Napi::Env env, git_repository* repo, std::string tree_oid)
      : ThunderbringerWorker<DiffResult>(env), repo_(repo),
        tree_oid_(std::move(tree_oid)) {}

 protected:
  DiffResult DoExecute() override {
    DiffResult result;

    git_tree* tree = nullptr;
    if (!tree_oid_.empty()) {
      git_oid oid;
      git_check(hex_to_oid(&oid, tree_oid_));
      git_check(git_tree_lookup(&tree, repo_, &oid));
    }
    GitTreeGuard tree_guard(tree);

    git_diff* diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    git_check(git_diff_tree_to_index(&diff, repo_, tree, nullptr, &opts));
    GitDiffGuard diff_guard(diff);

    size_t num_deltas = git_diff_num_deltas(diff);
    for (size_t i = 0; i < num_deltas; i++) {
      const git_diff_delta* delta = git_diff_get_delta(diff, i);
      result.deltas.push_back(DeltaToData(delta));
    }

    git_diff_stats* stats = nullptr;
    if (git_diff_get_stats(&stats, diff) == 0) {
      result.stats.files_changed = git_diff_stats_files_changed(stats);
      result.stats.insertions = git_diff_stats_insertions(stats);
      result.stats.deletions = git_diff_stats_deletions(stats);
      git_diff_stats_free(stats);
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, DiffResult result) override {
    auto obj = Napi::Object::New(env);

    auto deltas = Napi::Array::New(env, result.deltas.size());
    for (size_t i = 0; i < result.deltas.size(); i++) {
      deltas.Set(static_cast<uint32_t>(i), DiffDeltaToJS(env, result.deltas[i]));
    }
    obj.Set("deltas", deltas);

    auto stats = Napi::Object::New(env);
    stats.Set("filesChanged", Napi::Number::New(env, static_cast<double>(result.stats.files_changed)));
    stats.Set("insertions", Napi::Number::New(env, static_cast<double>(result.stats.insertions)));
    stats.Set("deletions", Napi::Number::New(env, static_cast<double>(result.stats.deletions)));
    obj.Set("stats", stats);

    return obj;
  }

 private:
  git_repository* repo_;
  std::string tree_oid_;
};

// Diff.indexToWorkdir(repo) → Promise<DiffResult>
static Napi::Value IndexToWorkdir(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  auto* worker = new IndexToWorkdirWorker(env, repo_wrap->GetRepo());
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Diff.treeToTree(repo, oldTreeOid, newTreeOid) → Promise<DiffResult>
static Napi::Value TreeToTree(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected (Repository, oldTreeOid, newTreeOid)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string old_oid = get_string_or_empty(info[1]);
  std::string new_oid = get_string_or_empty(info[2]);

  auto* worker = new TreeToTreeWorker(env, repo_wrap->GetRepo(), old_oid, new_oid);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Diff.treeToIndex(repo, treeOid?) → Promise<DiffResult>
static Napi::Value TreeToIndex(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string tree_oid = info.Length() > 1 ? get_string_or_empty(info[1]) : "";

  auto* worker = new TreeToIndexWorker(env, repo_wrap->GetRepo(), tree_oid);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

void InitDiff(Napi::Env env, Napi::Object exports) {
  auto diff = Napi::Object::New(env);
  diff.Set("indexToWorkdir", Napi::Function::New(env, IndexToWorkdir, "indexToWorkdir"));
  diff.Set("treeToTree", Napi::Function::New(env, TreeToTree, "treeToTree"));
  diff.Set("treeToIndex", Napi::Function::New(env, TreeToIndex, "treeToIndex"));

  // Delta status constants
  diff.Set("DELTA_UNMODIFIED", Napi::Number::New(env, GIT_DELTA_UNMODIFIED));
  diff.Set("DELTA_ADDED", Napi::Number::New(env, GIT_DELTA_ADDED));
  diff.Set("DELTA_DELETED", Napi::Number::New(env, GIT_DELTA_DELETED));
  diff.Set("DELTA_MODIFIED", Napi::Number::New(env, GIT_DELTA_MODIFIED));
  diff.Set("DELTA_RENAMED", Napi::Number::New(env, GIT_DELTA_RENAMED));
  diff.Set("DELTA_COPIED", Napi::Number::New(env, GIT_DELTA_COPIED));
  diff.Set("DELTA_IGNORED", Napi::Number::New(env, GIT_DELTA_IGNORED));
  diff.Set("DELTA_UNTRACKED", Napi::Number::New(env, GIT_DELTA_UNTRACKED));
  diff.Set("DELTA_TYPECHANGE", Napi::Number::New(env, GIT_DELTA_TYPECHANGE));
  diff.Set("DELTA_UNREADABLE", Napi::Number::New(env, GIT_DELTA_UNREADABLE));
  diff.Set("DELTA_CONFLICTED", Napi::Number::New(env, GIT_DELTA_CONFLICTED));

  exports.Set("Diff", diff);
}

}  // namespace thunderbringer
