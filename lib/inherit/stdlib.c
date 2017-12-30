#pragma strict_types
#pragma save_types

static int _RandSeed1 = 0x5c2582a4;
static int _RandSeed2 = 0x64dff8ca;

int rand()
{
  _RandSeed1 = ((_RandSeed1 * 13 + 1) ^ (_RandSeed1 >> 9)) + _RandSeed2;
  _RandSeed2 = (_RandSeed2 * _RandSeed1 + 13) ^ (_RandSeed2 >> 13);
  return _RandSeed1;
}

void srand(int seed)
{
  _RandSeed1 = (seed - 1) ^ 0xAB569834;
  _RandSeed2 = (seed + 1) ^ 0x56F42001;
  rand();
  rand();
  rand();
}
