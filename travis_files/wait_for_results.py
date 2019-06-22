import os
import sys
import time
import json

from google_drive_integration import GoogleDriveService

slug = "cisco-system-traffic-generator/trex-core"

def get_started_time():
    build_id = os.environ['BUILD_ID']
    command = "curl https://api.travis-ci.org/repos/%s/builds/%s" % (slug, build_id)
    output = os.popen(command).read()
    build_json = json.loads(output)
    return build_json['started_at']


if __name__ == '__main__':

    last_check = os.environ['IS_LAST'] == "True"
    pr_num, sha = int(os.environ['PR_NUM']), os.environ['SHA']
    started_time = get_started_time()

    gd_service = GoogleDriveService()
    total_sleep, sleeping_between_download = 45, 1  # in min

    for i in range(total_sleep // sleeping_between_download):
        # check if there are new results in google drive
        print('-' * 42)
        print('looking for test results for pull request number %d, sha: %s' % (pr_num, sha))
        file_data = gd_service.find_file_id(sha, started_time)

        if file_data:
            passed = file_data['name'].split(',')[3].strip('.html') == "True"  # extract the boolean from file name
            if passed:
                print("test passed, look for a comment with html download link at the pull request page")
                sys.exit(0)
            else:
                sys.exit("test failed, more information can be found in html file")

        print("didn't find results, sleeping for %d min..." % sleeping_between_download)
        time.sleep(sleeping_between_download * 60)

    print('-' * 42)
    if last_check:
        sys.exit('waiting for results timeout! you may restart the build later')
    else:
        print('did not find results at this run, another job will continue seeking')
