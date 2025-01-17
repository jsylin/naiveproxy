// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file._

#ifndef QUICHE_QUIC_PLATFORM_API_QUIC_CERT_UTILS_H_
#define QUICHE_QUIC_PLATFORM_API_QUIC_CERT_UTILS_H_

#include "absl/strings/string_view.h"
#include "quic/platform/api/quic_export.h"
#include "net/quic/platform/impl/quic_cert_utils_impl.h"

namespace quic {

class QUIC_EXPORT_PRIVATE QuicCertUtils {
 public:
  static bool ExtractSubjectNameFromDERCert(absl::string_view cert,
                                            absl::string_view* subject_out) {
    return QuicCertUtilsImpl::ExtractSubjectNameFromDERCert(cert, subject_out);
  }
};

}  // namespace quic

#endif  // QUICHE_QUIC_PLATFORM_API_QUIC_CERT_UTILS_H_
