#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

void InitStatus(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer
