#define _GNU_SOURCE
#include "mymalloc.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>



/* Global değişkenlerin tanımlamaları */
Strategy strategy = BEST_FIT;
ListType listtype = ADDR_ORDERED_LIST;
Block *free_list = NULL;
Block *heap_start = NULL;
Block *heap_end = NULL;
Block *last_freed = NULL;

/* Free list'e ekleme: Blok adres sırasına göre eklenir */
static void add_to_free_list(Block *block) {
    block->info.isfree = 1;
    block->next = block->prev = NULL;
    if (free_list == NULL) {
        free_list = block;
        return;
    }
    Block *curr = free_list;
    Block *prev = NULL;
    while (curr && curr < block) {
        prev = curr;
        curr = curr->next;
    }
    block->next = curr;
    block->prev = prev;
    if (curr)
        curr->prev = block;
    if (prev)
        prev->next = block;
    else
        free_list = block;
}

/* Free list'ten çıkarma */
static void remove_from_free_list(Block *block) {
    if (block->prev)
        block->prev->next = block->next;
    else
        free_list = block->next;
    if (block->next)
        block->next->prev = block->prev;
    block->next = block->prev = NULL;
}

/* Heap genişletme: sbrk() ile HEAP_SIZE bayt ekler ve yeni alanı free list'e ekler */
static Block* extend_heap() {
    void *ptr = sbrk(HEAP_SIZE);
    if (ptr == (void*)-1)
        return NULL;

    Block *new_block = (Block*)ptr;
    new_block->info.size = HEAP_SIZE / 16; /* Toplam 16 baytlık blok sayısı */
    new_block->info.isfree = 1;
    new_block->next = new_block->prev = NULL;

    if (heap_start == NULL)
        heap_start = new_block;
    heap_end = (Block*)((char*)ptr + HEAP_SIZE);
    add_to_free_list(new_block);
    return new_block;
}

/* Bölme işlemi: Eğer blok gereğinden büyükse istenilen boyuta böl, kalan kısmı free list'e ekle */
Block *split_block(Block *b, size_t required) {
    uint64_t original_size = b->info.size;
    /* Kalan alanın minimum anlamlı bir free blok oluşturacak kadar (örneğin 2 blok) olması sağlanır */
    if (original_size <= required + 2)
        return b;

    b->info.size = required;
    Block *new_free = (Block*)((char*)b + required * 16);
    new_free->info.size = original_size - required;
    new_free->info.isfree = 1;
    new_free->next = new_free->prev = NULL;
    add_to_free_list(new_free);
    return b;
}

/* Sol birleşme: Eğer sol komşu boşsa, blokları birleştirir */
Block *left_coalesce(Block *b) {
    if (b == heap_start)
        return b;
    Block *prev = NULL;
    Block *cur = heap_start;
    while (cur && cur < b) {
        prev = cur;
        cur = (Block*)((char*)cur + cur->info.size * 16);
    }
    if (prev && prev->info.isfree) {
        remove_from_free_list(prev);
        remove_from_free_list(b);
        prev->info.size += b->info.size;
        add_to_free_list(prev);
        return prev;
    }
    return b;
}

/* Sağ birleşme: Eğer sağ komşu boşsa, blokları birleştirir */
Block *right_coalesce(Block *b) {
    Block *next = (Block*)((char*)b + b->info.size * 16);
    if ((char*)next >= (char*)heap_end)
        return b;
    if (next->info.isfree) {
        remove_from_free_list(b);
        remove_from_free_list(next);
        b->info.size += next->info.size;
        add_to_free_list(b);
    }
    return b;
}

/* mymalloc: İstenen boyutta bellek tahsisi yapar.
   - İlk çağrıda veya uygun blok yoksa sbrk ile HEAP_SIZE bayt ekler.
   - İstek 16 bayt hizalamasıyla yuvarlanır.
   - Uygun blok seçilirse (stratejiye göre) free list'ten çıkarılır ve gerekirse bölünür.
   - Kullanıcıya veri bölümünün başlangıç adresi döndürülür. */
