'use strict';

const path = require('path');

let binding;

try {
  // Try prebuild first
  binding = require('node-gyp-build')(path.join(__dirname));
} catch {
  try {
    // Fall back to cmake-js build output
    binding = require(path.join(__dirname, 'build', 'Release', 'thunderbringer.node'));
  } catch {
    binding = require(path.join(__dirname, 'build', 'Debug', 'thunderbringer.node'));
  }
}

module.exports = binding;
