#ifndef MYMALLOC_H
#define MYMALLOC_H

#include <inttypes.h>
#include <stddef.h>

#define HEAP_SIZE 1024  /* sbrk ile heap genişletirken kullanılacak bayt sayısı */

/* Bellek tahsisi stratejileri */
typedef enum {
    BEST_FIT,
    NEXT_FIT,
    FIRST_FIT,
    WORST_FIT
} Strategy;

/* Free list organizasyonu */
typedef enum {
    ADDR_ORDERED_LIST,
    UNORDERED_LIST
} ListType;

/* Tag yapısı: 8 byte size, 4 byte isfree, 4 byte padding
   __attribute__((aligned(16))) ile 16 bayt hizalama sağlanır */
typedef struct tag {
    uint64_t size;    /* 16 baytlık blok sayısı */
    uint32_t isfree;  /* 1: boş, 0: tahsis edilmiş */
    uint32_t padding; /* hizalama için yedek alan */
} Tag __attribute__((aligned(16)));

/* Block yapısı: next ve prev pointer'lar, Tag ve esnek dizi (data[]) */
typedef struct block {
    struct block *next; /* Free list'teki sonraki blok */
    struct block *prev; /* Free list'teki önceki blok */
    Tag info;           /* Blok bilgileri: boyut ve boşluk durumu */
    char data[];        /* Kullanıcıya verilecek veri alanı */
} Block __attribute__((aligned(16)));

/* Global değişkenlerin bildirimleri */
extern Block *free_list;  /* Free list'in başlangıcı */
extern Block *heap_start; /* sbrk ile alınan belleğin başlangıcı */
extern Block *heap_end;   /* sbrk ile alınan belleğin sonu */
extern Block *last_freed; /* Son serbest bırakılan blok */

/* Fonksiyon prototipleri */
void *mymalloc(size_t size);
void myfree(void *p);

Block *split_block(Block *b, size_t size);
Block *left_coalesce(Block *b);
Block *right_coalesce(Block *b);
Block *next_block_in_freelist(Block *b);
Block *prev_block_in_freelist(Block *b);
Block *next_block_in_addr(Block *b);
Block *prev_block_in_addr(Block *b);

/* Verilen byte cinsinden boyutu, kaç tane 16 baytlık bloğa ihtiyaç olduğunu hesaplar */
uint64_t numberof16blocks(size_t size_inbytes);

/* Heap'in durumunu ekrana yazdırır */
void printheap();

ListType getlisttype();
int setlisttype(ListType listtype);
Strategy getstrategy();
int setstrategy(Strategy strategy);

#endif  /* MYMALLOC_H */