void *mymalloc(size_t size) {
    if (size == 0)
        return NULL;
    uint64_t payload_blocks = numberof16blocks(size);
    /* Header için 2 blok ekleniyor (Block yapısı 32 byte) */
    uint64_t required = payload_blocks + 2;

    Block *chosen = NULL;
    Block *iter = free_list;

    if (strategy == BEST_FIT) {
        uint64_t best_diff = ~0ULL;
        while (iter) {
            if (iter->info.size >= required) {
                uint64_t diff = iter->info.size - required;
                if (diff < best_diff) {
                    best_diff = diff;
                    chosen = iter;
                }
            }
            iter = iter->next;
        }
    } else if (strategy == FIRST_FIT) {
        while (iter) {
            if (iter->info.size >= required) {
                chosen = iter;
                break;
            }
            iter = iter->next;
        }
    } else if (strategy == WORST_FIT) {
        uint64_t worst_size = 0;
        while (iter) {
            if (iter->info.size >= required && iter->info.size > worst_size) {
                worst_size = iter->info.size;
                chosen = iter;
            }
            iter = iter->next;
        }
    } else if (strategy == NEXT_FIT) {
        Block *start = last_freed ? last_freed->next : free_list;
        iter = start;
        while (iter) {
            if (iter->info.size >= required) {
                chosen = iter;
                break;
            }
            iter = iter->next;
        }
        if (!chosen) {
            iter = free_list;
            while (iter && iter != start) {
                if (iter->info.size >= required) {
                    chosen = iter;
                    break;
                }
                iter = iter->next;
            }
        }
    }
    if (!chosen) {
        if (!extend_heap())
            return NULL;
        return mymalloc(size);
    }
    remove_from_free_list(chosen);
    if (chosen->info.size >= required + 3)
        chosen = split_block(chosen, required);
    chosen->info.isfree = 0;
    return (void*)chosen->data;
}

/* myfree: Verilen adresdeki bloğu serbest bırakır.
   - Eğer p NULL ise hiçbir işlem yapmaz.
   - Blok free list'e eklenir ve sol-sağ birleşme (coalescing) denenir. */
void myfree(void *p) {
    if (p == NULL)
        return;
    Block *b = (Block*)((char*)p - offsetof(Block, data));
    b->info.isfree = 1;
    add_to_free_list(b);
    b = left_coalesce(b);
    b = right_coalesce(b);
    last_freed = b;
}

/* Heap'teki blokların durumunu yazdırır. Her blok için boşluk durumu ve bayt cinsinden boyutu gösterilir. */
void printheap() {
    Block *current = heap_start;
    while (current && (char*)current < (char*)heap_end) {
        printf("Free: %d\n", current->info.isfree);
        printf("Size: %lu\n", current->info.size * 16);
        printf("---------------\n");
        current = (Block*)((char*)current + current->info.size * 16);
    }
}

/* Free list'te ve adres sırasıyla gezinmek için yardımcı fonksiyonlar */
Block *next_block_in_freelist(Block *b) {
    return b ? b->next : NULL;
}

Block *prev_block_in_freelist(Block *b) {
    return b ? b->prev : NULL;
}

Block *next_block_in_addr(Block *b) {
    return (Block*)((char*)b + b->info.size * 16);
}

Block *prev_block_in_addr(Block *b) {
    if (b == heap_start)
        return NULL;
    Block *prev = NULL;
    Block *cur = heap_start;
    while (cur && cur < b) {
        prev = cur;
        cur = (Block*)((char*)cur + cur->info.size * 16);
    }
    return prev;
}

int setlisttype(ListType lt) {
    listtype = lt;
    return 0;
}

int setstrategy(Strategy s) {
    strategy = s;
    return 0;
}

Strategy getstrategy() {
    return strategy;
}

ListType getlisttype() {
    return listtype;
}

uint64_t numberof16blocks(size_t size_inbytes) {
    return (size_inbytes + 15) / 16;
}
