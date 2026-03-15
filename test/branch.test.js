'use strict';

const { createTempDir, cleanupDir } = require('./fixtures/setup');
const binding = require('..');

describe('Branch', () => {
  test('should have branch constants', () => {
    expect(binding.Branch.LOCAL).toBeDefined();
    expect(binding.Branch.REMOTE).toBeDefined();
    expect(binding.Branch.ALL).toBeDefined();
  });

  test('should have branch functions', () => {
    expect(binding.Branch.create).toBeDefined();
    expect(binding.Branch.list).toBeDefined();
    expect(binding.Branch.lookup).toBeDefined();
    expect(binding.Branch.current).toBeDefined();
    expect(binding.Branch.rename).toBeDefined();
  });
});
