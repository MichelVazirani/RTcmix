/* An interpolating table lookup (one-shot oscillator) class, by John Gibson
   (This is like tablei in cmix genlib.)
*/

#if !defined(__TableL_h)
#define __TableL_h

#include "TableN.h"

class TableL : public TableN
{
  protected:  
  public:
    TableL(MY_FLOAT duration, MY_FLOAT *aTable, int tableSize);
    ~TableL();
    MY_FLOAT tick(long nsample, MY_FLOAT amp);
};

#endif

