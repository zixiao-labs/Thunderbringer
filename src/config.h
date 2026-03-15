#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

void InitConfig(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer
