#include "addon.h"
#include <git2.h>

// Forward declarations for each module's Init function
namespace thunderbringer {

// Phase 1
void InitRepository(Napi::Env env, Napi::Object exports);
void InitStatus(Napi::Env env, Napi::Object exports);

// Phase 2
void InitIndex(Napi::Env env, Napi::Object exports);
void InitCommit(Napi::Env env, Napi::Object exports);
void InitSignature(Napi::Env env, Napi::Object exports);
void InitRevwalk(Napi::Env env, Napi::Object exports);

// Phase 3
void InitReference(Napi::Env env, Napi::Object exports);
void InitBranch(Napi::Env env, Napi::Object exports);

// Phase 4
void InitDiff(Napi::Env env, Napi::Object exports);
void InitPatch(Napi::Env env, Napi::Object exports);
void InitBlame(Napi::Env env, Napi::Object exports);

// Phase 5
void InitRemote(Napi::Env env, Napi::Object exports);

// Phase 6
void InitMerge(Napi::Env env, Napi::Object exports);
void InitTag(Napi::Env env, Napi::Object exports);
void InitStash(Napi::Env env, Napi::Object exports);
void InitCheckout(Napi::Env env, Napi::Object exports);
void InitConfig(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer

// Module init function — must be at file scope for NODE_API_MODULE
static Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  git_libgit2_init();

  thunderbringer::InitRepository(env, exports);
  thunderbringer::InitStatus(env, exports);
  thunderbringer::InitIndex(env, exports);
  thunderbringer::InitCommit(env, exports);
  thunderbringer::InitSignature(env, exports);
  thunderbringer::InitRevwalk(env, exports);
  thunderbringer::InitReference(env, exports);
  thunderbringer::InitBranch(env, exports);
  thunderbringer::InitDiff(env, exports);
  thunderbringer::InitPatch(env, exports);
  thunderbringer::InitBlame(env, exports);
  thunderbringer::InitRemote(env, exports);
  thunderbringer::InitMerge(env, exports);
  thunderbringer::InitTag(env, exports);
  thunderbringer::InitStash(env, exports);
  thunderbringer::InitCheckout(env, exports);
  thunderbringer::InitConfig(env, exports);

  // Version info
  exports.Set("libgit2Version", Napi::String::New(env, LIBGIT2_VERSION));

  return exports;
}

NODE_API_MODULE(thunderbringer, InitAll)
