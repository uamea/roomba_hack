#!/bin/bash

################################################################################

# Initialize uv package manager

if [ ! -f "pyproject.toml" ]; then
    echo "Initializing the project..."
    uv init
    uv python install 3.12
    uv python pin 3.12
else 
    echo "Project already initialized."
fi

if [ ! -d "/root/.venv" ]; then
    echo "Initializing the venv environment..."
    uv sync
fi 

