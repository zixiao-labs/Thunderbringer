#include "commit.h"
#include "repository.h"
#include "common/error.h"
#include "common/guard.h"
#include "common/async_worker.h"
#include "common/util.h"

namespace thunderbringer {

static Napi::Object CommitToJS(Napi::Env env, git_commit* commit) {
  auto obj = Napi::Object::New(env);
  obj.Set("oid", Napi::String::New(env, oid_to_hex(git_commit_id(commit))));
  obj.Set("message", Napi::String::New(env, git_commit_message(commit) ? git_commit_message(commit) : ""));
  obj.Set("summary", Napi::String::New(env, git_commit_summary(commit) ? git_commit_summary(commit) : ""));

  const git_signature* author = git_commit_author(commit);
  if (author) obj.Set("author", signature_to_js(env, author));

  const git_signature* committer = git_commit_committer(commit);
  if (committer) obj.Set("committer", signature_to_js(env, committer));

  obj.Set("time", Napi::Number::New(env, static_cast<double>(git_commit_time(commit)) * 1000.0));
  obj.Set("parentCount", Napi::Number::New(env, git_commit_parentcount(commit)));

  // Parent OIDs
  auto parents = Napi::Array::New(env, git_commit_parentcount(commit));
  for (unsigned int i = 0; i < git_commit_parentcount(commit); i++) {
    parents.Set(i, Napi::String::New(env, oid_to_hex(git_commit_parent_id(commit, i))));
  }
  obj.Set("parents", parents);

  // Tree OID
  obj.Set("treeOid", Napi::String::New(env, oid_to_hex(git_commit_tree_id(commit))));

  return obj;
}

struct CreateCommitResult {
  std::string oid;
};

class CreateCommitWorker : public ThunderbringerWorker<CreateCommitResult> {
 public:
  CreateCommitWorker(Napi::Env env, git_repository* repo,
                     std::string message, std::string tree_oid_str,
                     std::vector<std::string> parent_oid_strs,
                     git_signature* author, git_signature* committer,
                     std::string update_ref)
      : ThunderbringerWorker<CreateCommitResult>(env),
        repo_(repo), message_(std::move(message)),
        tree_oid_str_(std::move(tree_oid_str)),
        parent_oid_strs_(std::move(parent_oid_strs)),
        author_(author), committer_(committer),
        update_ref_(std::move(update_ref)) {}

  ~CreateCommitWorker() {
    if (author_) git_signature_free(author_);
    if (committer_) git_signature_free(committer_);
  }

 protected:
  CreateCommitResult DoExecute() override {
    git_oid tree_oid;
    git_check(hex_to_oid(&tree_oid, tree_oid_str_));

    git_tree* tree = nullptr;
    git_check(git_tree_lookup(&tree, repo_, &tree_oid));
    GitTreeGuard tree_guard(tree);

    // Lookup parent commits
    std::vector<git_commit*> parent_ptrs_raw;
    std::vector<GitCommitGuard> parent_guards;
    for (auto& parent_str : parent_oid_strs_) {
      git_oid parent_oid;
      git_check(hex_to_oid(&parent_oid, parent_str));
      git_commit* parent = nullptr;
      git_check(git_commit_lookup(&parent, repo_, &parent_oid));
      parent_ptrs_raw.push_back(parent);
      parent_guards.emplace_back(parent);
    }

    // git_commit_create expects const git_commit**
    std::vector<const git_commit*> parents(parent_ptrs_raw.begin(), parent_ptrs_raw.end());
    const git_commit** parent_ptrs = parents.empty() ? nullptr : parents.data();

    git_oid commit_oid;
    git_check(git_commit_create(
        &commit_oid, repo_,
        update_ref_.empty() ? nullptr : update_ref_.c_str(),
        author_, committer_,
        nullptr,  // UTF-8 encoding
        message_.c_str(),
        tree,
        parents.size(), parent_ptrs));

    return CreateCommitResult{oid_to_hex(&commit_oid)};
  }

  Napi::Value GetResult(Napi::Env env, CreateCommitResult result) override {
    return Napi::String::New(env, result.oid);
  }

 private:
  git_repository* repo_;
  std::string message_;
  std::string tree_oid_str_;
  std::vector<std::string> parent_oid_strs_;
  git_signature* author_;
  git_signature* committer_;
  std::string update_ref_;
};

struct LookupCommitResult {
  std::string oid;
  std::string message;
  std::string summary;
  std::string author_name;
  std::string author_email;
  double author_time;
  int author_offset;
  std::string committer_name;
  std::string committer_email;
  double committer_time;
  int committer_offset;
  double time;
  unsigned int parent_count;
  std::vector<std::string> parent_oids;
  std::string tree_oid;
};

class LookupCommitWorker : public ThunderbringerWorker<LookupCommitResult> {
 public:
  LookupCommitWorker(Napi::Env env, git_repository* repo, std::string oid_str)
      : ThunderbringerWorker<LookupCommitResult>(env),
        repo_(repo), oid_str_(std::move(oid_str)) {}

