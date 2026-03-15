'use strict';

const binding = require('..');

describe('Module', () => {
  test('should load native binding', () => {
    expect(binding).toBeDefined();
  });

  test('should expose libgit2 version', () => {
    expect(binding.libgit2Version).toBeDefined();
    expect(typeof binding.libgit2Version).toBe('string');
    expect(binding.libgit2Version).toMatch(/^\d+\.\d+\.\d+/);
  });

  test('should expose all namespaces', () => {
    expect(binding.Repository).toBeDefined();
    expect(binding.Status).toBeDefined();
    expect(binding.Index).toBeDefined();
    expect(binding.Commit).toBeDefined();
    expect(binding.Signature).toBeDefined();
    expect(binding.Revwalk).toBeDefined();
    expect(binding.Reference).toBeDefined();
    expect(binding.Branch).toBeDefined();
    expect(binding.Diff).toBeDefined();
    expect(binding.Patch).toBeDefined();
    expect(binding.Blame).toBeDefined();
    expect(binding.Remote).toBeDefined();
    expect(binding.Merge).toBeDefined();
    expect(binding.Tag).toBeDefined();
    expect(binding.Stash).toBeDefined();
    expect(binding.Checkout).toBeDefined();
    expect(binding.Config).toBeDefined();
  });
});
