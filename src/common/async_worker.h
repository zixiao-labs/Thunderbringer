#pragma once

#include <napi.h>
#include <string>
#include "error.h"

namespace thunderbringer {

// Base async worker that returns a Promise.
// Subclasses implement DoExecute() (worker thread) and GetResult() (main thread).
template <typename ResultType>
class ThunderbringerWorker : public Napi::AsyncWorker {
 public:
  ThunderbringerWorker(Napi::Env env)
      : Napi::AsyncWorker(env),
        deferred_(Napi::Promise::Deferred::New(env)) {}

  Napi::Promise Promise() { return deferred_.Promise(); }

 protected:
  // Worker thread: perform the libgit2 operation
  virtual ResultType DoExecute() = 0;

  // Main thread: convert result to Napi::Value
  virtual Napi::Value GetResult(Napi::Env env, ResultType result) = 0;

  void Execute() override {
    try {
      result_ = DoExecute();
    } catch (const GitException& e) {
      SetError(e.what());
    } catch (const std::exception& e) {
      SetError(e.what());
    }
  }

  void OnOK() override {
    Napi::HandleScope scope(Env());
    try {
      deferred_.Resolve(GetResult(Env(), std::move(result_)));
    } catch (const std::exception& e) {
      deferred_.Reject(Napi::Error::New(Env(), e.what()).Value());
    }
  }

  void OnError(const Napi::Error& error) override {
    deferred_.Reject(error.Value());
  }

  Napi::Promise::Deferred deferred_;
  ResultType result_;
};

// Specialization for void result (operations that don't return a value)
template <>
class ThunderbringerWorker<void> : public Napi::AsyncWorker {
 public:
  ThunderbringerWorker(Napi::Env env)
      : Napi::AsyncWorker(env),
        deferred_(Napi::Promise::Deferred::New(env)) {}

  Napi::Promise Promise() { return deferred_.Promise(); }

 protected:
  virtual void DoExecute() = 0;

  void Execute() override {
    try {
      DoExecute();
    } catch (const GitException& e) {
      SetError(e.what());
    } catch (const std::exception& e) {
      SetError(e.what());
    }
  }

  void OnOK() override {
    deferred_.Resolve(Env().Undefined());
  }

  void OnError(const Napi::Error& error) override {
    deferred_.Reject(error.Value());
  }

  Napi::Promise::Deferred deferred_;
};

}  // namespace thunderbringer
