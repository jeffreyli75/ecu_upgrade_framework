from typing import Any
from udsoncan import latest_standard
from udsoncan.typing import ClientConfig
from udsoncan.client import Client
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.exceptions import *
from udsoncan.services import *
from udsoncan.common.DidCodec import AsciiCodec, DidCodec
from udsoncan.common import MemoryLocation
from doipclient import DoIPClient
from doipclient.connectors import DoIPClientUDSConnector
from hex_parser import HexParser
from can.interfaces.socketcan import SocketcanBus
from can.interfaces.pcan import PcanBus
import time, datetime
import udsoncan
import ctypes, os, platform, sys
import logging
import isotp

# The `NumberCodec` class is a codec for encoding and decoding integers or floats into bytes with
# specified byte order and conversion functions.
class NumberCodec(DidCodec):
  # @staticmethod
  def default_enc_conversion(x : Any):
    return int(x)
  # @staticmethod
  def default_dec_conversion(x : Any):
    return x
  def __init__(self, used_bytes: int, enc_conv = default_enc_conversion, dec_conv = default_dec_conversion, big_endian = True):
      self.used_bytes = used_bytes
      self.big_endian = big_endian
      self.enc_conv = enc_conv
      self.dec_conv = dec_conv

  def encode(self, value: Any) -> bytes:
      if not isinstance(value, int) and not isinstance(value, float):
          raise ValueError("NumberCodec requires a integer for encoding")
      conv = self.enc_conv(value)
      return conv.to_bytes(self.used_bytes, byteorder = 'big' if self.big_endian else 'little')

  def decode(self, string_bin: bytes) -> Any:
      if len(string_bin) != self.used_bytes:
          raise ValueError('Trying to decode a integer of %d bytes but codec expects %d bytes' % (len(string_bin), self.used_bytes))
      
      val = int.from_bytes(string_bin, byteorder = 'big' if self.big_endian else 'little')
      return self.dec_conv(val)

  def __len__(self) -> int:
      return self.used_bytes

# The FloatCodec class provides encoding and decoding functionality for floats using specified
# conversion functions and byte order.
class FloatCodec(DidCodec):
  @staticmethod
  def default_enc_conversion(x : float):
    return int(x)
  @staticmethod
  def default_dec_conversion(x : int):
    return float(x)
  def __init__(self, used_bytes: int, enc_conv = default_enc_conversion, dec_conv = default_dec_conversion, big_endian = True):
      self.used_bytes = used_bytes
      self.big_endian = big_endian
      self.enc_conv = enc_conv
      self.dec_conv = dec_conv

  def encode(self, value: Any) -> bytes:
      if not isinstance(value, float):
          raise ValueError("FloatCodec requires a integer for encoding")
      conv = self.enc_conv(value)
      return conv.to_bytes(self.used_bytes, byteorder = 'big' if self.big_endian else 'little')

  def decode(self, string_bin: bytes) -> Any:
      if len(string_bin) != self.used_bytes:
          raise ValueError('Trying to decode a float of %d bytes but codec expects %d bytes' % (len(string_bin), self.used_bytes))
      
      val = int.from_bytes(string_bin, byteorder = 'big' if self.big_endian else 'little')
      return self.dec_conv(val)
  def __len__(self) -> int:
      return self.used_bytes

def etas_algo(level, seed, params=None):
  print("etas_algo entry with seed ")
  # 加载C库
  surfix = '.so' if platform.system() == 'Linux' else '.dll'
  platform_type = platform.system() + '_' + platform.machine()
  lib_path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), 
                          'lib', platform_type, 'security_alg') + surfix
  print(lib_path)
  security_lib = ctypes.CDLL(lib_path) # Linux系统上为动态链接库（.so文件），Windows系统上为DLL文件

  # 定义函数原型
  alg_func = security_lib.seedToKey
  alg_func.argtypes = [ctypes.c_uint, ctypes.c_uint] # 设置参数类型
  alg_func.restype = ctypes.c_uint # 设置返回值类型
  
  # 调用C函数
  result = alg_func(int.from_bytes(seed, byteorder='big'), 0x7A6B5C4D)
  print("got key ")
  print(result)
  return int.to_bytes(result, 4 , byteorder='big')

