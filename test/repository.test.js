'use strict';

const path = require('path');
const fs = require('fs');
const { createTempDir, cleanupDir } = require('./fixtures/setup');
const binding = require('..');

describe('Repository', () => {
  let tempDir;

  beforeEach(() => {
    tempDir = createTempDir();
  });

  afterEach(() => {
    cleanupDir(tempDir);
  });

  describe('init', () => {
    test('should initialize a new repository', async () => {
      const repo = await binding.Repository.init(tempDir);
      expect(repo).toBeDefined();
      expect(repo.path()).toContain('.git');
      expect(repo.isBare()).toBe(false);
      expect(repo.isEmpty()).toBe(true);
      repo.free();
    });

    test('should initialize a bare repository', async () => {
      const bareDir = path.join(tempDir, 'bare.git');
      fs.mkdirSync(bareDir);
      const repo = await binding.Repository.init(bareDir, true);
      expect(repo.isBare()).toBe(true);
      repo.free();
    });
  });

  describe('open', () => {
    test('should open an existing repository', async () => {
      await binding.Repository.init(tempDir);
      const repo = await binding.Repository.open(tempDir);
      expect(repo).toBeDefined();
      expect(repo.workdir()).toBeTruthy();
      repo.free();
    });

    test('should reject for non-existent path', async () => {
      await expect(binding.Repository.open('/nonexistent/path'))
        .rejects.toThrow();
    });
  });

  describe('discover', () => {
    test('should discover repository from subdirectory', async () => {
      await binding.Repository.init(tempDir);
      const subDir = path.join(tempDir, 'a', 'b', 'c');
      fs.mkdirSync(subDir, { recursive: true });
      const discovered = await binding.Repository.discover(subDir);
      expect(discovered).toContain('.git');
    });
  });

  describe('head', () => {
    test('should return null for empty repository', async () => {
      const repo = await binding.Repository.init(tempDir);
      expect(repo.headUnborn()).toBe(true);
      const head = repo.head();
      expect(head).toBeNull();
      repo.free();
    });
  });

  describe('state', () => {
    test('should return clean state for new repo', async () => {
      const repo = await binding.Repository.init(tempDir);
      expect(repo.state()).toBe(0); // GIT_REPOSITORY_STATE_NONE
      repo.free();
    });
  });
});
