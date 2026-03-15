'use strict';

const path = require('path');
const fs = require('fs');
const { createTempDir, cleanupDir } = require('./fixtures/setup');
const binding = require('..');

describe('Status', () => {
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

  test('should return empty status for clean repo', async () => {
    const status = await binding.Status.getStatus(repo);
    expect(status).toEqual([]);
  });

  test('should detect new untracked file', async () => {
    fs.writeFileSync(path.join(tempDir, 'test.txt'), 'hello');
    const status = await binding.Status.getStatus(repo);
    expect(status.length).toBe(1);
    expect(status[0].path).toBe('test.txt');
    expect(status[0].status & binding.Status.WT_NEW).toBeTruthy();
  });

  test('should detect file status by path', async () => {
    fs.writeFileSync(path.join(tempDir, 'test.txt'), 'hello');
    const flags = binding.Status.getFileStatus(repo, 'test.txt');
    expect(flags & binding.Status.WT_NEW).toBeTruthy();
  });

  test('should have status constants', () => {
    expect(binding.Status.CURRENT).toBeDefined();
    expect(binding.Status.INDEX_NEW).toBeDefined();
    expect(binding.Status.WT_NEW).toBeDefined();
    expect(binding.Status.WT_MODIFIED).toBeDefined();
    expect(binding.Status.IGNORED).toBeDefined();
  });
});
