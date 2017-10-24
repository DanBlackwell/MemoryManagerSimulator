#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "manager.h"

int op_count;

// Instantiate a manager_t
manager_t* new_memory_manager(uint32_t page_num, uint32_t frame_num, uint32_t frame_size, uint32_t lru_parameter) {
    manager_t* self = malloc(sizeof(manager_t));
    self->_page_num = page_num;
    self->_frame_num = frame_num;
    self->_frame_size = frame_size;
    self->_lru_parameter = lru_parameter;
    self->_page_list = (page*)malloc(sizeof(page) * page_num);
    int i, j;
    for (i = 0; i < page_num; i++) {
        (self->_page_list + i)->_current_frame = -1;
        (self->_page_list + i)->_access_list = (int*)malloc(sizeof(int) * (lru_parameter + 2)); // 1st elem contains lru_paramenter, 2nd contains pos of front of queue
        *(self->_page_list + i)->_access_list = lru_parameter; //set size of list as lru_param
        *((self->_page_list + i)->_access_list + 1) = 2; //set FRONT to 2
        for (j = 2; j < lru_parameter + 2; j++) {
            *((self->_page_list + i)->_access_list + j) = -1;
        }
    }
   // printf("\n\n");

    self->_frame_list = (int*)malloc(sizeof(int) * frame_num);
    for (i = 0; i < frame_num; i++) {
        *(self->_frame_list + i) = -1;
    }
    // TODO: initiate other members you add
    return self;
}

// Free manager_t
void deconstruct_manager(manager_t* self) {
    int i;
    for (i = 0; i < self->_page_num; i++) {
        //printf("freeing %i of %i, val: %i\n", i, self->_page_num, *((self->_page_list + i)->_access_list + 2));
        //free((self->_page_list + i)->_access_list); //This causes a weird error if enabled :(
    }
    free(self->_page_list);
    free(self);
    op_count = 0;
    // TODO: free other members you add if in need
}

uint32_t addr_to_page(uint32_t addr, uint32_t frame_size) {
    return addr / frame_size;
}

uint32_t translate_address(uint32_t virtual_add, uint32_t frame_num, uint32_t frame_size) {
    //printf("translating %i (%i) onto frame %i = %i\n", virtual_add, virtual_add % frame_size, frame_num, (frame_num * frame_size) + (virtual_add % frame_size));
    return (frame_num * frame_size) + (virtual_add % frame_size);
}

uint32_t find_empty_frame(int* _frame_list, uint32_t frame_num) {
    int i;
    for (i = 0; i < frame_num; i++) {
        if (*(_frame_list + i) == -1) {
            return (uint32_t)i;
        }
    }
    return -1;
}

void set_access_list(page* page_var) {
    int queue_size = *page_var->_access_list;
    int front = *(page_var->_access_list + 1); 
    int i, inserted = 0;
    
    for (i = 0; i < queue_size; i++) {
        if (*(page_var->_access_list + i + 2) == -1) {
            *(page_var->_access_list + i + 2) = op_count;
            inserted = 1;
            break;
        }
    }

    if (!inserted) { //the list is already full
        *(page_var->_access_list + front) = op_count; //insert most recent in place of front
        if (queue_size > 1) {
             *(page_var->_access_list + 1) = ((front - 2 + 1) % queue_size) + 2; //update front, wrap around if needed 
        }
    }
}

void set_frame_addr_and_access_list(page* page_var, uint32_t new_frame_addr) {
    page_var->_current_frame = new_frame_addr;
    set_access_list(page_var);
}

int find_age(page* page_var) {
    int list_size = *page_var->_access_list;
    int front = *(page_var->_access_list + 1);
    int tail_pos = front - 1;
    if (tail_pos == 1) 
        tail_pos = front + list_size - 1;
    int age;
    if (*((page_var->_access_list) + tail_pos) == -1) {
        int i = 2; 
        while(1) { //loop through list, return latest non empty elem
            if (*(page_var->_access_list + i) == -1) {
                age = op_count - *(page_var->_access_list + i - 1);
                break;
            }
            i++;
        }
    } else { 
        age = op_count - *(page_var->_access_list + front);
    }

    return age;
}

uint32_t find_victim_page(int* frame_list, uint32_t frame_num, page* page_list) {
    int i;
    uint32_t victim = 0; 
    int victim_age = -1; 
    uint32_t candidate_page_addr; 
    int candidate_page_age;
    for (i = 0; i < frame_num; i++) {
        candidate_page_addr = *(frame_list + i);
        candidate_page_age = find_age(page_list + candidate_page_addr);
        if (candidate_page_age > victim_age) {
            victim = candidate_page_addr;
            victim_age = candidate_page_age;
        }
    }
    return victim;
} 

void swap_in(uint32_t page_num, int* frame_list, uint32_t frame_num, page* page_list) {
    uint32_t new_frame_addr = find_empty_frame(frame_list, frame_num);
    if (new_frame_addr != -1) {
        *(frame_list + new_frame_addr) = page_num;
        set_frame_addr_and_access_list((page_list + page_num), new_frame_addr);
    } else {
        uint32_t victim_page = find_victim_page(frame_list, frame_num, page_list);
        new_frame_addr = (uint32_t)(page_list + victim_page)->_current_frame;
        (page_list + victim_page)->_current_frame = -1;
        *(frame_list + new_frame_addr) = page_num;
        set_frame_addr_and_access_list((page_list + page_num), new_frame_addr);
    }
}

// TODO: return the physical address of the logical address 
uint32_t access(manager_t* self,  uint32_t addr) {
    op_count++;
    uint32_t page_num = addr_to_page(addr, self->_frame_size);

    if ((self->_page_list + page_num)->_current_frame != -1) {
        set_access_list((self->_page_list) + page_num);
        return translate_address(addr, (self->_page_list + page_num)->_current_frame, self->_frame_size);
    } else {
        swap_in(page_num, self->_frame_list, self->_frame_num, self->_page_list);
        return translate_address(addr, (self->_page_list + page_num)->_current_frame, self->_frame_size);
    }

    return 0; 
}
