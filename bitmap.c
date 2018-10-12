
/*
 * HINT-STUB:
 * This is a required function, which you must write most of.
 *
 * It receives a request to free up a chunk of the specified size
 * (units is log2(num_blocks_to_free)) starting at the specified disk
 * block number, and puts it back into the binary buddy bitmap.
 * Note it must handle mapping disk block# to bitmap level and position,
 * and then also handle coalescing.
 *
 * IMPORTANT: be very consistent in the units you are passing around: is it
 * bits, bytes, words, or blocks??
 */
void fs421_free_block(struct inode *inode, unsigned long block, unsigned size_log2)
{
	struct super_block *sb = inode->i_sb;
	struct fs421_sb_info *sbi = fs421_sb(sb);
	struct buffer_head *bh;
	__u32 zchunk, bit, bbit; 
	int level;
	
	// added these variables
	__u32 sum = 0;
	__u32 *offset = sbi->s_zmap_offset;
	printk("freeblock(), function is called.\n");
	
	level = size_log2;
	
	printk("freeblock(), level=size_log2 = %d\n", level);
	printk("freeblock(), sbi->s_nzones = %lu\n", sbi->s_nzones);
	printk("freeblock(), block = %lu\n", block);
	
	zchunk = block - sbi->s_firstdatazone;
	
	//CHECK THAT BLOCK IS IN LEGAL RANGE FOR DISK;
	if ((block >= sbi->s_firstdatazone) && (block - sbi->s_firstdatazone <= sbi->s_nzones-1)) {
	
		//CHECK THAT BLOCK IS PROPERLY ALIGNED FOR ITS BINARY SIZE;
		if (!(zchunk % (1 << level))) {  // use block or zchunk here?
			
			printk("freeblock(), block is properly aligned.\n");
			spin_lock(&bitmap_lock);  /* lEAVE THIS ALONE! */
			
			printk("freeblock(), offset[level] = %lu\n", offset[level]);
			//CHECK THAT BIT ISN NOT ALREADY FREE AT LEVEL;
			sum += count_free(sbi->s_zmap, sb->s_blocksize,
				  offset[level], offset[level + 1] - offset[level])
			<< level;
			
			printk("freeblock(), count_free()=sum= %lu\n", sum);
			
			if (!sum) {	
				
				get_bh_and_bit(sb, level, zchunk, &bh, &bit);
					
				//FREE UP BIT AT LEVEL;
				clear_bit_le(bit, bh->b_data);
				
				//IMPORTANT--EVERY TIME YOU CHANGE A BIT, MUST USE FOLLOWING
				//	ON CONTAINING BUFFER''S buffer_head:
				//	mark_buffer_dirty(bh);
				mark_buffer_dirty(bh);
				
				bbit = (bit % 2) * -2 + 1;
				
				//CHECK BUDDY FOR COALESCING;
				//COALESCE IF NECESSARY;
				while (!test_bit_le(bbit, bh->b_data)) {
				
					printk("freeblock(), coalescing...\n");
				
					set_bit_le(bit, bh->b_data);
					set_bit_le(bbit, bh->b_data);
					mark_buffer_dirty(bh);
					
					level++;
					
					get_bh_and_bit(sb, level, zchunk, &bh, &bit);
					
					clear_bit_le(bit, bh->b_data);
					mark_buffer_dirty(bh);
					
					bbit = (bit % 2) * -2 + 1;
				}
				//THIS MIGHT CAUSE CASCADE AT UPPER LEVELS;
				
			}
			spin_unlock(&bitmap_lock);  /* lEAVE THIS ALONE! */
		}
		else
			printk("ERROR:  freeblock(), block not properly aligned!\n");
	}
	else
		printk("ERROR:  freeblock(), block not in legal range of disk!\n");
}

/*
 * HINT-STUB:
 * This is a required function, which you must write most of.
 *
 * It implements the allocation side of the binary buddy allocator.
 * It receives a request for a chunk of a specified size
 * (units is log2(num_blocks_to_free)), and tries to allocate it from
 * the binary buddy bitmap.
 * Note it must handle mapping bitmap level and position to  disk block#.
 *
 * Return: disk-relative block# of first disk block of contiguous chunk
 *   of size 2**size_log2,
 *   0 on error (e.g., no more space big enough)
 *  *** MAKE SURE TO ADD firstdatazone to zchunk to make chunk relative to disk ***
 *
 * IMPORTANT: be very consistent in the units you are working with at any
 * given time: is it bits, bytes, words, or blocks??
 */
