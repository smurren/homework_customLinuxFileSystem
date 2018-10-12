
/*
 * block_to_path():
 *   Maps a file-relative block number to a inode chunk level,
 *   and the offset within that chunk.
 *   Kept name of original, since what the new function does is somewhat
 *   similar to the old function, albeit simpler.
 *
 * Inputs:
 *   inode -- inode to operate on; just used to get sb info.
 *   block -- the file-relative block number to compute the path for
 *
 * Outputs:
 *   offset -- block offset relative to chunk at returned level
 * Return: level of chunk ptr, -1 on error
 */
static int block_to_path(struct inode * inode, long block, int *offset)
{
	printk("itree: blocktopath(), function is called.\n");
	
	char b[BDEVNAME_SIZE];
	struct super_block *sb = inode->i_sb;
	int level;

	if (block < 0) {
		printk("FS421-fs: block_to_path: block %ld < 0 on dev %s\n",
			block, bdevname(sb->s_bdev, b));
		return -1;
	} else if ((u64)block * (u64)sb->s_blocksize >=
			fs421_sb(sb)->s_max_size) {
		if (printk_ratelimit())
			printk("FS421-fs: block_to_path: "
			       "block %ld too big on dev %s\n",
				block, bdevname(sb->s_bdev, b));
		return -1;
	}

	/*
	 * HINT-STUB:
	 * NEED TO ADD CODE HERE TO COMPUTE level AND offset, BASED
	 * ON PARAM block
	 * E.g.s:
	 *   if request is for block# 0, return level=0, offset=0
	 *   if request is for block# 1, return level=1, offset=0
	 *   if request is for block# 2, return level=1, offset=1
	 *   if request is for block# 3, return level=2, offset=0
	 *    ... and so forth.
	 *
	 * THIS SHOULD BE ABOUT 4-5 LINES OF CODE
	 */
	
	level = 0;
	while ((1 << level) - 1 <= block)
		level += 1; 
	level -= 1;
	/* Return by reference */
	*offset = block - ((1 << level) - 1);
	return level;
}

/*
 * fs421_get_block():
 *   Maps a file block offset to a disk zone# and fetches it. Works w/chunks.
 *   Substantially simpler because we don't use indirect blocks.
 * Inputs:
 *   inode -- inode to fetch a data block for
 *   block -- block offset in file to fetch
 *   create -- flag to indicate we should grow file if request is beyond EOF
 * Outputs:
 *   bh -- buffer_head that is configured to point to data block in buffer
 *     cache.  *** CONTAINS DISK BLOCK NUMBER, RELATIVE??? ***
 *            *** say block 7 is asked for, could be 52 - startblock = block 45, for example***
 * Return:
 *   0 if all ok; else -errno
 */
// CALLED WHEN USER IS READING OR WRITING A BLOCK
//   if you write beyond end of the file, the file grows from end of file to new pointer location

int fs421_get_block(struct inode * inode, sector_t block,
		       struct buffer_head *bh, int create)
{
	printk("itree: getblock(), function is called.\n");
	
	int err = 0;
	int offset;
	int l;
	int new_block = 0;
	block_t *idata = i_data(inode);
	int level = block_to_path(inode, block, &offset);

	if (level < 0)
		return -EIO;
	else if (idata[level] == 0) {
		if (!create)
			return 0;
		/*
		printk("FS421-fs: get_block(create==1): l=%d, off=%d\n",
		       level, offset);
		*/
		/* set flag to indicate we had to allocate a new block: */
		new_block = 1;
		printk("itree: getblock(), new_block = 1.\n");
		/*
		 * HINT-STUB:
		 * NEED TO ADD CODE HERE TO EXTEND FILE IF NECESSARY:
		 * You need to:
		 * 1) See what level you have chunks allocated up to;
		 *    (look for first zero idata[] entry--cannot use inode's
		 *    size field, as it was already modified).
		 * 2) Allocate chunks for all empty levels up to requested
		 *    level (in "level", computed above)
		 * 3) For each chunk, put starting block address into
		 *    appropriate idata[] slot
		 * 4) IMPORTANT: keep track of what you are allocating:
		 *    on failure, MUST FREE UP WHAT YOU ADDED HERE SO FAR
		 *    DURING THIS CALL (but not what was already there before)
		 *
		 * You will need to call:
		 *   fs421_new_block(inode, size)
		 * and possibly:
		 *   fs421_free_block(inode, blocknum, size)
		 *
		 * (you will be writing these, too, in bitmap.c)
		 *
		 * Don't forget to set "err" if there was a problem
		 *
		 * THIS SHOULD BE ABOUT 8-10 LINES OF CODE
		 */
		
		// 1)
		l = 0;
		while (idata[l] && l <= DIRECT)  // l == DIRECT will make while condition false below
			l++;
		int i = l;
		
		while (i <= level) {
			// 2) and 3)
			idata[i] = fs421_new_block(inode, i);
			if (idata[i] == 0) {  // error was returned
				printk("itree: ERROR: getblock(), idata[i] = 0 after calling newblock()\n");
				err = -ENOSPC;  // set to a negative errno (-ENOSPC makes most sense)
				// 4)
				int j = i - 1;
				while (j >= l) {
					printk("itree: getblock(), calling freeblock()\n");
					fs421_free_block(inode, idata[j], j);
					j--;
				}
				break;
			}
			i++;
		}
		
	}

	if (!err) {
		/* STUB: really should zero out all of the blocks in all
		 * the chunks we allocated in this call */
		if (new_block) {
			set_buffer_new(bh);
		}
		map_bh(bh, inode->i_sb, idata[level] + offset);
	}
	return err;
}
