#include "status.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"

namespace thunderbringer {

// Status entry to JS object
static Napi::Object StatusEntryToJS(Napi::Env env,
                                     const git_status_entry* entry) {
  auto obj = Napi::Object::New(env);
  obj.Set("status", Napi::Number::New(env, entry->status));

  if (entry->head_to_index) {
    auto staged = Napi::Object::New(env);
    staged.Set("status", Napi::Number::New(env, entry->head_to_index->status));
    if (entry->head_to_index->old_file.path)
      staged.Set("oldPath", Napi::String::New(env, entry->head_to_index->old_file.path));
    if (entry->head_to_index->new_file.path)
      staged.Set("newPath", Napi::String::New(env, entry->head_to_index->new_file.path));
    obj.Set("headToIndex", staged);
  }

  if (entry->index_to_workdir) {
    auto unstaged = Napi::Object::New(env);
    unstaged.Set("status", Napi::Number::New(env, entry->index_to_workdir->status));
    if (entry->index_to_workdir->old_file.path)
      unstaged.Set("oldPath", Napi::String::New(env, entry->index_to_workdir->old_file.path));
    if (entry->index_to_workdir->new_file.path)
      unstaged.Set("newPath", Napi::String::New(env, entry->index_to_workdir->new_file.path));
    obj.Set("indexToWorkdir", unstaged);
  }

  // Convenience: primary file path
  const char* path = nullptr;
  if (entry->head_to_index && entry->head_to_index->new_file.path)
    path = entry->head_to_index->new_file.path;
  else if (entry->index_to_workdir && entry->index_to_workdir->new_file.path)
    path = entry->index_to_workdir->new_file.path;
  if (path)
    obj.Set("path", Napi::String::New(env, path));

  return obj;
}

struct StatusResult {
  std::vector<std::pair<unsigned int, std::string>> entries;
  // We store full entry data to reconstruct on main thread
  struct EntryData {
    unsigned int status;
    int head_to_index_status;
    std::string head_to_index_old_path;
    std::string head_to_index_new_path;
    int index_to_workdir_status;
    std::string index_to_workdir_old_path;
    std::string index_to_workdir_new_path;
    std::string path;
    bool has_head_to_index;
    bool has_index_to_workdir;
  };
  std::vector<EntryData> full_entries;
};

class StatusWorker : public ThunderbringerWorker<StatusResult> {
 public:
  StatusWorker(Napi::Env env, git_repository* repo, unsigned int flags)
      : ThunderbringerWorker<StatusResult>(env), repo_(repo), flags_(flags) {}

