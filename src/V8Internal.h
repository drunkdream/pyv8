#pragma once

#undef COMPILER
#undef TRUE
#undef FALSE

#include "v8config.h"
# define V8_INLINE inline

#include "src/v8.h"
#include "src/api.h"
#include "src/api-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/debug/debug.h"

/*
#include "src/bootstrapper.h"

#include "src/natives.h"
#include "src/platform.h"
#include "src/scopes.h"

#include "src/debug.h"

#include "src/serialize.h"
#include "src/stub-cache.h"
#include "src/heap.h"

#include "src/parser.h"
#include "src/compiler.h"
#include "src/scanner.h"
*/

namespace v8i = v8::internal;
