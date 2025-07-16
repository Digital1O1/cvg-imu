#!/bin/bash
set -e

# 1. Create the named FIFO pipe if it does not exist
PIPE_PATH="/tmp/hmdop_laser_pipe"
if [ ! -p "$PIPE_PATH" ]; then
    echo "Creating FIFO pipe at $PIPE_PATH..."
    mkfifo "$PIPE_PATH"
else
    echo "FIFO pipe $PIPE_PATH already exists."
fi

# 2. Install necessary packages
OS="$(uname)"
echo "Detected OS: $OS"
if [ "$OS" = "Darwin" ]; then
    # macOS
    if ! command -v brew >/dev/null 2>&1; then
        echo "Homebrew not found. Please install Homebrew first."
        exit 1
    fi
    echo "Installing libiio via Homebrew..."
    brew install libiio || true
    echo "Installing make and gcc via Homebrew..."
    brew install make gcc || true
else
    # Assume Debian/Ubuntu
    echo "Updating package list and installing libiio and build tools..."
    sudo apt-get update
    sudo apt-get install -y libiio-dev build-essential
fi

# 3. Compile head_tracking using Makefile
if [ -f Makefile ]; then
    echo "Compiling head_tracking using Makefile..."
    make head_tracking
    echo "Build complete."
else
    echo "Makefile not found! Please ensure you are in the correct directory."
    exit 1
fi

echo "Setup complete."
