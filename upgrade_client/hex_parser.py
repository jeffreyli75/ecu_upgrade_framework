import intelhex
import crc

# The `HexParser` class parses Intel Hex files, extracts data and checksums for each segment, and
# provides methods to access this information.
class HexParser:
  def __init__(self, filepath : str):
    self.__intel_hex__ = intelhex.IntelHex()
    self.__intel_hex__.fromfile(filepath, format='hex')
    # self.__seg_count__ = len(self.__intel_hex__.segments())
    self.__datas__ = {}
    self.__checksums__ = {}
    for segment in self.__intel_hex__.segments():
      start_addr, end_addr = segment
      size : int = end_addr - start_addr
      self.__datas__[start_addr] = self.__intel_hex__.tobinarray(start=start_addr, size=size).tobytes()
      self.__checksums__[start_addr] = self.CalcChecksum(self.__datas__[start_addr])

  def CalcChecksum(self, data):
    config = crc.Configuration(  width=32,
                                polynomial=0x04C11DB7,
                                init_value=0xFFFFFFFF,
                                final_xor_value=0,
                                reverse_input=False,
                                reverse_output=False)
    checksumer = crc.Calculator(config)
    return checksumer.checksum(data)

  def GetSegments(self):
    return self.__intel_hex__.segments()
  
  def GetData(self, start_addr):
    return self.__datas__[start_addr]
  
  def GetCheckSum(self, start_addr):
    return self.__checksums__[start_addr]
  
  def GetMinAddr(self):
    return self.__intel_hex__.minaddr()

  def GetMemSize(self):
    return self.__intel_hex__.maxaddr() - self.__intel_hex__.minaddr() + 1
