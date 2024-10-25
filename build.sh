#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define the build directory
BUILD_DIR="build"

mkdir -p $BUILD_DIR

cd $BUILD_DIR

cmake .. && make

cp -f parallel ../parallel

echo "Build completed successfully."