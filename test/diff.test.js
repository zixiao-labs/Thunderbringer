'use strict';

const binding = require('..');

describe('Diff', () => {
  test('should have diff delta constants', () => {
    expect(binding.Diff.DELTA_ADDED).toBeDefined();
    expect(binding.Diff.DELTA_DELETED).toBeDefined();
    expect(binding.Diff.DELTA_MODIFIED).toBeDefined();
    expect(binding.Diff.DELTA_RENAMED).toBeDefined();
    expect(binding.Diff.DELTA_UNTRACKED).toBeDefined();
  });

  test('should have diff functions', () => {
    expect(binding.Diff.indexToWorkdir).toBeDefined();
    expect(binding.Diff.treeToTree).toBeDefined();
    expect(binding.Diff.treeToIndex).toBeDefined();
  });
});
