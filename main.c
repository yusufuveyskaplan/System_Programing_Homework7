#include "mymalloc.h"
#include <stdio.h>

int main() {
    void *ptr1 = mymalloc(16);
    if (ptr1 == NULL) {
        printf("mymalloc hata!\n");
        return 1;
    }
    printf("Heap durumu (ilk durum):\n");
    printheap();

    void *ptr2 = mymalloc(32);
    printf("\nHeap durumu (ikinci durum):\n");
    printheap();

    myfree(ptr1);
    printf("\nHeap durumu (ptr1 free sonrası):\n");
    printheap();

    return 0;
}
