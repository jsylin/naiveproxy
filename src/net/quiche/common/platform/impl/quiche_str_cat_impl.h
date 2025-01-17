// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_STR_CAT_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_STR_CAT_IMPL_H_

#include <sstream>
#include <string>
#include <utility>

#include "base/strings/abseil_string_conversions.h"
#include "base/strings/stringprintf.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace quiche {

inline absl::string_view MaybeStringPieceToStringView(base::StringPiece arg) {
  return base::StringPieceToStringView(arg);
}

template <typename T>
inline T MaybeStringPieceToStringView(const T& arg) {
  return arg;
}

template <typename... Args>
inline std::string QuicheStrCatImpl(const Args&... args) {
  return absl::StrCat(MaybeStringPieceToStringView(args)...);
}

template <typename... Args>
inline std::string QuicheStringPrintfImpl(const char* format,
                                          const Args&... args) {
  std::string out;
  std::vector<absl::FormatArg> args_converted{absl::FormatArg(args)...};
  bool success = absl::FormatUntyped(&out, absl::UntypedFormatSpec(format),
                                     args_converted);
  DCHECK(success);
  return out;
}

}  // namespace quiche

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_STR_CAT_IMPL_H_
