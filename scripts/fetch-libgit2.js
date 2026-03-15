'use strict';

const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const libgit2Dir = path.join(__dirname, '..', 'deps', 'libgit2');

if (fs.existsSync(path.join(libgit2Dir, 'CMakeLists.txt'))) {
  console.log('libgit2 submodule already present.');
  process.exit(0);
}

console.log('Initializing libgit2 submodule...');
try {
  execSync('git submodule update --init --recursive', {
    cwd: path.join(__dirname, '..'),
    stdio: 'inherit',
  });
  console.log('libgit2 submodule initialized.');
} catch (err) {
  console.error('Failed to initialize submodule:', err.message);
  console.error('Please run: git submodule update --init --recursive');
  process.exit(1);
}
