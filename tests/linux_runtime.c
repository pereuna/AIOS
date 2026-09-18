/* Verify every row is calculated once, including uneven and empty partitions,
 * across repeated pool start/stop and thread-count changes. */
#include <assert.h>
#include <stdio.h>
#include "../baremetal/runtime.h"

static void fill(void *arg, int first, int last) {
    int *rows=arg;
    for (int i=first;i<last;i++) rows[i]++;
}
int main(void) {
    for (unsigned threads=1;threads<=4;threads++) {
        bm_parallel_set_limit(threads);
        for (int repeat=0;repeat<3;repeat++) {
            assert(bm_parallel_begin()==threads);
            for (int n=0;n<259;n++) {
                int rows[259]={0};
                for (int j=0;j<7;j++) bm_parallel_rows(fill,rows,n);
                for (int i=0;i<259;i++) assert(rows[i]==(i<n ? 7 : 0));
            }
            bm_parallel_end();
        }
    }
    int row=0;
    bm_parallel_rows(fill,&row,1);
    assert(row==1);
    puts("Linux row scheduler: PASS");
}
