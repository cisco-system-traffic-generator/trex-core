from __future__ import print_function

import io
import os.path
import pickle
import sys

from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseDownload

from apiclient import errors

TOKEN = 'travis_files/token.pickle'
DOWNLOAD_LINK = "travis_files/download_link.txt"


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

    def rename_file(self, file_id, new_name):
        try:
            file = {'name': new_name}

            updated_file = self.google_drive_service.files().update(
                fileId=file_id,
                body=file).execute()
            print('renamed to: %s' % new_name)
        except errors.HttpError as error:
            print("An error occurred during rename file '%s':\n%s" % (new_name, error))

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

            with io.open('travis_files/%s' % file_name, 'wb') as f:
                fh.seek(0)
                f.write(fh.read())

            file_args = file_name.split(',')
            file_args[4] = 'True.html'
            self.rename_file(file_id, ','.join(file_args))

            passed = file_name.split(',')[3] == "True"  # extract the boolean from file name
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
        # looking for the file with that sha(file format: pr,sha,date,passed,was_taken.html)

        for file in files:
            if sha in file['name']:
                return file
        return False
