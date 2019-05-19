import sys
import os
import time

from google_drive_integration import GoogleDriveService


def get_latest_sha(pr_number):
    command = """
    git ls-remote https://github.com/elados93/trex-core refs/pull/%d/head | awk '{print $1;}'""" % pr_number
    return os.popen(command).read()


def main():
    if len(sys.argv) < 1:
        sys.exit('need pr number as arg!')
    sha = get_latest_sha(int(sys.argv[1]))  # sent by .travis.yml as string
    gdservice = GoogleDriveService()
    sleeping_between_download = 1

    while True:
        # check if there are new results
        print('looking for test results for pull request for sha: %s' % sha)
        print('-' * 42)
        res, passed = gdservice.download_result(sha)
        if res:
            print("found the results!")
            print("test passes: %s" % passed)
            if not passed:
                sys.exit("test failed, more information can be found in html file")
            else:
                print("test passed, look for a comment with html download link at the pull request page")
            break

        print("didn't find results, sleeping for %d min..." % sleeping_between_download)
        time.sleep(sleeping_between_download * 60)


if __name__ == '__main__':
    main()
