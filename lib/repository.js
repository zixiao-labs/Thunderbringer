'use strict';

const binding = require('../index');
const Repository = binding.Repository;

// Re-export static methods with cleaner names
module.exports = {
  /**
   * Open an existing repository.
   * @param {string} path - Path to the repository
   * @returns {Promise<Repository>}
   */
  open: (path) => Repository.open(path),

  /**
   * Initialize a new repository.
   * @param {string} path - Path for the new repository
   * @param {boolean} [bare=false] - Create a bare repository
   * @returns {Promise<Repository>}
   */
  init: (path, bare = false) => Repository.init(path, bare),

  /**
   * Discover a repository by walking up from a start path.
   * @param {string} startPath - Path to start searching from
   * @returns {Promise<string>} Path to the discovered repository
   */
  discover: (startPath) => Repository.discover(startPath),

  /**
   * The native Repository class.
   */
  Repository,
};
