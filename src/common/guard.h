#pragma once

#include <git2.h>
#include <utility>

namespace thunderbringer {

// Generic RAII guard for libgit2 objects.
// Calls FreeFn on destruction. Move-only to prevent double-free.
template <typename T, void (*FreeFn)(T*)>
class GitGuard {
 public:
  GitGuard() : ptr_(nullptr) {}
  explicit GitGuard(T* ptr) : ptr_(ptr) {}

  ~GitGuard() {
    if (ptr_) FreeFn(ptr_);
  }

  // Move semantics
  GitGuard(GitGuard&& other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  GitGuard& operator=(GitGuard&& other) noexcept {
    if (this != &other) {
      if (ptr_) FreeFn(ptr_);
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  // No copy
  GitGuard(const GitGuard&) = delete;
  GitGuard& operator=(const GitGuard&) = delete;

  T* get() const { return ptr_; }
  T** pptr() { return &ptr_; }
  T* operator->() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

  // Release ownership without freeing
  T* release() {
    T* tmp = ptr_;
    ptr_ = nullptr;
    return tmp;
  }

  // Reset with optional new pointer
  void reset(T* ptr = nullptr) {
    if (ptr_) FreeFn(ptr_);
    ptr_ = ptr;
  }

 private:
  T* ptr_;
};

// Type aliases for common libgit2 objects
using GitRepoGuard = GitGuard<git_repository, git_repository_free>;
using GitCommitGuard = GitGuard<git_commit, git_commit_free>;
using GitTreeGuard = GitGuard<git_tree, git_tree_free>;
using GitIndexGuard = GitGuard<git_index, git_index_free>;
using GitReferenceGuard = GitGuard<git_reference, git_reference_free>;
using GitRemoteGuard = GitGuard<git_remote, git_remote_free>;
using GitDiffGuard = GitGuard<git_diff, git_diff_free>;
using GitPatchGuard = GitGuard<git_patch, git_patch_free>;
using GitBlameGuard = GitGuard<git_blame, git_blame_free>;
using GitRevwalkGuard = GitGuard<git_revwalk, git_revwalk_free>;
using GitSignatureGuard = GitGuard<git_signature, git_signature_free>;
using GitConfigGuard = GitGuard<git_config, git_config_free>;
using GitAnnotatedCommitGuard = GitGuard<git_annotated_commit, git_annotated_commit_free>;
using GitObjectGuard = GitGuard<git_object, git_object_free>;
using GitTagGuard = GitGuard<git_tag, git_tag_free>;
using GitBlobGuard = GitGuard<git_blob, git_blob_free>;
using GitBufGuard = GitGuard<git_buf, git_buf_dispose>;
using GitStatusListGuard = GitGuard<git_status_list, git_status_list_free>;

}  // namespace thunderbringer
