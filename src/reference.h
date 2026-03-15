#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

void InitReference(Napi::Env env, Napi::Object exports);

}  // namespace thunderbringer
