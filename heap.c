#include <err.h>

#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2
#define IPRANGE pow(2,32)

static uint32_t **heap;
static uint32_t *temp; //for swapping
static int size;
static uint32_t zero = 0;
static uint32_t *zeroptr = &zero;

void swap(uint32_t** x, uint32_t** y) {
  temp = *x;
  *x = *y;
  *y = temp;
}

void heapify(int i) {
    int smallest=i;
    if (LCHILD(i) < size)
      if (*heap[LCHILD(i)] < *heap[i])
        smallest = LCHILD(i);
    if (RCHILD(i) < size)
      if (*heap[RCHILD(i)] < *heap[smallest])
        smallest = RCHILD(i);

    if(smallest != i) {
        swap(&heap[i], &heap[smallest]);
        heapify(smallest);
    }
}

uint32_t** initHeap(uint32_t *hugebuf, int sz) {
    size = sz;
    // free, allocate and initialize memory
    free(heap);
    heap = calloc(size, sizeof(uint32_t*));

    int i;
    for (i = 1; i < size; i++)
      heap[i] = &zeroptr;
    // put first {size} elements in buffer to heap, than heapify
    for (i = 0; i < size; i++) {
      heap[i] = &hugebuf[i];
      //printf("hugebuf[%d]: %u, heap[%d]: %p, *heap[%d]: %u \n",i, hugebuf[i], i, heap[i], i, *heap[i]);
    }
    for (i = (size-1)/2; i>=0 ; i--) {
      heapify(i);
    }

    // for each element in buffer, from {size} to {IPRANGE}, check:
      // if it's greater than root, swap it
      // if it's equal to root, expand the heap, add a new node.
    // heapify.
    uint32_t j;
    for (j = size; j < IPRANGE; j++) {
      if (hugebuf[j] > **heap && uniqip(&hugebuf[j])) {
        *heap = &hugebuf[j]; // make it root
        heapify(0); // iterate first (size-1)/2 elements
      } //else if (hugebuf[i] == *heap[0]) {
      //     heap = realloc(heap, sizeof(uint32_t**) * size+1);
      //     heap[size] = &hugebuf[i]; // new element in heap
      //     heapify(size++);
      // }
    }
    return heap;
}

int uniqip(uint32_t* ip) {
  int i;
  uint32_t **index = heap;
  for (i = 0; i < size; i++, index++) {
      if (*index == ip)
        return 0;
  }
  return 1;
}
