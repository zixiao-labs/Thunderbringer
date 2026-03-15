'use strict';

const binding = require('../index');

// Re-export all native modules
module.exports = {
  // Core
  Repository: binding.Repository,
  Status: binding.Status,

  // Staging & Commits
  Index: binding.Index,
  Commit: binding.Commit,
  Signature: binding.Signature,
  Revwalk: binding.Revwalk,

  // References & Branches
  Reference: binding.Reference,
  Branch: binding.Branch,

  // Diff & Blame
  Diff: binding.Diff,
  Patch: binding.Patch,
  Blame: binding.Blame,

  // Remote
  Remote: binding.Remote,

  // Operations
  Merge: binding.Merge,
  Tag: binding.Tag,
  Stash: binding.Stash,
  Checkout: binding.Checkout,
  Config: binding.Config,

  // Metadata
  libgit2Version: binding.libgit2Version,
};
