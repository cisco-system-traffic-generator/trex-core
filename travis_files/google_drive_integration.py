from __future__ import print_function

import io
import pickle
import os.path
import sys

from googleapiclient.discovery import build

# If modifying these scopes, delete the file token.pickle.
from googleapiclient.http import MediaFileUpload, MediaIoBaseDownload

SCOPES = ['https://www.googleapis.com/auth/drive.readonly']

TOKEN = 'travis_files/token.pickle'
DOWNLOAD_LINK =  "travis_files/download_link.txt"


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
        # If there are no (valid) credentials available, let the user log in.
        return creds

    def print_n_files(self, n):
        # Call the Drive v3 API
        results = self.google_drive_service.files().list(
            pageSize=n, fields="nextPageToken, files(id, name)").execute()
        items = results.get('files', [])

        if not items:
            print('No files found.')
        else:
            print('Files:')
            for item in items:
                print(u'{0} ({1})'.format(item['name'], item['id']))

    def download_result(self, commit_id):
        """
        Download the html file from google drive using the commit id
        :param commit_id: The connit id to download.
        :return: the html file and the results as a bool type.
        """
        file_id, file_name = self.find_file_id(commit_id)
        if not file_id:
            return False, False

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

            # os.mkdir('Travis_Results/%s' % pull_request_number)  # create a folder for the specific test result

            with io.open('%s' % file_name, 'wb') as f:
                fh.seek(0)
                f.write(fh.read())

            passed = file_name[
                     file_name.index('-') + 1: file_name.index('.')] == "True"  # extract the boolean from file name
            return True, passed

        else:
            # there are no results yet
            print("Didn't find the file id!")
            return False, False

    def find_file_id(self, commit_id):
        """
        Looking for the commit id in google drive
        :param commit_id: the commit id to look for.
        :return: The id and the file name as in google drive.
        """
        results = self.google_drive_service.files().list(
            fields="nextPageToken, files(id, name)").execute()
        files = results.get('files', [])
        # looking for the file with that name(what comes before the delimiter)
        filtered = list(filter(lambda item: item['name'].split('-')[0] == commit_id, files))
        if filtered:
            return filtered[0]['id'], filtered[0]['name']
        else:
            return False, False
