#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
  {
    printf("\tim: error! invalid blockid %d\n", id);
    return;
  }

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
  {
    printf("\tim: error! invalid blockid %d\n", id);
    return;
  }
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  uint32_t block_id = 3 + 1 / IPB + BLOCK_NUM / BPB + INODE_NUM; //block bitmap
  while (using_blocks.find(block_id) != using_blocks.end() && using_blocks[block_id] == 1)
  {
    block_id++;
  }
  if (block_id < BLOCK_NUM)
  {
    using_blocks[block_id] = 1;
    return block_id;
  }
  else
  {

    printf("\tim: error! out of blocks\n");
    exit(0);
  }
}

void block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id] = 1;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void block_manager::read_block(uint32_t id, char *buf)
{

  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{

  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1)
  {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

  struct inode *ino_disk;
  ino_disk = (struct inode *)malloc(sizeof(struct inode));
  ino_disk->type = type;
  ino_disk->size = 0;

  struct timespec time1 = {0, 0};
  clock_gettime(CLOCK_REALTIME, &time1);

  ino_disk->ctime = ino_disk->mtime = ino_disk->atime =
      time1.tv_sec * 1000 + time1.tv_nsec / 1000000;

  static uint32_t inum;
  inum++;
  //write inode into block

  put_inode(inum, ino_disk);
  return inum;
}

void inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  struct inode *ino = get_inode(inum);
  ino->type = 0;

  put_inode(inum, ino);
  free(ino);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM)
  {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode *)buf + inum % IPB;
  if (ino_disk->type == 0)
  {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode *)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode *)buf + inum % IPB;
  *ino_disk = *ino;

  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */

  printf("read file %d\n", inum);
  struct inode *ino = get_inode(inum);
  *size = ino->size;
  int i;
  int nblocks = bm->get_nblocks(ino->size);
  *buf_out = (char *)malloc(nblocks * BLOCK_SIZE);
  for (i = 0; i < MIN(NDIRECT, nblocks); i++)
  {
    blockid_t id = ino->blocks[i];
    bm->read_block(id, *buf_out + i * BLOCK_SIZE);
  }
  if (i < nblocks)
  {

    blockid_t indirect_blocks[NINDIRECT];
    bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);

    for (; i < nblocks; ++i)
    {
      blockid_t id = indirect_blocks[i - NDIRECT];
      bm->read_block(id, *buf_out + i * BLOCK_SIZE);
    }
  }
  struct timespec time1 = {0, 0};
  ino->atime = clock_gettime(CLOCK_REALTIME, &time1);
  put_inode(inum, ino);
  free(ino);
  return;
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  printf("\tim: write_file %d, size %d\n", inum, size);

  if (inum >= INODE_NUM)
    return;

  int nblocks = bm->get_nblocks(size);

  if (nblocks > MAXFILE)
    return;

  struct inode *inode = get_inode(inum);

  int old_nblocks = bm->get_nblocks(inode->size);

  inode->size = size;

  blockid_t indirect_blocks[NINDIRECT];

  if (old_nblocks > NDIRECT)
  {
    bm->read_block(inode->blocks[NDIRECT], (char *)indirect_blocks);
  }
  if (nblocks > old_nblocks)
  {
    // grow
    int i = old_nblocks;
    if (old_nblocks < NDIRECT)
    {
      for (; i < MIN(NDIRECT, nblocks); ++i)
      {
        inode->blocks[i] = bm->alloc_block();
      }
    }
    if (i < nblocks)
    {

      // need to alloc indirect blocks
      if (old_nblocks <= NDIRECT)
      {
        inode->blocks[NDIRECT] = bm->alloc_block();
      }
      else
      {
        i = old_nblocks; // ASSERT old_nblocks > DIRECT
      }

      for (; i < nblocks; ++i)
      {
        indirect_blocks[i - NDIRECT] = bm->alloc_block();
      }

      bm->write_block(inode->blocks[NDIRECT], (const char *)indirect_blocks);
    }
  }
  else if (nblocks < old_nblocks)
  {
    // shrink
    int i;

    for (i = old_nblocks - 1; i >= MAX(NDIRECT, nblocks); --i)
    {
      blockid_t id = indirect_blocks[i - NDIRECT];
      bm->free_block(id);
    }

    if (i >= nblocks)
    {
      if (old_nblocks > NDIRECT)
        bm->free_block(inode->blocks[NDIRECT]);
      for (; i >= nblocks; --i)
      {
        blockid_t id = inode->blocks[i];
        bm->free_block(id);
      }
    }
  }

  // copy data
  int i = 0;
  char write_buf[BLOCK_SIZE];

  for (; i < MIN(nblocks, NDIRECT); ++i)
  {
    blockid_t id = inode->blocks[i];
    memset(write_buf, 0, BLOCK_SIZE);
    int len = MIN(BLOCK_SIZE, size - i * BLOCK_SIZE);
    memcpy(write_buf, buf + i * BLOCK_SIZE, len);
    bm->write_block(id, write_buf);
  }

  for (; i < nblocks; ++i)
  {
    blockid_t id = indirect_blocks[i - NDIRECT];
    memset(write_buf, 0, BLOCK_SIZE);
    int len = MIN(BLOCK_SIZE, size - i * BLOCK_SIZE);
    memcpy(write_buf, buf + i * BLOCK_SIZE, len);
    bm->write_block(id, write_buf);
  }

  struct timespec time1 = {0, 0};
  clock_gettime(CLOCK_REALTIME, &time1);

  inode->ctime = inode->mtime = inode->atime =
      time1.tv_sec * 1000 + time1.tv_nsec / 1000000;

  put_inode(inum, inode);

  free(inode);
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode *ino = get_inode(inum);

  if (!ino)
  {
    a.type = 0;
    return;
  }

  a.type = ino->type;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;
  a.size = ino->size;

  free(ino);
  return;
}

void inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  struct inode *ino = get_inode(inum);
  int i;
  for (i = 0; i < bm->get_nblocks(ino->size); i++)
  {
    bm->free_block(ino->blocks[i]);
  }
  if (i < bm->get_nblocks(ino->size))
  {

    blockid_t indirect_blocks[NINDIRECT];
    bm->read_block(ino->blocks[NDIRECT], (char *)indirect_blocks);
    for (int j = 0; j < bm->get_nblocks(ino->size) - i; j++)
    {
      bm->free_block(indirect_blocks[j]);
    }
  }
  free_inode(inum);
  free(ino);
  return;
}
