import sys
import os
import time

from google_drive_integration import GoogleDriveService


def get_latest_sha(pr_number):
    command = """
    git ls-remote https://github.com/cisco-system-traffic-generator/trex-core refs/pull/%d/head | awk '{print $1;}'""" % pr_number
    return os.popen(command).read().strip()


def main():
    if len(sys.argv) < 2:
        sys.exit('need pr number as arg!')
    sha = get_latest_sha(int(sys.argv[1]))  # sent by .travis.yml as string
    is_last = sys.argv[2] == 'True'  # sent by .travis.yml as True / False

    gdservice = GoogleDriveService()
    total_sleep, sleeping_between_download = 45, 1  # in min

    for _ in range(total_sleep // sleeping_between_download):
        # check if there are new results
        print('-' * 42)
        print('looking for test results for pull request for sha: %s' % sha)
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
    if is_last:
        print('-' * 42)
        sys.exit('waiting for results timeout! you may restart the build later')
    else:
        print('-' * 42)
        print('did not find results at this run')


if __name__ == '__main__':
    main()
