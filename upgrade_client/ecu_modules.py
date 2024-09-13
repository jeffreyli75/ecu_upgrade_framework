
import os
import sys
import shutil
import time
import json
import re

import upgrade_job

class Module():
  def __init__(self, json_module):
    self.name = json_module['name']
    self.config = json_module['config']
    self.dependens = []
    if 'dependens' in json_module:
      self.dependens = json_module['dependens']
    self.dependen_modules = []
    self.upgrade_done = False
    self.in_upgrading = False
    self.upgrade_success = True
    self.upgrade_job = None

  def upgrade_has_done(self):
    return self.upgrade_done

  def check_upgrade_done(self):
    if self.in_upgrading:
          if not self.upgrade_job.is_running():
              self.upgrade_done = True
              self.in_upgrading = False
              self.upgrade_success = self.upgrade_job.is_success()
              print(f"{self.name} job done, result {self.upgrade_success}")

  def is_upgrading(self):
    return self.in_upgrading

  def upgrade(self, base_params):
     if self.upgrade_done or self.in_upgrading:
        return
     self.in_upgrading = True
     params = {**base_params, "MODULE_CONFIG": self.config}
     module_param = {"MODULE_PARAM": params}
     self.upgrade_job = upgrade_job.UpgradeJob(self.name, module_param)

  def is_success(self):
    return self.upgrade_success

class Modules():
  def __init__(self, json_config):
    self.config = json_config
    self.modules = {}
    for module_data in self.config['modules']:
        module_obj = Module(module_data)
        self.modules[module_data['name']] = module_obj

    for name, module in self.modules.items():
        for dependen in module.dependens:
            if not dependen in self.modules.keys():
               raise Exception(name, "depend on invalid depend module: " + dependen)
            if name != dependen:
                dep_module = self.modules[dependen]
                module.dependen_modules.append(dep_module)
  
  def upgrade_module(self, module):
     if module.upgrade_has_done() or module.is_upgrading():
        return
     all_depeden_done = True
     for dep_module in module.dependen_modules:
         if not dep_module.upgrade_has_done():
             all_depeden_done = False
             self.upgrade_module(dep_module)
     if all_depeden_done:
         module.upgrade(self.module_job_params)
     
  def upgrade(self):
    self.module_job_params = {}
    exit_upgrade = False
    upgrade_error = False
    while not exit_upgrade:
      try:
        all_done = True
        if not upgrade_error:
          for module in self.modules.values():
            self.upgrade_module(module)
          time.sleep(1)
          for module in self.modules.values():
            module.check_upgrade_done()
            if not module.upgrade_has_done():
              all_done = False
            if not module.is_success():
              upgrade_error = True
        else:
          time.sleep(1)
          for module in self.modules.values():
            module.check_upgrade_done()
            if module.is_upgrading():
              all_done = False
        if all_done:
          exit_upgrade = True
      except Exception as e:
        print(e)
        upgrade_error = True
    if upgrade_error:
        raise Exception("upgrade failed")