 protected:
  StatusResult DoExecute() override {
    StatusResult result;
    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = flags_;

    git_status_list* list = nullptr;
    git_check(git_status_list_new(&list, repo_, &opts));
    GitStatusListGuard guard(list);

    size_t count = git_status_list_entrycount(list);
    result.full_entries.reserve(count);

    for (size_t i = 0; i < count; i++) {
      const git_status_entry* entry = git_status_byindex(list, i);
      StatusResult::EntryData data{};
      data.status = entry->status;
      data.has_head_to_index = entry->head_to_index != nullptr;
      data.has_index_to_workdir = entry->index_to_workdir != nullptr;

      if (entry->head_to_index) {
        data.head_to_index_status = entry->head_to_index->status;
        if (entry->head_to_index->old_file.path)
          data.head_to_index_old_path = entry->head_to_index->old_file.path;
        if (entry->head_to_index->new_file.path)
          data.head_to_index_new_path = entry->head_to_index->new_file.path;
      }

      if (entry->index_to_workdir) {
        data.index_to_workdir_status = entry->index_to_workdir->status;
        if (entry->index_to_workdir->old_file.path)
          data.index_to_workdir_old_path = entry->index_to_workdir->old_file.path;
        if (entry->index_to_workdir->new_file.path)
          data.index_to_workdir_new_path = entry->index_to_workdir->new_file.path;
      }

      // Primary path
      if (entry->head_to_index && entry->head_to_index->new_file.path)
        data.path = entry->head_to_index->new_file.path;
      else if (entry->index_to_workdir && entry->index_to_workdir->new_file.path)
        data.path = entry->index_to_workdir->new_file.path;

      result.full_entries.push_back(std::move(data));
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, StatusResult result) override {
    auto arr = Napi::Array::New(env, result.full_entries.size());
    for (size_t i = 0; i < result.full_entries.size(); i++) {
      auto& data = result.full_entries[i];
      auto obj = Napi::Object::New(env);
      obj.Set("status", Napi::Number::New(env, data.status));
      if (!data.path.empty())
        obj.Set("path", Napi::String::New(env, data.path));

      if (data.has_head_to_index) {
        auto staged = Napi::Object::New(env);
        staged.Set("status", Napi::Number::New(env, data.head_to_index_status));
        if (!data.head_to_index_old_path.empty())
          staged.Set("oldPath", Napi::String::New(env, data.head_to_index_old_path));
        if (!data.head_to_index_new_path.empty())
          staged.Set("newPath", Napi::String::New(env, data.head_to_index_new_path));
        obj.Set("headToIndex", staged);
      }

      if (data.has_index_to_workdir) {
        auto unstaged = Napi::Object::New(env);
        unstaged.Set("status", Napi::Number::New(env, data.index_to_workdir_status));
        if (!data.index_to_workdir_old_path.empty())
          unstaged.Set("oldPath", Napi::String::New(env, data.index_to_workdir_old_path));
        if (!data.index_to_workdir_new_path.empty())
          unstaged.Set("newPath", Napi::String::New(env, data.index_to_workdir_new_path));
        obj.Set("indexToWorkdir", unstaged);
      }

      arr.Set(i, obj);
    }
    return arr;
  }

 private:
  git_repository* repo_;
  unsigned int flags_;
};

// Status.getStatus(repo, opts?) → Promise<StatusEntry[]>
static Napi::Value GetStatus(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected Repository object").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  if (!repo_wrap || !repo_wrap->GetRepo()) {
    Napi::Error::New(env, "Invalid or freed Repository").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  unsigned int flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                       GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                       GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                       GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

  if (info.Length() > 1 && info[1].IsObject()) {
    auto opts = info[1].As<Napi::Object>();
    if (opts.Has("flags")) {
      flags = opts.Get("flags").As<Napi::Number>().Uint32Value();
    }
  }

  auto* worker = new StatusWorker(env, repo_wrap->GetRepo(), flags);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Status.getFileStatus(repo, path) → number (sync, lightweight)
static Napi::Value GetFileStatus(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, path)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string path = info[1].As<Napi::String>().Utf8Value();

  unsigned int status_flags = 0;
  int error = git_status_file(&status_flags, repo_wrap->GetRepo(), path.c_str());

  if (error == GIT_ENOTFOUND) {
    return Napi::Number::New(env, 0);
  }
  if (error < 0) {
    git_check_throw(env, error);
    return env.Undefined();
  }

  return Napi::Number::New(env, status_flags);
}

void InitStatus(Napi::Env env, Napi::Object exports) {
  auto status = Napi::Object::New(env);

  status.Set("getStatus", Napi::Function::New(env, GetStatus, "getStatus"));
  status.Set("getFileStatus", Napi::Function::New(env, GetFileStatus, "getFileStatus"));

  // Status flag constants
  status.Set("CURRENT", Napi::Number::New(env, GIT_STATUS_CURRENT));
  status.Set("INDEX_NEW", Napi::Number::New(env, GIT_STATUS_INDEX_NEW));
  status.Set("INDEX_MODIFIED", Napi::Number::New(env, GIT_STATUS_INDEX_MODIFIED));
  status.Set("INDEX_DELETED", Napi::Number::New(env, GIT_STATUS_INDEX_DELETED));
  status.Set("INDEX_RENAMED", Napi::Number::New(env, GIT_STATUS_INDEX_RENAMED));
  status.Set("INDEX_TYPECHANGE", Napi::Number::New(env, GIT_STATUS_INDEX_TYPECHANGE));
  status.Set("WT_NEW", Napi::Number::New(env, GIT_STATUS_WT_NEW));
  status.Set("WT_MODIFIED", Napi::Number::New(env, GIT_STATUS_WT_MODIFIED));
  status.Set("WT_DELETED", Napi::Number::New(env, GIT_STATUS_WT_DELETED));
  status.Set("WT_TYPECHANGE", Napi::Number::New(env, GIT_STATUS_WT_TYPECHANGE));
  status.Set("WT_RENAMED", Napi::Number::New(env, GIT_STATUS_WT_RENAMED));
  status.Set("WT_UNREADABLE", Napi::Number::New(env, GIT_STATUS_WT_UNREADABLE));
  status.Set("IGNORED", Napi::Number::New(env, GIT_STATUS_IGNORED));
  status.Set("CONFLICTED", Napi::Number::New(env, GIT_STATUS_CONFLICTED));

  exports.Set("Status", status);
}

}  // namespace thunderbringer
