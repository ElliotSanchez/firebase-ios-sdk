/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_REMOTE_GRPC_STREAM_H_
#define FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_REMOTE_GRPC_STREAM_H_

#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "Firestore/core/src/firebase/firestore/auth/token.h"
#include "Firestore/core/src/firebase/firestore/util/status.h"
#include "absl/strings/string_view.h"

#include <memory>

namespace firebase {
namespace firestore {
namespace remote {

using firebase::firestore::auth::CredentialsProvider;
using firebase::firestore::auth::Token;
using firebase::firestore::core::DatabaseInfo;
using firebase::firestore::model::DatabaseId;
// using firebase::firestore::model::SnapshotVersion;

/*
message ListenRequest {
// The database name. In the format:
`projects/{project_id}/databases/{database_id}`. string database = 1; oneof
target_change { Target add_target = 2; int32 remove_target = 3;
}
map<string, string> labels = 4;
}

message ListenResponse {
oneof response_type {
  TargetChange target_change = 2;
  DocumentChange document_change = 3;
  DocumentDelete document_delete = 4;
  // because it is no longer relevant to that target.
  DocumentRemove document_remove = 6;
  // Returned when documents may have been removed from the given target, but
  // the exact documents are unknown.
  ExistenceFilter filter = 5;
}
}
 */

// queue
//
// error
// close
// backoff
// idle
//
// write stream

class WatchStream {
 public:
  WatchStream(const DatabaseInfo& database_info,
              CredentialsProvider* const credentials_provider)
      : database_info_{database_info_},
        credentials_provider_{credentials_provider},
        stub_{CreateStub()} {
    Start();
  }

  void Start() {
    // TODO: on error state
    // TODO: state transition details

    state_ = State::Auth;
    // TODO: delegate?

    const bool do_force_refresh = false;
    credentials_provider_->GetToken(do_force_refresh,
                                    [this](util::StatusOr<Token> maybe_token) {
                                      Authenticate(maybe_token);
                                    });
  }

  // Call may be closed due to:
  // - error;
  // - idleness;
  // - network disable/reenable
  void Authenticate(const util::StatusOr<Token>& maybe_token) {
    if (state_ == State::Stopped) {
      // Streams can be stopped while waiting for authorization.
      return;
    }
    // TODO: state transition details
    if (!maybe_token.ok()) {
      // TODO: error handling
      return;
    }

    const auto context = [FSTDatastore
        createGrpcClientContextWithDatabaseID:token:maybe_token.ValueOrDie()];
    call_ = stub_.PrepareCall(
        context, "/google.firestore.v1beta1.Firestore/Listen" /*,queue*/);
    // TODO: if !call_
    // callback filter

    state_ = State::Open;
    // notifystreamopen
  }

  // TODO: Close

 private:
  enum class State {
    Initial,
    Auth,
    Open,
    // Error
    // Backoff
    Stopped
  };

  grpc::GenericStub CreateStub() const {
    return grpc::GenericStub{grpc::CreateChannel(
        database_info_->host(),
        grpc::SslCredentials(grpc::SslCredentialsOptions()))};
  }

  State state_{State::Initial};
  DatabaseInfo* database_info_;
  CredentialsProvider* credentials_provider_;
  std::shared_ptr<grpc::GenericStub> stub_;
  std::unique_ptr<grpc::ClientAsyncReaderWriter> call_;
};

}  // namespace remote
}  // namespace firestore
}  // namespace firebase

#endif  // FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_REMOTE_GRPC_STREAM_H_