 protected:
  LookupCommitResult DoExecute() override {
    git_oid oid;
    git_check(hex_to_oid(&oid, oid_str_));

    git_commit* commit = nullptr;
    git_check(git_commit_lookup(&commit, repo_, &oid));
    GitCommitGuard guard(commit);

    LookupCommitResult result;
    result.oid = oid_to_hex(git_commit_id(commit));
    result.message = git_commit_message(commit) ? git_commit_message(commit) : "";
    result.summary = git_commit_summary(commit) ? git_commit_summary(commit) : "";
    result.time = static_cast<double>(git_commit_time(commit)) * 1000.0;
    result.parent_count = git_commit_parentcount(commit);
    result.tree_oid = oid_to_hex(git_commit_tree_id(commit));

    const git_signature* author = git_commit_author(commit);
    if (author) {
      result.author_name = author->name ? author->name : "";
      result.author_email = author->email ? author->email : "";
      result.author_time = static_cast<double>(author->when.time) * 1000.0;
      result.author_offset = author->when.offset;
    }

    const git_signature* committer = git_commit_committer(commit);
    if (committer) {
      result.committer_name = committer->name ? committer->name : "";
      result.committer_email = committer->email ? committer->email : "";
      result.committer_time = static_cast<double>(committer->when.time) * 1000.0;
      result.committer_offset = committer->when.offset;
    }

    for (unsigned int i = 0; i < result.parent_count; i++) {
      result.parent_oids.push_back(oid_to_hex(git_commit_parent_id(commit, i)));
    }

    return result;
  }

  Napi::Value GetResult(Napi::Env env, LookupCommitResult r) override {
    auto obj = Napi::Object::New(env);
    obj.Set("oid", Napi::String::New(env, r.oid));
    obj.Set("message", Napi::String::New(env, r.message));
    obj.Set("summary", Napi::String::New(env, r.summary));
    obj.Set("time", Napi::Number::New(env, r.time));
    obj.Set("parentCount", Napi::Number::New(env, r.parent_count));
    obj.Set("treeOid", Napi::String::New(env, r.tree_oid));

    auto author = Napi::Object::New(env);
    author.Set("name", Napi::String::New(env, r.author_name));
    author.Set("email", Napi::String::New(env, r.author_email));
    author.Set("time", Napi::Number::New(env, r.author_time));
    author.Set("offset", Napi::Number::New(env, r.author_offset));
    obj.Set("author", author);

    auto committer = Napi::Object::New(env);
    committer.Set("name", Napi::String::New(env, r.committer_name));
    committer.Set("email", Napi::String::New(env, r.committer_email));
    committer.Set("time", Napi::Number::New(env, r.committer_time));
    committer.Set("offset", Napi::Number::New(env, r.committer_offset));
    obj.Set("committer", committer);

    auto parents = Napi::Array::New(env, r.parent_oids.size());
    for (size_t i = 0; i < r.parent_oids.size(); i++) {
      parents.Set(static_cast<uint32_t>(i), Napi::String::New(env, r.parent_oids[i]));
    }
    obj.Set("parents", parents);

    return obj;
  }

 private:
  git_repository* repo_;
  std::string oid_str_;
};

// Commit.create(repo, { message, tree, parents, author, committer, updateRef }) → Promise<oid>
static Napi::Value Create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsObject()) {
    Napi::TypeError::New(env, "Expected (Repository, options)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  auto opts = info[1].As<Napi::Object>();

  std::string message = opts.Get("message").As<Napi::String>().Utf8Value();
  std::string tree_oid = opts.Get("tree").As<Napi::String>().Utf8Value();

  std::vector<std::string> parent_oids;
  if (opts.Has("parents") && opts.Get("parents").IsArray()) {
    auto arr = opts.Get("parents").As<Napi::Array>();
    for (uint32_t i = 0; i < arr.Length(); i++) {
      parent_oids.push_back(arr.Get(i).As<Napi::String>().Utf8Value());
    }
  }

  git_signature* author = nullptr;
  git_signature* committer = nullptr;

  if (opts.Has("author") && opts.Get("author").IsObject()) {
    author = js_to_signature(env, opts.Get("author").As<Napi::Object>());
  }
  if (opts.Has("committer") && opts.Get("committer").IsObject()) {
    committer = js_to_signature(env, opts.Get("committer").As<Napi::Object>());
  }

  // If no committer, use author
  if (!committer && author) {
    git_signature_dup(&committer, author);
  }

  std::string update_ref = "HEAD";
  if (opts.Has("updateRef")) {
    auto ref_val = opts.Get("updateRef");
    if (ref_val.IsNull() || ref_val.IsUndefined()) {
      update_ref = "";
    } else {
      update_ref = ref_val.As<Napi::String>().Utf8Value();
    }
  }

  auto* worker = new CreateCommitWorker(
      env, repo_wrap->GetRepo(), message, tree_oid,
      parent_oids, author, committer, update_ref);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

// Commit.lookup(repo, oid) → Promise<CommitObject>
static Napi::Value Lookup(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected (Repository, oid)").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto* repo_wrap = Napi::ObjectWrap<GitRepository>::Unwrap(info[0].As<Napi::Object>());
  std::string oid_str = info[1].As<Napi::String>().Utf8Value();

  auto* worker = new LookupCommitWorker(env, repo_wrap->GetRepo(), oid_str);
  auto promise = worker->Promise();
  worker->Queue();
  return promise;
}

void InitCommit(Napi::Env env, Napi::Object exports) {
  auto commit = Napi::Object::New(env);
  commit.Set("create", Napi::Function::New(env, Create, "create"));
  commit.Set("lookup", Napi::Function::New(env, Lookup, "lookup"));
  exports.Set("Commit", commit);
}

}  // namespace thunderbringer
