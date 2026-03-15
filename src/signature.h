#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

void InitSignature(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer
