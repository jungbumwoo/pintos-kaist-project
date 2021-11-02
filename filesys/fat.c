#include "filesys/fat.h"
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include <string.h>

#define CEILING(x, y) ((x) / (y) + ((x) % (y) != 0))
#define FLOOR(x, y) ((x) / (y))

/* Should be less than DISK_SECTOR_SIZE */
struct fat_boot {
	unsigned int magic;
	unsigned int sectors_per_cluster; /* Fixed to 1 */
	unsigned int total_sectors; // = disk_size (filesys_disk)
	unsigned int fat_start;
	unsigned int fat_sectors; /* Size of FAT in sectors. */
	unsigned int root_dir_cluster;
};

/* FAT FS */
struct fat_fs {
	struct fat_boot bs;
	unsigned int *fat;
	unsigned int fat_length;
	disk_sector_t data_start;
	cluster_t last_clst;
	struct lock write_lock;
};

static struct fat_fs *fat_fs;

void fat_boot_create (void);
void fat_fs_init (void);

// don't have to modify 
void
fat_init (void) {
	fat_fs = calloc (1, sizeof (struct fat_fs));
	if (fat_fs == NULL)
		PANIC ("FAT init failed");

	// Read boot sector from the disk
	unsigned int *bounce = malloc (DISK_SECTOR_SIZE);	
	if (bounce == NULL)
		PANIC ("FAT init failed");
	disk_read (filesys_disk, FAT_BOOT_SECTOR, bounce);
	memcpy (&fat_fs->bs, bounce, sizeof (fat_fs->bs));
	free (bounce);

	// Extract FAT info
	if (fat_fs->bs.magic != FAT_MAGIC)
		fat_boot_create ();
	fat_fs_init (); // need to write 
}

