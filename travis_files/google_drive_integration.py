from __future__ import print_function

import io
import os.path
import pickle
import sys

from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseDownload

TOKEN = 'travis_files/token.pickle'
DOWNLOAD_LINK = "travis_results/download_link.txt"


class GoogleDriveService:

    def __init__(self):
        self.creds = self._auth()
        self.google_drive_service = build('drive', 'v3', credentials=self.creds)

    def _auth(self):
        """
        Authenticate google drive credentails.
        """
        creds = None
        # The file token.pickle stores the user's access and refresh tokens, and is
        # created automatically when the authorization flow completes for the first
        # time.
        if os.path.exists(TOKEN):
            with open(TOKEN, 'rb') as token:
                creds = pickle.load(token)
        else:
            sys.exit("can't find token.pickle")
        return creds

    def download_result(self, file_id, file_name):

        file = self.google_drive_service.files().get(fileId=file_id, fields='webContentLink').execute()
        print('Download test result at: %s' % file['webContentLink'])

        # create a text file with the download link (later to post as a comment)
        with open(DOWNLOAD_LINK, "w") as f:
            f.write(file['webContentLink'])

        file_request = self.google_drive_service.files().get_media(fileId=file_id)

        if file_request:
            # there are results
            fh = io.BytesIO()
            downloader = MediaIoBaseDownload(fh, file_request)
            done = False

            while done is False:
                status, done = downloader.next_chunk()
                print("Download %d%%." % int(status.progress() * 100))

            with io.open('travis_results/%s' % file_name, 'wb') as f:
                fh.seek(0)
                f.write(fh.read())

            passed = file_name[
                     file_name.index('-') + 1: file_name.index('.')] == "True"  # extract the boolean from file name
            return passed
        else:
            sys.exit('ERROR download file: %s with id: %s' % (file_id, file_name))

    def find_file_id(self, sha):
        """
        Looking for the commit id in google drive
        :param sha: the commit id to look for.
        :return: The id and the file name as in google drive.
        """
        results = self.google_drive_service.files().list(
            fields="nextPageToken, files(id, name)").execute()
        files = results.get('files', [])
        # looking for the file with that name(what comes before the delimiter)
        filtered = list(filter(lambda item: item['name'].split('-')[0] == sha, files))
        if filtered:
            return filtered[0]['id'], filtered[0]['name']
        else:
            return False, False
