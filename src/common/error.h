#pragma once

#include <napi.h>
#include <git2.h>
#include <stdexcept>
#include <string>

namespace thunderbringer {

// C++ exception for use in worker threads (cannot throw JS exceptions there)
class GitException : public std::runtime_error {
 public:
  GitException(int error_code, const std::string& message)
      : std::runtime_error(message), error_code_(error_code) {}

  int error_code() const { return error_code_; }

 private:
  int error_code_;
};

// Check a libgit2 return code in a worker thread context.
// Throws GitException on error.
inline void git_check(int error) {
  if (error < 0) {
    const git_error* e = git_error_last();
    std::string msg = e ? e->message : "Unknown libgit2 error";
    throw GitException(error, msg);
  }
}

// Check a libgit2 return code in the main thread context.
// Throws Napi::Error on error.
inline void git_check_throw(Napi::Env env, int error) {
  if (error < 0) {
    const git_error* e = git_error_last();
    std::string msg = e ? e->message : "Unknown libgit2 error";
    Napi::Error::New(env, msg).ThrowAsJavaScriptException();
  }
}

// Format a libgit2 error with class info
inline std::string git_error_message(int error_code) {
  const git_error* e = git_error_last();
  if (e) {
    return std::string(e->message) + " (error " + std::to_string(error_code) + ")";
  }
  return "Unknown libgit2 error (error " + std::to_string(error_code) + ")";
}

}  // namespace thunderbringer
