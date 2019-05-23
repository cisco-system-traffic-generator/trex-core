import argparse
import os
import sys
import time

from google_drive_integration import GoogleDriveService

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--last-check", dest="last_check", required=True,
                        help="True/False if it is the last script checking results")
    args = parser.parse_args()
    pr_num, sha = int(os.environ['PR_NUM']), os.environ['SHA']

    gdservice = GoogleDriveService()
    total_sleep, sleeping_between_download = 45, 1  # in min
    os.mkdir('travis_results')

    for _ in range(total_sleep // sleeping_between_download):
        # check if there are new results
        print('-' * 42)
        print('looking for test results for pull request number %d for sha: %s' % (pr_num, sha))
        res, passed = gdservice.download_result(sha)
        if res:
            print("found the results!")
            if not passed:
                sys.exit("test failed, more information can be found in html file")
            else:
                print("test passed, look for a comment with html download link at the pull request page")
                sys.exit(0)

        print("didn't find results, sleeping for %d min..." % sleeping_between_download)
        time.sleep(sleeping_between_download * 60)

    print('-' * 42)
    if args.last_check:
        sys.exit('waiting for results timeout! you may restart the build later')
    else:
        print('did not find results at this run')
