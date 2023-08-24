#include <stdio.h>

int array[8] = {1, 1, 0, 0, 0, 0, 0, 1};

static int numusearray (int *nums) {
  int lg;
  int ttlg;  /* 2^lg */
  int ause = 0;  /* summation of `nums' */
  int i = 1;  /* count to traverse all array keys */

  for (lg=0, ttlg=1; lg<=26; lg++, ttlg*=2) {  /* for each slice */
    int lc = 0;  /* counter */
    int lim = ttlg;
    if (lim > 8) {
      lim = 8;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {
      if (array[i-1])
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}


static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;  /* 2^i */
  int a = 0;  /* number of elements smaller than 2^i */
  int na = 0;  /* number of elements to go to array part */
  int n = 0;  /* optimal size for array part */
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a > twotoi/2) {  /* more than half elements present? */
        n = twotoi;  /* optimal size (till now) */
        na = a;  /* all elements smaller than n will go to array part */
      }
    }
    if (a == *narray) break;  /* all elements already counted */
  }
  *narray = n;
  //lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}

int main()
{
    int nums[27] = {0};
    int ret = numusearray(nums);
    printf("ret: %d\n", numusearray(nums));
    int na = computesizes(nums, &ret);
    printf("na: %d, ret: %d\n", na, ret);

    return 0;
}
