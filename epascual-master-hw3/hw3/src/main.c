#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *x = sf_malloc(100);
    sf_realloc(x, 120);

    double fragments = sf_fragmentation();
    fragments += 0;

    sf_show_heap();

    return EXIT_SUCCESS;
}
