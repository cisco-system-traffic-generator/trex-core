#!/bin/sh

comment_link_to_results() {
    DOWNLOAD_LINK=$(<Travis_Results/download_link.txt)
    echo ${DOWNLOAD_LINK}
    COMMENT="Download test results [here](${DOWNLOAD_LINK})"
    curl -H "Authorization: token $GITHUB_TOKEN" -X POST -d "{\"body\": \"$COMMENT\"}" "https://api.github.com/repos/${TRAVIS_REPO_SLUG}/issues/${TRAVIS_PULL_REQUEST}/comments"
    rm Travis_Results/download_link.txt
}

comment_link_to_results
