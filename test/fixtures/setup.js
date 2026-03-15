'use strict';

const path = require('path');
const fs = require('fs');
const os = require('os');

function createTempDir(prefix = 'thunderbringer-test-') {
  return fs.mkdtempSync(path.join(os.tmpdir(), prefix));
}

function cleanupDir(dirPath) {
  if (dirPath && fs.existsSync(dirPath)) {
    fs.rmSync(dirPath, { recursive: true, force: true });
  }
}

module.exports = { createTempDir, cleanupDir };
