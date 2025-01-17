// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/test_tools/fake_proof_source_handle.h"
#include "quic/core/quic_types.h"
#include "quic/platform/api/quic_bug_tracker.h"

namespace quic {
namespace test {
namespace {

struct QUIC_EXPORT_PRIVATE ComputeSignatureResult {
  bool ok;
  std::string signature;
  std::unique_ptr<ProofSource::Details> details;
};

class QUIC_EXPORT_PRIVATE ResultSavingSignatureCallback
    : public ProofSource::SignatureCallback {
 public:
  explicit ResultSavingSignatureCallback(
      absl::optional<ComputeSignatureResult>* result)
      : result_(result) {
    DCHECK(!result_->has_value());
  }
  void Run(bool ok,
           std::string signature,
           std::unique_ptr<ProofSource::Details> details) override {
    result_->emplace(
        ComputeSignatureResult{ok, std::move(signature), std::move(details)});
  }

 private:
  absl::optional<ComputeSignatureResult>* result_;
};

ComputeSignatureResult ComputeSignatureNow(
    ProofSource* delegate,
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    const std::string& hostname,
    uint16_t signature_algorithm,
    absl::string_view in) {
  absl::optional<ComputeSignatureResult> result;
  delegate->ComputeTlsSignature(
      server_address, client_address, hostname, signature_algorithm, in,
      std::make_unique<ResultSavingSignatureCallback>(&result));
  CHECK(result.has_value()) << "delegate->ComputeTlsSignature must computes a "
                               "signature immediately";
  return std::move(result.value());
}
}  // namespace

FakeProofSourceHandle::FakeProofSourceHandle(
    ProofSource* delegate,
    ProofSourceHandleCallback* callback,
    Action select_cert_action,
    Action compute_signature_action)
    : delegate_(delegate),
      callback_(callback),
      select_cert_action_(select_cert_action),
      compute_signature_action_(compute_signature_action) {}

void FakeProofSourceHandle::CancelPendingOperation() {
  select_cert_op_.reset();
  compute_signature_op_.reset();
}

QuicAsyncStatus FakeProofSourceHandle::SelectCertificate(
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    const std::string& hostname,
    absl::string_view client_hello,
    const std::string& alpn,
    const std::vector<uint8_t>& quic_transport_params,
    const absl::optional<std::vector<uint8_t>>& early_data_context) {
  if (select_cert_action_ == Action::DELEGATE_ASYNC ||
      select_cert_action_ == Action::FAIL_ASYNC) {
    select_cert_op_.emplace(delegate_, callback_, select_cert_action_,
                            server_address, client_address, hostname,
                            client_hello, alpn, quic_transport_params,
                            early_data_context);
    return QUIC_PENDING;
  } else if (select_cert_action_ == Action::FAIL_SYNC) {
    callback()->OnSelectCertificateDone(/*ok=*/false,
                                        /*is_sync=*/true, nullptr);
    return QUIC_FAILURE;
  }

  DCHECK(select_cert_action_ == Action::DELEGATE_SYNC);
  QuicReferenceCountedPointer<ProofSource::Chain> chain =
      delegate_->GetCertChain(server_address, client_address, hostname);

  bool ok = chain && !chain->certs.empty();
  callback_->OnSelectCertificateDone(ok, /*is_sync=*/true, chain.get());
  return ok ? QUIC_SUCCESS : QUIC_FAILURE;
}

QuicAsyncStatus FakeProofSourceHandle::ComputeSignature(
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    const std::string& hostname,
    uint16_t signature_algorithm,
    absl::string_view in,
    size_t max_signature_size) {
  if (compute_signature_action_ == Action::DELEGATE_ASYNC ||
      compute_signature_action_ == Action::FAIL_ASYNC) {
    compute_signature_op_.emplace(
        delegate_, callback_, compute_signature_action_, server_address,
        client_address, hostname, signature_algorithm, in, max_signature_size);
    return QUIC_PENDING;
  } else if (compute_signature_action_ == Action::FAIL_SYNC) {
    callback()->OnComputeSignatureDone(/*ok=*/false, /*is_sync=*/true,
                                       /*signature=*/"", /*details=*/nullptr);
    return QUIC_FAILURE;
  }

  DCHECK(compute_signature_action_ == Action::DELEGATE_SYNC);
  ComputeSignatureResult result =
      ComputeSignatureNow(delegate_, server_address, client_address, hostname,
                          signature_algorithm, in);
  callback_->OnComputeSignatureDone(
      result.ok, /*is_sync=*/true, result.signature, std::move(result.details));
  return result.ok ? QUIC_SUCCESS : QUIC_FAILURE;
}

ProofSourceHandleCallback* FakeProofSourceHandle::callback() {
  return callback_;
}

bool FakeProofSourceHandle::HasPendingOperation() const {
  int num_pending_operations = NumPendingOperations();
  return num_pending_operations > 0;
}

void FakeProofSourceHandle::CompletePendingOperation() {
  DCHECK_LE(NumPendingOperations(), 1);

  if (select_cert_op_.has_value()) {
    select_cert_op_->Run();
  } else if (compute_signature_op_.has_value()) {
    compute_signature_op_->Run();
  }
}

int FakeProofSourceHandle::NumPendingOperations() const {
  return static_cast<int>(select_cert_op_.has_value()) +
         static_cast<int>(compute_signature_op_.has_value());
}

FakeProofSourceHandle::SelectCertOperation::SelectCertOperation(
    ProofSource* delegate,
    ProofSourceHandleCallback* callback,
    Action action,
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    const std::string& hostname,
    absl::string_view client_hello,
    const std::string& alpn,
    const std::vector<uint8_t>& quic_transport_params,
    const absl::optional<std::vector<uint8_t>>& early_data_context)
    : PendingOperation(delegate, callback, action),
      server_address_(server_address),
      client_address_(client_address),
      hostname_(hostname),
      client_hello_(client_hello),
      alpn_(alpn),
      quic_transport_params_(quic_transport_params),
      early_data_context_(early_data_context) {}

void FakeProofSourceHandle::SelectCertOperation::Run() {
  if (action_ == Action::FAIL_ASYNC) {
    callback_->OnSelectCertificateDone(/*ok=*/false,
                                       /*is_sync=*/false, nullptr);
  } else if (action_ == Action::DELEGATE_ASYNC) {
    QuicReferenceCountedPointer<ProofSource::Chain> chain =
        delegate_->GetCertChain(server_address_, client_address_, hostname_);
    bool ok = chain && !chain->certs.empty();
    callback_->OnSelectCertificateDone(ok, /*is_sync=*/false, chain.get());
  } else {
    QUIC_BUG << "Unexpected action: " << static_cast<int>(action_);
  }
}

FakeProofSourceHandle::ComputeSignatureOperation::ComputeSignatureOperation(
    ProofSource* delegate,
    ProofSourceHandleCallback* callback,
    Action action,
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    const std::string& hostname,
    uint16_t signature_algorithm,
    absl::string_view in,
    size_t max_signature_size)
    : PendingOperation(delegate, callback, action),
      server_address_(server_address),
      client_address_(client_address),
      hostname_(hostname),
      signature_algorithm_(signature_algorithm),
      in_(in),
      max_signature_size_(max_signature_size) {}

void FakeProofSourceHandle::ComputeSignatureOperation::Run() {
  if (action_ == Action::FAIL_ASYNC) {
    callback_->OnComputeSignatureDone(
        /*ok=*/false, /*is_sync=*/false,
        /*signature=*/"", /*details=*/nullptr);
  } else if (action_ == Action::DELEGATE_ASYNC) {
    ComputeSignatureResult result =
        ComputeSignatureNow(delegate_, server_address_, client_address_,
                            hostname_, signature_algorithm_, in_);
    callback_->OnComputeSignatureDone(result.ok, /*is_sync=*/false,
                                      result.signature,
                                      std::move(result.details));
  } else {
    QUIC_BUG << "Unexpected action: " << static_cast<int>(action_);
  }
}

}  // namespace test
}  // namespace quic
