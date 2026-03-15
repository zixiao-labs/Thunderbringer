#pragma once

#include <napi.h>
#include <git2.h>

namespace thunderbringer {

// Credential callback bridge for use with ThreadSafeFunction.
// Currently provides basic credential types; TSFN bridging for
// JS-driven interactive credentials will be added in a later phase.

// Create userpass plaintext credential
inline int create_userpass_credential(git_credential** out,
                                      const char* username,
                                      const char* password) {
  return git_credential_userpass_plaintext_new(out, username, password);
}

// Create SSH key credential
inline int create_ssh_key_credential(git_credential** out,
                                     const char* username,
                                     const char* public_key_path,
                                     const char* private_key_path,
                                     const char* passphrase) {
  return git_credential_ssh_key_new(out, username,
                                    public_key_path, private_key_path,
                                    passphrase);
}

}  // namespace thunderbringer
