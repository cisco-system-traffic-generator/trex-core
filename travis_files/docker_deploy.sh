#!/usr/bin/env bash

if (( $# < 3 )); then
    echo "Usage: docker_deploy.sh [ORG/REPO] [FROM_TAG] [TO_TAG]..."
    echo
    echo "You need at least to specify:"
    echo "    repository name (organization/repository)"
    echo "    current tag"
    echo "    at least one tag to push"
    exit 1
fi

for ((i = 3; i <= $#; i++ )); do
    echo "Pushing $1:$2 as ${!i}"
    docker tag "$1:$2" "$1:${!i}"
    docker push "$1:${!i}"
done