// don't have to modify 
void
fat_open (void) {
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT load failed");

	// Load FAT directly from the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_read = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_read (filesys_disk, fat_fs->bs.fat_start + i,
			           buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		} else {
			uint8_t *bounce = malloc (DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT load failed");
			disk_read (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			memcpy (buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free (bounce);
		}
	}
}

// don't have to modify 
void
fat_close (void) {
	// Write FAT boot sector
	uint8_t *bounce = calloc (1, DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT close failed");
	memcpy (bounce, &fat_fs->bs, sizeof (fat_fs->bs));
	disk_write (filesys_disk, FAT_BOOT_SECTOR, bounce);
	free (bounce);

	// Write FAT directly to the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_wrote = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_write (filesys_disk, fat_fs->bs.fat_start + i,
			            buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		} else {
			bounce = calloc (1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT close failed");
			memcpy (bounce, buffer + bytes_wrote, bytes_left);
			disk_write (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			bytes_wrote += bytes_left;
			free (bounce);
		}
	}
}

// don't have to modify 
void
fat_create (void) {
	// Create FAT boot
	fat_boot_create ();
	fat_fs_init ();

	// Create FAT table
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT creation failed");

	// Set up ROOT_DIR_CLST
	fat_put (ROOT_DIR_CLUSTER, EOChain);

	// Fill up ROOT_DIR_CLUSTER region with 0
	uint8_t *buf = calloc (1, DISK_SECTOR_SIZE);
	if (buf == NULL)
		PANIC ("FAT create failed due to OOM");
	disk_write (filesys_disk, cluster_to_sector (ROOT_DIR_CLUSTER), buf);
	free (buf);
}

// don't have to modify 
void
fat_boot_create (void) {
	unsigned int fat_sectors =
	    (disk_size (filesys_disk) - 1)
	    / (DISK_SECTOR_SIZE / sizeof (cluster_t) * SECTORS_PER_CLUSTER + 1) + 1;
	fat_fs->bs = (struct fat_boot){
	    .magic = FAT_MAGIC,
	    .sectors_per_cluster = SECTORS_PER_CLUSTER,
	    .total_sectors = disk_size (filesys_disk),
	    .fat_start = 1,
	    .fat_sectors = fat_sectors,
	    .root_dir_cluster = ROOT_DIR_CLUSTER,
	};
}

void
fat_fs_init (void) {
	/* TODO: Your code goes here!! */
	/*Initialize FAT file system. */ 
	/* You have to initialize fat_length and data_start field */
	/* fat_length stores how many clusters in the filesystem and data_start stores in which sector we can start to store files. 
	
	exploit some values stored in fat_fs->bs. 

	Also, you may want to initialize some other useful data in this function.*/
	// 1 for boot sector count.
	unsigned int data_sectors = fat_fs->bs.total_sectors - fat_fs->bs.fat_sectors - 1;
	// Forget last few sectors when final sectors are not enough to construct a cluster.
	fat_fs->fat_length = FLOOR(data_sectors, fat_fs->bs.sectors_per_cluster);
	fat_fs->data_start = fat_fs->bs.fat_start + fat_fs->bs.fat_sectors;
	fat_fs->last_clst = ROOT_DIR_CLUSTER + 1;
	lock_init (&fat_fs->write_lock);
}

/*----------------------------------------------------------------------------*/
/* FAT handling                                                               */
/*----------------------------------------------------------------------------*/

/* Add a cluster to the chain.
 * If CLST is 0, start a new chain.
 * Returns 0 if fails to allocate a new cluster. */
cluster_t
fat_create_chain (cluster_t clst) {
	/* TODO: Your code goes here. */
	/* 'clst' 뒤에다가 새로운 chain을 달아. 없으면 chain 새로 생성 */
	cluster_t testing = fat_fs->last_clst;
	while (fat_get (testing) != 0) {
		testing += 1;
		if (testing >= ROOT_DIR_CLUSTER + fat_fs->fat_length)
			return 0;
	}

	if (clst != 0) {
		fat_put (clst, testing);
	}
	fat_put (testing, EOChain);

	lock_acquire (&fat_fs->write_lock);
	fat_fs->last_clst = testing + 1;
	lock_release (&fat_fs->write_lock);

	return testing;
}

/* Remove the chain of clusters starting from CLST.
 * If PCLST is 0, assume CLST as the start of the chain. */
void
fat_remove_chain (cluster_t clst, cluster_t pclst) {
	/* TODO: Your code goes here. */
	/* clst 체인에서 pclst가 마지막 chain이 되도록 나머지는 지워줌 */
	/* clst 가 첫 chain이면 pclst는 마지막 체인이 되어야함 */
	if (pclst != 0) {
		ASSERT (fat_get (pclst) == clst);
		fat_put(pclst, EOChain);
	}

	cluster_t current_clst = clst;
	cluster_t next;
	while (current_clst != 0) {
		next = fat_get (current_clst);
		fat_put (current_clst, 0);
		if (current_clst < fat_fs->last_clst) {
			lock_acquire (&fat_fs->write_lock);
			fat_fs->last_clst = current_clst;
			lock_release (&fat_fs->write_lock);
		}
		if (next == EOChain) break;
		current_clst = next;
	}
}

/* Update a value in the FAT table. */
void
fat_put (cluster_t clst, cluster_t val) {
	/* TODO: Your code goes here. */
	/* clst를 val로 update. 바꿔줌. */
	*(fat_fs->fat + clst) = val;
}

/* Fetch a value in the FAT table. */
cluster_t
fat_get (cluster_t clst) {
	/* TODO: Your code goes here. */
	/* Return in which cluster number the given cluster clst points. 음..?*/
	return *(fat_fs->fat + clst);
}

/* Covert a cluster # to a sector number. */
disk_sector_t
cluster_to_sector (cluster_t clst) {
	/* TODO: Your code goes here. */
	/* Translates a cluster number clst into the corresponding sector number and return the sector number */
	return fat_fs->data_start + (clst -1) * fat_fs->bs.sectors_per_cluster;
}

cluster_t sector_to_cluster (disk_sector_t sector) {
	return ((sector - fat_fs->data_start) / fat_fs->bs.sectors_per_cluster) + 1;
}

/* Get next sector in clusters. */
disk_sector_t
next_sector (disk_sector_t sector) {
	disk_sector_t ofs = sector - fat_fs->data_start;
	unsigned int spc = fat_fs->bs.sectors_per_cluster;
	if (ofs % spc == (spc - 1)) {
		// Last sector in cluster
		cluster_t this_clst = (ofs / spc) + 1;
		cluster_t next_clst = fat_get (this_clst);
		if (next_clst == EOChain)
			return -1;
		else
			return cluster_to_sector (next_clst);
	}
	else {
		return (sector + 1);
	}
}