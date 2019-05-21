#!/bin/sh

comment_link_to_results() {
    DOWNLOAD_LINK=$(<Travis_Results/download_link.txt)
    echo ${DOWNLOAD_LINK}
    COMMENT="Download your test results [here](${DOWNLOAD_LINK}) (html file)"
    curl -H "Authorization: token $GITHUB_COMMENT_TOKEN" -X POST -d "{\"body\": \"$COMMENT\"}" "https://api.github.com/repos/cisco-system-traffic-generator/trex-core/issues/$TRAVIS_PULL_REQUEST/comments"
    rm Travis_Results/download_link.txt
}

comment_link_to_results
