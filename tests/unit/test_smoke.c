#include "natural_time.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
  nt_natural_date nd = {0};
  if (nt_make_natural_date(1356091200000LL, 0.0, &nd) != NT_OK) return 1;
  if (nd.unix_time != 1356091200000LL) return 2;
  if (nd.longitude != 0.0) return 3;
  printf("smoke ok\n");
  return 0;
}


