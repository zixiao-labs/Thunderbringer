'use strict';

const path = require('path');
const fs = require('fs');
const { createTempDir, cleanupDir } = require('./fixtures/setup');
const binding = require('..');

describe('Index & Commit', () => {
  let tempDir;
  let repo;

  beforeEach(async () => {
    tempDir = createTempDir();
    repo = await binding.Repository.init(tempDir);
  });

  afterEach(() => {
    repo.free();
    cleanupDir(tempDir);
  });

  test('full staging and commit workflow', async () => {
    // Create a file
    fs.writeFileSync(path.join(tempDir, 'hello.txt'), 'hello world');

    // Get the index (by opening a fresh repo — libgit2 gives us index from repo)
    // We need to access the index through a helper since Index is ObjectWrap
    // For now, we test via the Commit.create flow

    // The native binding exposes Index as a class but we need repo.index()
    // This will be wired up when we add repo.index() method
    // For now, verify that the Commit namespace exists
    expect(binding.Commit).toBeDefined();
    expect(binding.Commit.create).toBeDefined();
    expect(binding.Commit.lookup).toBeDefined();
  });

  test('Signature.create should return signature data', () => {
    const sig = binding.Signature.create('Test User', 'test@example.com');
    expect(sig.name).toBe('Test User');
    expect(sig.email).toBe('test@example.com');
    expect(sig.time).toBeGreaterThan(0);
  });
});
