'use strict';

class GitError extends Error {
  constructor(message, code) {
    super(message);
    this.name = 'GitError';
    this.code = code;
  }
}

class NotFoundError extends GitError {
  constructor(message) {
    super(message, 'ENOTFOUND');
    this.name = 'NotFoundError';
  }
}

class ConflictError extends GitError {
  constructor(message) {
    super(message, 'ECONFLICT');
    this.name = 'ConflictError';
  }
}

module.exports = { GitError, NotFoundError, ConflictError };
