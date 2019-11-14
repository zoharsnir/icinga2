#!/bin/bash
set -exo pipefail

CMAKE_CURRENT_BINARY_DIR="$1"
GO="$2"
DEP="$3"

cd "$(dirname "$0")"

export GOPATH="${CMAKE_CURRENT_BINARY_DIR}/GOPATH"
mkdir -p "${GOPATH}/src/github.com/Icinga/icinga2/go"
ln -vfs "$(pwd)" "${GOPATH}/src/github.com/Icinga/icinga2/go/procmgr"

pushd "${GOPATH}/src/github.com/Icinga/icinga2/go/procmgr"
"${DEP}" ensure
popd

"$GO" build -o "${CMAKE_CURRENT_BINARY_DIR}/icinga2-procmgr" github.com/Icinga/icinga2/go/procmgr
