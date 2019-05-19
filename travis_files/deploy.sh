#!/bin/sh

#setup_git() {
#  git config --global user.email "elados1163@gmail.com"
#  git config --global user.name "elados93"
#}
#
#commit_website_files() {
#  git checkout -b gh-pages
#  git add . *.html
#  git commit --message "Travis build that"
#}
#
#upload_files() {
#  git remote add origin https://elados93:${GITHUB_TOKEN}@github.com/elados93/Dummy-Project.git --branch=gh-pages gh-pages > /dev/null 2>&1
#  echo "Before Push"
#  cd gh-pages
#  git push --quiet --set-upstream origin gh-pages
#}

comment_link_to_results() {
    DOWNLOAD_LINK=$(<Results/download_link.txt)
    echo ${DOWNLOAD_LINK}
    COMMENT="Download test results [here](${DOWNLOAD_LINK})"
    curl -H "Authorization: token $GITHUB_TOKEN" -X POST -d "{\"body\": \"$COMMENT\"}" "https://api.github.com/repos/${TRAVIS_REPO_SLUG}/issues/${TRAVIS_PULL_REQUEST}/comments"
    rm Results/download_link.txt
}

#setup_git
#commit_website_files
#upload_files

comment_link_to_results