int fs421_new_block(struct inode * inode, unsigned size_log2)
{
	printk("newblock(), function is called.\n");
	
	struct super_block *sb = inode->i_sb;
	struct fs421_sb_info *sbi = fs421_sb(sb);
	struct buffer_head *bh;
	int level;
	__u32 bit, zchunk;
	
	// added these variables
	__u32 start;
	unsigned start_in_blk;
	unsigned bits_to_test;
	int total_bits_scanned = 0;
	__u32 block;
	__u32 bits_per_zone = 8 * sb->s_blocksize;
	__u32 bits_left;
	__u32 *p;
	int bit_found = 0;
	int dwords;
	

	spin_lock(&bitmap_lock);

	/* HINT-STUB: depending on units of "size" param, convert
	 * it to log2 units--that's what "size_log2" below is.
	 */
	//CALCULATE LEVEL BASED ON SIZE: STORE IN size_log2;
	level = size_log2;
	
	zchunk = 0;  // returned as 0 as error if no space found
	
	do {
		printk("newblock(), scanning level = %d.\n", level);
	
		start = sbi->s_zmap_offset[level];
		bits_left = sbi->s_zmap_offset[level+1] - sbi->s_zmap_offset[level];
		
		printk("newblock(), bits_left = %u\n", bits_left);
		
		block = start / bits_per_zone;
		start_in_blk = start % bits_per_zone;
		
		//SCAN REQUESETD LEVEL;
		do {
			bits_to_test = bits_per_zone - start_in_blk;
			
			if (bits_to_test > bits_left)
				bits_to_test = bits_left;
			
			printk("newblock(), bits_to_test = %u\n", bits_to_test);
			
			bh = sbi->s_zmap[block]; 
			p = (__u32 *)bh->b_data + start_in_blk / 32;
			
			dwords = bits_to_test / 32;
			while (dwords--) {
				bit = find_first_zero_bit_le(p, 32);
				printk("newblock(), find_first_zero_bit_le() = bit = %u\n", bit);
				
				if (bit == 32) {
					bit_found = 0;
					*p++;
					total_bits_scanned += 32;
				}
				else {
					bit_found = 1;
					break;
				}
			}
			
			if (bit_found) {
				zchunk = ((total_bits_scanned + bit) << level) + sbi->s_firstdatazone;
				bits_left = 0;
				bits_to_test = 0;
			}
			
			//total_bits_scanned += bits_to_test;
			
			block++;
			bits_left -= bits_to_test;
			start_in_blk = 0;	/* for all subsequent blocks */
			
		} while (bits_left);

		//IF NECESSARY: SCAN HIGHER LEVELS {
		//    IF FOUND AT HIGHER LEVEL, SPLIT INTO PIECES
		//	BACK DOWN TO REQUESTED LEVEL;
		//}
		if (bit_found) {
			printk("newblock(), 0 bit found at level = %d.\n", level);
			set_bit_le(bit, p);
			mark_buffer_dirty(bh);
				
			// SPLITTING IS NOT WORKING
			while (level > size_log2) {
				printk("newblock(), splitting bits...\n");
				level--;

				get_bh_and_bit(sb, level, zchunk, &bh, &bit);
				clear_bit_le(bit + (bit ^ 1), bh->b_data);
				mark_buffer_dirty(bh);
			}	
		}
		else {
			level++;
			total_bits_scanned = 0;
		}
	
	} while (!bit_found && level < sbi->s_zmap_nlevels);


	
	//MARK BIT AS USED (BIT SET);

	//IMPORTANT--EVERY TIME YOU CHANGE A BIT, MUST USE FOLLOWING
	//    ON CONTAINING BUFFER''S buffer_head:
	//    mark_buffer_dirty(bh);


	spin_unlock(&bitmap_lock);

	//RETURN DISK-RELATIVE OFFSET OF CHUNK, OR 0 FOR ERROR;
	//if (!zchunk)
		//return zchunk;  //error
	//else
	//	return zchunk;
	printk("newblock(), return zchunk=%u\n", zchunk);
	return zchunk;
}

