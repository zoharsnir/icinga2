#!/bin/bash
set -exo pipefail

cd "$(mktemp -d)"
git clone https://git.icinga.com/packaging/deb-icinga2.git
cd deb-icinga2

if [ -n "$DRONE_BRANCH" ]; then
	export ICINGA_BUILD_UPSTREAM_BRANCH="$DRONE_BRANCH"
fi

if [ -n "$DRONE_TAG" ]; then
	export ICINGA_BUILD_UPSTREAM_BRANCH="$DRONE_TAG"
fi

export ICINGA_BUILD_PROJECT=icinga2
export ICINGA_BUILD_TYPE=snapshot
export UPSTREAM_GIT_URL="$DRONE_REMOTE_URL"

icinga-build-deb-source
icinga-build-deb-binary