# This class `UDSClient` represents a client for communicating with an ECU using Unified Diagnostic
# Services (UDS) protocol, providing various methods for reading and writing data, performing
# routines, and programming the ECU.
class UDSClient:
  Security_Level1 : int = 3
  Security_LevelFBL : int = 5
  def __init__(self, txid : int, rxid : int, proto : str):
    """
    The function initializes a communication connection with specific parameters and configurations,
    including setting up security algorithms if needed.
    
    :param txid: The `txid` parameter in the `__init__` method is used to specify the transmission ID
    for the ISO-TP communication. It is an integer value representing the ID of the transmitter in the
    communication setup
    :type txid: int
    :param rxid: The `rxid` parameter in the `__init__` method is used to specify the receiving
    identifier for the communication. It is an integer value representing the ID of the receiver in the
    communication protocol
    :type rxid: int
    :param need_security: The `need_security` parameter in the `__init__` method is a boolean flag that
    indicates whether security features should be enabled for the communication protocol being set up.
    If `need_security` is `True`, certain security configurations and algorithms will be applied to
    enhance the security of the communication
    :type need_security: bool
    """
    self.conn_ = None
    self.__bus__ = None
    self.__txid__ = txid
    self.__rxid__ = rxid
    
    if proto == 'can':
      if platform.system() == 'Linux':
        self.__bus__ = SocketcanBus(channel = "can0", bitrate = 500000)
      else:
        self.__bus__ = PcanBus(channel = "PCAN_USBBUS1", bitrate = 500000)
      tp_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, txid, rxid)
      self.__stack__ = isotp.CanStack(bus = self.__bus__, address = tp_addr, params = isotp_params)
      self.__stack__.set_sleep_timing(0, 0)
      self.conn_ = PythonIsoTpConnection(self.__stack__)
    elif proto == 'doip':
      ecu_ip = '127.0.0.1'
      ecu_logical_address = 0x00E0
      doip_client = DoIPClient(ecu_ip, ecu_logical_address)
      self.conn_ = DoIPClientUDSConnector(doip_client)
    else:
      raise
    config = dict(udsoncan.configs.default_client_config)
    config['data_identifiers'] = { }
    config['use_server_timing'] = False
    config['request_timeout'] = 10.0
    config['p2_timeout'] = 10.0
    config['p2_star_timeout'] = 10.0
    config['security_algo'] = etas_algo
    config['security_algo_params'] = self
    
      
    self.cleint_ = Client(conn = self.conn_, request_timeout = 15, config=config)
    self.conn_.open()
    while not self.conn_.is_open():
      time.sleep(1)

  def __del__(self):
    if self.conn_:
      self.conn_.close()
    if self.__bus__:
      self.__bus__.shutdown()
  
  def GetTxid(self):
     return self.__txid__

  def ExecRoutine(self, id : int, param : bytes = None, wait_result = True) -> int:
    self.cleint_.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    self.cleint_.unlock_security_access(1)   
    self.cleint_.start_routine(id, param)       
    if not wait_result:
      self.cleint_.change_session(DiagnosticSessionControl.Session.defaultSession)
      return 0
    start = time.time()
    ret = -1
    while True:
      time.sleep(0.5)
      response = self.cleint_.get_routine_result(id)
      if response.positive and len(response.service_data.routine_status_record) > 0:
        ret = response.service_data.routine_status_record[0]
        if ret != 2:
          break
        # if response.service_data.routine_status_record[0] == 1:
        #   ret = False
        #   break
        # elif response.service_data.routine_status_record[0] == 0:
        #   ret = True
        #   break
      curr = time.time()
      if curr - start > 30.0:
        self.cleint_.change_session(DiagnosticSessionControl.Session.defaultSession)
        raise TimeoutException('操作超时')
    self.cleint_.change_session(DiagnosticSessionControl.Session.defaultSession)
    return ret

  def DownLoadFW(self, parser : HexParser, progress_cb = None) -> int:
    """
    This function downloads firmware segments using a HexParser and updates progress using a callback
    function.
    
    :param parser: The `parser` parameter in the `DownLoadFW` method is an instance of the `HexParser`
    class. It is used to parse hex data and retrieve segments of data for downloading firmware
    :type parser: HexParser
    :param progress_cb: The `progress_cb` parameter in the `DownLoadFW` method is a callback function
    that can be passed in to receive progress updates during the firmware download process. The function
    should accept a single parameter, which will be the progress value (a float) indicating the current
    progress of the download operation
    :return: The function `DownLoadFW` is returning an integer value, specifically the first element of
    the `status` list obtained from the `response` object after the download routine is completed.
    """
    progress : float = 0
    progress_step1 : float = 100.0 / len(parser.GetSegments())
    try:
      for segment in parser.GetSegments():
        start_addr, end_addr = segment
        size : int = end_addr - start_addr
        data = parser.GetData(start_addr)
        loc = MemoryLocation.MemoryLocation(start_addr, size, 32, 32)
        response = self.cleint_.request_download(loc)
        # block_size = min(response.service_data.max_length, 256)
        block_size = response.service_data.max_length - 2
        block_count = int(size / block_size)
        if size % block_size != 0:
          block_count += 1
        progress_step2 : float = progress_step1 / block_count
        self.cleint_.set_config('request_timeout', 2)
        for index in range(block_count):
          while True:
            try:
              self.cleint_.transfer_data((index + 1) & 0xff, data[index * block_size : index * block_size + block_size])
            except TimeoutException as e:
              continue
            # except Exception as e:
            #   raise e
            else:
              progress += progress_step2
              if callable(progress_cb):
                progress_cb(progress)
              break
        self.cleint_.request_transfer_exit()
        self.cleint_.set_config('request_timeout', 20)
        response = self.cleint_.start_routine(0x0202, parser.GetCheckSum(start_addr).to_bytes(4, 'big'))
        status = response.service_data.routine_status_record
        return status[0]
    except Exception as e:
      print('got exception while downloading ', e)
      logging.exception(e)
      return -1

  def ProgramEcu(self, app : str, driver = None, progress_cb = None):
    """
    The `ProgramEcu` function in Python is responsible for programming an Electronic Control Unit (ECU)
    by parsing hex files, setting addresses, downloading firmware, erasing memory, and handling
    exceptions.
    
    :param app: The `app` parameter in the `ProgramEcu` method is expected to be a string representing
    the application firmware that needs to be programmed into the Electronic Control Unit (ECU)
    :type app: str
    :param driver: The `driver` parameter in the `ProgramEcu` method is used to specify the firmware
    file for the driver. It can be provided as a string containing the path to the driver firmware file.
    If this parameter is not provided, the program will skip the driver firmware download step and
    proceed with the
    :param progress_cb: The `progress_cb` parameter in the `ProgramEcu` method is a callback function
    that can be used to report progress during the execution of the program. It is a function that takes
    a single argument, which represents the progress of the program. This callback function is optional
    and can be provided to
    :return: The `ProgramEcu` method returns a tuple containing two values: `result` and `reson`.
    `result` is a boolean indicating the success or failure of the ECU programming process, and `reson`
    is a string providing details or reasons for any failures that occurred during the process.
    """
    result = True
    reson = ''
    loop = None
    progress = 0
    fn_progress = lambda prog : progress_cb(prog) if callable(progress_cb) else None
    try:
      app_parser = HexParser(app)
      progress += 5
      fn_progress(progress)
      driver_parser = None
      if type(driver) is str:
        driver_parser = HexParser(driver)
      progress += 5
      fn_progress(progress)

    except Exception as e:
      logging.getLogger('flashdiag').warning('got exception while parsing hex file : ' + str(e))
      result = False
      reson = str(e)
    else:
    #   unicast_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, self.__txid__, self.__rxid__)
    #   multi_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, txid= 0x701, rxid= self.__rxid__)
      try:
        # self.__stack__.set_address(unicast_addr)
        # self.__stack__.params.set('default_target_address_type', isotp.TargetAddressType.Physical)
        self.cleint_.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
        response = self.cleint_.start_routine(0x0203)
        self.cleint_.change_session(DiagnosticSessionControl.Session.defaultSession)
        status = response.service_data.routine_status_record
        if len(status) == 0 or status[0] != 0:
          raise Exception("Unable to program the ECU")
        # self.__stack__.set_address(multi_addr)
        # self.__stack__.params.set('default_target_address_type', isotp.TargetAddressType.Functional)
        self.cleint_.set_config('request_timeout', 1)
        with self.cleint_.suppress_positive_response(wait_nrc=True):
          self.cleint_.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
          self.cleint_.control_dtc_setting(ControlDTCSetting.SettingType.off)
          self.cleint_.communication_control(CommunicationControl.ControlType.disableRxAndTx, 0x03)
        
        self.cleint_.set_config('request_timeout', 2)
        # self.__stack__.set_address(unicast_addr)
        # self.__stack__.params.set('default_target_address_type', isotp.TargetAddressType.Physical)
        self.cleint_.change_session(DiagnosticSessionControl.Session.programmingSession)
        time.sleep(0.1)
        self.cleint_.unlock_security_access(5)
        
        now = datetime.datetime.now()
        date = self.__Int2Bcd__(now.year % 100) + self.__Int2Bcd__(now.month % 100) + self.__Int2Bcd__(now.day % 100)
        fp = date + self.GetSurportStationNum()
        self.cleint_.write_data_by_identifier(0xF15A, fp)

        if driver_parser is None:
          progress_step1 = 80
        else:
          progress_step1 = 20
          if self.DownLoadFW(driver_parser, lambda proc : fn_progress(progress + progress_step1 * proc / 100.0)) != 0:
            raise Exception('Failed to down load driver.')
          progress += progress_step1
          progress_step1 = 60

        loc = MemoryLocation.MemoryLocation(app_parser.GetMinAddr(), app_parser.GetMemSize(), 32, 32)
        erase_addr = bytes(b'\x44') + loc.get_address_bytes() + loc.get_memorysize_bytes()
        response = self.cleint_.start_routine(0xFF00, erase_addr)
        status = response.service_data.routine_status_record
        if len(status) == 0 or status[0] != 0:
          raise Exception("Error to erase the ECU memory")

        if self.DownLoadFW(app_parser, lambda proc : fn_progress(progress + progress_step1 * proc / 100.0)) != 0:
          raise Exception('Failed to down load app fw.')

        response = self.cleint_.start_routine(0xff01)
        status = response.service_data.routine_status_record
        # if len(status) == 0 or status[0] != 0:
        #   raise Exception("Error to check the ProgrammingDependencies")
        self.cleint_.ecu_reset(ECUReset.ResetType.hardReset)
      except Exception as e:
        logging.getLogger('flashdiag').warning('got exception while download firmware : ' + str(e))
        # logging.getLogger('flashdiag').warning('callstack : ' + e.with_traceback())
        result = False
        reson = str(e)
      finally:
        # self.__stack__.set_address(multi_addr)
        # self.__stack__.params.set('default_target_address_type', isotp.TargetAddressType.Functional)
        with self.cleint_.suppress_positive_response(wait_nrc=True):
          self.cleint_.change_session(DiagnosticSessionControl.Session.defaultSession)
        # self.cleint_.clear_dtc()
        if loop is not None:
          loop.stop()
    return result, reson
  
  def ProgramSoc(self, app : str, progress_cb = None):
    result = True
    reson = ''
    progress = 0
    fn_progress = lambda prog : progress_cb(prog) if callable(progress_cb) else None
    try:
      file_app = open(app, mode="rb")
      progress += 5
      fn_progress(progress)
      self.cleint_.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
      response = self.cleint_.start_routine(0x0203)
      status = response.service_data.routine_status_record
      if len(status) == 0 or status[0] != 0:
        raise Exception("Unable to program the ECU")
      self.cleint_.change_session(DiagnosticSessionControl.Session.programmingSession)
      time.sleep(0.1)
      self.cleint_.unlock_security_access(5)
      
      now = datetime.datetime.now()
      date = self.__Int2Bcd__(now.year % 100) + self.__Int2Bcd__(now.month % 100) + self.__Int2Bcd__(now.day % 100)
      fp = date + self.GetSurportStationNum()
      self.cleint_.write_data_by_identifier(0xF15A, fp)

      filesize = os.path.getsize(app)
      dfi = udsoncan.DataFormatIdentifier(0, 0)
      response = self.cleint_.request_file_transfer(RequestFileTransfer.ModeOfOperation.AddFile, app, dfi, filesize)
      # block_size = min(response.service_data.max_length, 256)
      block_size = response.service_data.max_length - 2
      block_count = int(filesize / block_size)
      if filesize % block_size != 0:
        block_count += 1
      print("block size " + str(block_size) + " block count " + str(block_count))
      for index in range(block_count):
        data = file_app.read(block_size)
        while True:
          try:
            self.cleint_.transfer_data((index + 1) & 0xff, data)
          except TimeoutException as e:
            continue
          else:
            # progress += progress_step2
            # if callable(progress_cb):
            #   progress_cb(progress)
            break
      self.cleint_.request_transfer_exit()
      file_app.close()
      with open(app + ".md5", 'r') as f:
        md5_str = f.readline().split()[0]
      response = self.cleint_.start_routine(0x0202, bytes.fromhex(md5_str))
      status = response.service_data.routine_status_record
      
      response = self.cleint_.start_routine(0xff02)
      status = response.service_data.routine_status_record
      while True:
        response = self.cleint_.get_routine_result(0xff02)
        ret = response.service_data.routine_status_record[0]
        if ret != 2:
          break
      self.cleint_.ecu_reset(ECUReset.ResetType.hardReset)
    except Exception as e:
      logging.getLogger('flashdiag').warning('got exception while download firmware : ' + str(e))
      raise
      # logging.getLogger('flashdiag').warning('callstack : ' + e.with_traceback())
      result = False
      reson = str(e)
    finally:
      self.cleint_.change_session(DiagnosticSessionControl.Session.defaultSession)
    return result, reson

  @staticmethod
  def CreateClient( ecu : str, proto : str):
    """
    This static method creates a UDS client based on the provided ECU identifier and security
    requirement.
    
    :param ecu: The `ecu` parameter in the `CreateClient` method is a string that represents the
    Electronic Control Unit (ECU) identifier. This identifier is used to look up the corresponding
    transmission and reception IDs in the `ecu_list` dictionary to create a new `UDSClient` object
    :type ecu: str
    :param need_security: The `need_security` parameter in the `CreateClient` method is a boolean value
    that indicates whether the client being created requires security measures or not. If
    `need_security` is `True`, it means that the client needs security, and if it is `False`, it means
    that the client does
    :type need_security: bool
    :return: The method `CreateClient` returns an instance of the `UDSClient` class with the specified
    transmission ID, reception ID, and security requirement based on the input parameters `ecu` and
    `need_security`. If the `ecu` is found in the `ecu_list` dictionary, it retrieves the corresponding
    transmission and reception IDs from the dictionary. If the `ecu` is not
    """
    try:
      if ecu in ecu_list.keys():
        return UDSClient(ecu_list[ecu].get("txid"), ecu_list[ecu].get("rxid"), proto)
      else:
        return UDSClient(0x77F, 0x7FF, proto)
    except Exception as e:
      logging.getLogger('flashdiag').warning('got exception while create client : ' + str(e))
      return None
    
ecu_list = {
  "VCU"      : {"txid" : 0x700, "rxid" : 0x780}, 
  "BMS"      : {"txid" : 0x701, "rxid" : 0x781}, 
  "HVDC"     : {"txid" : 0x702, "rxid" : 0x782}, 
  "TMS"      : {"txid" : 0x703, "rxid" : 0x783}, 
  "MCU"      : {"txid" : 0x1101, "rxid" : 0x1001}, 
  "SOC1"     : {"txid" : 0x1102, "rxid" : 0x1002},
  "SOC2"     : {"txid" : 0x1103, "rxid" : 0x1003},
  "SOC3"     : {"txid" : 0x1104, "rxid" : 0x1004},
}
