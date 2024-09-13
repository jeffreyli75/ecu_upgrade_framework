#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

uint32_t seedToKey(uint32_t seed, uint32_t Learmask)
{
  uint32_t key = 0; 

  if (seed != 0)  {
    for (int i = 0; i < 32; i++)     {
      if (seed & 0x80000000)      {
        seed = (seed << 1) ^ Learmask;
      }  else   {
        seed = seed << 1;
      }
    }
    key = seed;
  }

  return key;
}

#ifdef __cplusplus
}
#endif