#include "inode.h"
#include "super_block.h"
#include "file.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "memory.h"
#include "interrupt.h"
#include "fs.h"

struct inode_position {
	bool two_sec;
	uint32_t sec_lba;
	uint32_t off_size;
};

/*get inode_positino by inode_no*/
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
	ASSERT(inode_no < 4096);
	uint32_t inode_table_lba = part->sb->inode_table_lba;
	
	uint32_t inode_size = sizeof(struct inode);
	uint32_t off_size = inode_no * inode_size;
	uint32_t off_sec = off_size >> 9; // /512
	uint32_t off_size_in_sec = off_size & 0x1ff; // %512
	//pos.sector = off_sec
	//pos.offset_in_sector = off_size_in_sec
	
	uint32_t left_in_sec = 512 - off_size_in_sec;
	/*check if the inode is in two sectors*/
	if(left_in_sec < inode_size) {
		inode_pos->two_sec = true;
	} else {
		inode_pos->two_sec = false;
	}
	inode_pos->sec_lba = inode_table_lba + off_sec;
	inode_pos->off_size = off_size_in_sec;
}

/*write inode into part*/
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {
	uint8_t inode_no = inode->i_no;
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
	
	/*inode_tag and i_open_cnts are not needed in hd*/
	struct inode pure_inode;
	memcpy(&pure_inode, inode, sizeof(struct inode));

	pure_inode.i_open_cnts = 0;
	pure_inode.write_deny = false;
	pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

	uint8_t sectors_to_write = inode_pos.two_sec ? 2 : 1;
	char* inode_buf = (char*)io_buf;

	ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, sectors_to_write);
	memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
	ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, sectors_to_write);
}

struct inode* inode_open(struct partition* part, uint32_t inode_no) {
	struct list_elem* elem = part->open_inodes.head.next;
	struct inode* inode_found;
	while(elem != &part->open_inodes.tail) {
		inode_found = elem2entry(struct inode, inode_tag, elem);
		if(inode_found->i_no == inode_no) {
			++inode_found->i_open_cnts;
			return inode_found;
		}
		elem = elem->next;
	}

	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);

	inode_found = (struct inode*)sys_malloc_kernel(sizeof(struct inode));
	
	char* inode_buf;
	uint8_t sectors_to_read = inode_pos.two_sec ? 2 : 1;
	inode_buf = (char*)sys_malloc(1024);
	ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, sectors_to_read);
	
	memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

	list_push(&part->open_inodes, &inode_found->inode_tag);
	inode_found->i_open_cnts = 1;
	sys_free(inode_buf);
	return inode_found;
}

void inode_close(struct inode* inode) {
	enum intr_status old_status = intr_disable();
	if(--inode->i_open_cnts == 0) {
		list_remove(&inode->inode_tag);
		struct task_struct* cur = running_thread();
		uint32_t* cur_pagedir_bak = cur->pgdir;
		cur->pgdir = NULL;
		sys_free(inode);
		cur->pgdir = cur_pagedir_bak;
	}
	intr_set_status(old_status);
}

void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf) {
	ASSERT(inode_no < 4096);
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

	char* inode_buf = (char*)io_buf;
	uint8_t sectors_to_edit = inode_pos.two_sec ? 2 : 1;
		
	ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, sectors_to_edit);
	memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
	ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, sectors_to_edit);
}

void inode_release(struct partition* part, uint32_t inode_no) {
	struct inode* inode_to_del = inode_open(part, inode_no);
	ASSERT(inode_to_del->i_no == inode_no);

	uint8_t block_idx = 0;
	uint8_t block_cnt = 12;
	uint32_t block_bitmap_idx;
	uint32_t all_blocks[140] = {0};

	while(block_idx < 12) {
		all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
		++block_idx;
	}

	if(inode_to_del->i_sectors[12] != 0) {
		ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;

		block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
		ASSERT(block_bitmap_idx > 0);
		bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
		bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	}

	block_idx = 0;
	while(block_idx < block_cnt) {
		if(all_blocks[block_idx] != 0) {
			block_bitmap_idx = 0;
			block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			ASSERT(block_bitmap_idx > 0);
			bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
			bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
		}
		++block_idx;
	}

	bitmap_set(&part->inode_bitmap, inode_no, 0);
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);

	/**************** for debug *****************
	 * inode is distributed by inode_bitmap,    *
	 * so there is no need to clear inode_table */
	void* io_buf = sys_malloc(1024);
	inode_delete(part, inode_no, io_buf);
	sys_free(io_buf);
	/********************************************/

	inode_close(inode_to_del);
}

void inode_init(uint32_t inode_no, struct inode* new_inode) {
	new_inode->i_no = inode_no;
	new_inode->i_size = 0;
	new_inode->i_open_cnts = 0;
	new_inode->write_deny = false;

	uint8_t sec_idx = 0;
	while(sec_idx < 13) {
		new_inode->i_sectors[sec_idx] =0;
		++sec_idx;
	}
}
