/*
 * Copyright (C) 2021 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


/* Number of memory pages */
#define MEMORY_PAGE_NUM         256
#define MEMORY_PAGE_SIZE        256
#define OFFSET                  8
#define MASK                    0xFF

uint16_t page_table[MEMORY_PAGE_NUM];

/* 256 pages of 256B or 64 pages of 4096B */
uint8_t main_mem[MEMORY_PAGE_NUM][MEMORY_PAGE_SIZE] = {0};


/**
 * @brief   Convert a virtual address to a page table entry. 
 * @param   vaddr virtual address
 * @param   pte page table entry   
 * 
 * @return  0 on success, -1 on error.
 */
int virt_to_pte(uint16_t vaddr, uint16_t *pte){

    *pte = page_table[vaddr >> OFFSET];
    return 0;
}

/**
 * @brief   Read a byte from memory. For step 3 also checks RWX and Valid bit.
 *          Uses virt to phys to get the physical address.
 * @param   vaddr virtual address
 * @param   byte value to read   
 * 
 * @return  0 on success, -1 on error. Page faults must use PAGE_FAULT macro to
 *          to print an error message
 */
int get_byte(uint16_t vaddr, uint8_t *byte) {

    uint16_t pte, page_num;
    int success = virt_to_pte(vaddr, &pte);
    if (success < 0) {
        return -1;
    }

    // get rid of the flags
    page_num = pte >> OFFSET;

	*byte = main_mem[page_num][vaddr & MASK];

    return 0;
}

/**
 * @brief   Write a byte to memory. For step 3 also checks RWX and Valid bit.
 *          Uses virt to pte to get the physical address.
 * @param   vaddr virtual address
 * @param   byte value to write   
 * 
 * @return  0 on success, -1 on error. Page faults must use PAGE_FAULT macro to
 *          to print an error message
 */
int store_byte(uint16_t vaddr, uint8_t byte) {

    uint16_t pte, page_num;
    int success = virt_to_pte(vaddr, &pte);
    if (success < 0) {
        return -1;
    }

    // get rid of the flags
    page_num = pte >> OFFSET;

    main_mem[page_num][vaddr & MASK] = byte;

    return 0;
 }

/**
 * @brief   Prints an entire page. Can be used for debug.
 * @param   vaddr: virtual address of the page
 * @param   width: number of bytes per line (1/2/3/../32)
 */
void print_page(uint16_t vaddr, uint8_t width)
{

    uint16_t i;
    uint8_t byte;

    if (width > 32) {
        width = 32;
    }

    for (i = 0; i < MEMORY_PAGE_SIZE; ++i) {
        if (i % width == 0 && i > 0) 
            printf("\n");

        if (get_byte(vaddr | i, &byte) < 0) {
            break;
        }
        printf("%02x ", byte);
    }
    printf("\n");
}

/**
 * @brief   This function is used to test the read and write methods.
 *          !!! DO NOT MODIFY !!!
 */
int test_mem()
{
    uint8_t byte;
    int vaddr;
    int error_count = 0;

    for (vaddr = 0; vaddr < MEMORY_PAGE_NUM * MEMORY_PAGE_SIZE; ++vaddr) {
        if (get_byte(vaddr, &byte) < 0) {
            printf("Error vaddr : 0x%04x\n");
            return -1;
        }

        if (byte != (uint8_t)(vaddr & 0xFF)) {
            error_count++;
        }
    }

    if (error_count > 0) {
        printf("Memory test failed. There are %d errors\n", error_count);
        return -1;
    }
    else {
        printf("=== Memory test successfull !!!===\n");
        return 0;
    }
}

/**
 * @brief   Initializes the page table. The virtual addresses are
 *          the inverse of the physical ones. Ex. physical 0x0000
 *          is equal to virtual 0xFFFF
 */
void init_page_table(void) {
    int i;
    uint8_t r_shift = 8;

    for (i = 0; i < MEMORY_PAGE_NUM; ++i) {
        page_table[i] = ((MEMORY_PAGE_NUM -1) - i) << r_shift;}
}

int main(int argc, char *argv[]) {    
    /*
        !!! DO NOT MODIFY the main function!!! 
    */

    init_page_table();
    int vaddr;

    /** Puts data into memory */
    for (vaddr = 0; vaddr < MEMORY_PAGE_NUM * MEMORY_PAGE_SIZE; ++vaddr) {
        store_byte(vaddr, (uint8_t)(vaddr & 0xFF));
    }
    test_mem();

    /* ADD your code here for debug purposes */

    // print_page(0x0000, 32);

    return 0;
}
