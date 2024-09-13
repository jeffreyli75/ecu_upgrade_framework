import os, logging
import logging.config
from argparse import ArgumentParser as Parser
import tarfile
import tempfile
import json
from ecu_modules import Modules

def read_file(file_path):
  content = ''
  if os.path.exists(file_path):
    fd = open(file_path, 'r')
    content = fd.read()
    fd.close()
  else:
    raise Exception("file " + file_path + " not exist")
  return content

if __name__ == "__main__":
  base_path = os.path.dirname(os.path.abspath(__file__))
  log_folder = os.path.join(base_path, 'log')
  if not os.path.exists(log_folder):
    os.makedirs(log_folder)
  logging.config.fileConfig(os.path.join(base_path, 'logging.conf'))
  logger = logging.getLogger('upgrade_client')
  
  parser = Parser(description ='Upgrade ECUs through DoIP.')

  parser.add_argument('filename', metavar='FILENAME', action='store',
                      help ='Firmware file name')

  args = parser.parse_args()
  if not os.path.exists(args.filename):
    exit(1)
  temp_dir = os.path.join(tempfile.gettempdir(), '__firmwares__')
  os.mkdir(temp_dir)
  try:
    with tarfile.open(args.filename, 'r:gz') as tar:
      tar.extractall(temp_dir)
    config_content = read_file(os.path.join(temp_dir, 'config.json'))
    config_json = json.loads(config_content)
    modules = Modules(config_json)
    modules.upgrade()
  except tarfile.ExtractError:
    exit(1)

