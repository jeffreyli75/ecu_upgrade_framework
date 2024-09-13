
import datetime
import os
from uds_client import UDSClient
from threading import Thread

class UpgradeJob(Thread):
    def __init__(self, job_name, params):
        self.job_name = job_name
        self.params = params
        if os.path.exists(os.path.join(params["data_path"], params["package"])) and \
                os.path.exists(os.path.join(params["data_path"],params["package_checksum"])):
            self.upgrade_success = True
            self.upgrade_done = False
        else:
            print("firmware file not found")
            self.upgrade_success = False
            self.upgrade_done = True

    def run(self):
        client = UDSClient.CreateClient(self.params['name'], self.params['proto'])
        if self.params["upgrade_type"] == 'program':
            result, reson = client.ProgramEcu(os.path.join(self.params["data_path"], self.params["package"]))
        else:
            result, reson = client.ProgramSoc(os.path.join(self.params["data_path"], self.params["package"]))
        self.upgrade_success = result
        self.upgrade_done = True

    def is_running(self):
        if self.upgrade_done:
            return False
        if not self.upgrade_success:
            raise Exception(f"job failed {self.job_name}")
        return not self.upgrade_done

    def is_success(self):
        return self.upgrade_success
