#ifndef ISO9660_H
#define ISO9660_H

#include <memory>
#include <stdint.h>
#include <stddef.h>
#include <vector>

#include "dev.h"
#include "disk.h"
#include "debug.h"

struct __attribute__((packed)) iso9660_dir_entry
{
	uint8_t len;			//0	1	Length of Directory Record.
	uint8_t ext_attr;		//1	1	Extended Attribute Record length.
	uint32_t extent_le;		//2	8	Location of extent (LBA) in both-endian format.
	uint32_t extent_be;
	uint32_t data_len_le;	//10	8	Data length (size of extent) in both-endian format.
	uint32_t data_len_be;
	uint8_t rec_date[7];	//18	7	Recording date and time (see format below).
	uint8_t file_flags;		//25	1	File flags (see below).
	uint8_t file_usize;		//26	1	File unit size for files recorded in interleaved mode, zero otherwise.
	uint8_t gap_size;		//27	1	Interleave gap size for files recorded in interleaved mode, zero otherwise.
	uint16_t vol_seq_le;	//28	4	Volume sequence number - the volume that this extent is recorded on, in 16 bit both-endian format.
	uint16_t vol_seq_be;
	uint8_t file_id_len;	//32	1	Length of file identifier (file name). This terminates with a ';' character followed by the file ID number in ASCII coded decimal ('1').
	char file_id[222];		//33	(variable)	File identifier.
};

struct __attribute__((packed)) i_iso9660_vol_desc { };

struct __attribute__((packed)) iso9660_vol_desc : public i_iso9660_vol_desc
{
	uint8_t type;
	char ident[5];
	uint8_t version;
	uint8_t data[2041];
};

struct __attribute__((packed)) iso9660_vol_desc_primary : public i_iso9660_vol_desc
{
	uint8_t type;
	char ident[5];
	uint8_t version;
	uint8_t unused0;			//7	1	Unused	-	Always 0x00.
	char sys_id[32];			//8	32	System Identifier	strA	The name of the system that can act upon sectors 0x00-0x0F for the volume.
	char vol_id[32];			//40	32	Volume Identifier	strD	Identification of this volume.
	uint8_t zeroes0[8];			//72	8	Unused Field	-	All zeroes.
	uint32_t vol_space_le;		//80	8	Volume Space Size	int32_LSB-MSB	Number of Logical Blocks in which the volume is recorded.
	uint32_t vol_space_be;		//
	uint8_t zeroes1[32];		//88	32	Unused Field	-	All zeroes.
	uint16_t set_size_le;		//120	4	Volume Set Size	int16_LSB-MSB	The size of the set in this logical volume (number of disks).
	uint16_t set_size_be;
	uint16_t seq_num_le;		//124	4	Volume Sequence Number	int16_LSB-MSB	The number of this disk in the Volume Set.
	uint16_t seq_num_be;
	uint16_t blk_size_le;		//128	4	Logical Block Size	int16_LSB-MSB	The size in bytes of a logical block. NB: This means that a logical block on a CD could be something other than 2 KiB!
	uint16_t blk_size_be;
	uint32_t path_tbl_size_le;	//132	8	Path Table Size	int32_LSB-MSB	The size in bytes of the path table.
	uint32_t path_tbl_size_be;
	uint32_t le_path_tbl;		//140	4	Location of Type-L Path Table	int32_LSB	LBA location of the path table. The path table pointed to contains only little-endian values.
	uint32_t le_path_tbl_opt;	//144	4	Location of the Optional Type-L Path Table	int32_LSB	LBA location of the optional path table. The path table pointed to contains only little-endian values. Zero means that no optional path table exists.
	uint32_t be_path_tbl;		//148	4	Location of Type-M Path Table	int32_MSB	LBA location of the path table. The path table pointed to contains only big-endian values.
	uint32_t be_path_tbl_opt;	//152	4	Location of Optional Type-M Path Table	int32_MSB	LBA location of the optional path table. The path table pointed to contains only big-endian values. Zero means that no optional path table exists.
	iso9660_dir_entry root_dir;	//156	34	Directory entry for the root directory	-	Note that this is not an LBA address, it is the actual Directory Record, which contains a single byte Directory Identifier (0x00), hence the fixed 34 byte size.
	char vol_set_id[128];		//190	128	Volume Set Identifier	strD	Identifier of the volume set of which this volume is a member.
	char pub_id[128];			// 318	128	Publisher Identifier	strA	The volume publisher. For extended publisher information, the first byte should be 0x5F, followed by the filename of a file in the root directory. If not specified, all bytes should be 0x20.
	char data_prep_id[128];		//446	128	Data Preparer Identifier	strA	The identifier of the person(s) who prepared the data for this volume. For extended preparation information, the first byte should be 0x5F, followed by the filename of a file in the root directory. If not specified, all bytes should be 0x20.
	char app_id[128];			//574	128	Application Identifier	strA	Identifies how the data are recorded on this volume. For extended information, the first byte should be 0x5F, followed by the filename of a file in the root directory. If not specified, all bytes should be 0x20.
	char copy_id[38];			//702	38	Copyright File Identifier	strD	Filename of a file in the root directory that contains copyright information for this volume set. If not specified, all bytes should be 0x20.
	char abs_file_is[36];		//740	36	Abstract File Identifier	strD	Filename of a file in the root directory that contains abstract information for this volume set. If not specified, all bytes should be 0x20.
	char bibl_file_id[37];		//776	37	Bibliographic File Identifier	strD	Filename of a file in the root directory that contains bibliographic information for this volume set. If not specified, all bytes should be 0x20.
	uint8_t t_creat[17];		//813	17	Volume Creation Date and Time	dec-datetime	The date and time of when the volume was created.
	uint8_t t_mod[17];			//830	17	Volume Modification Date and Time	dec-datetime	The date and time of when the volume was modified.
	uint8_t t_exp[17];			//847	17	Volume Expiration Date and Time	dec-datetime	The date and time after which this volume is considered to be obsolete. If not specified, then the volume is never considered to be obsolete.
	uint8_t t_eff[17];			//864	17	Volume Effective Date and Time	dec-datetime	The date and time after which the volume may be used. If not specified, the volume may be used immediately.
	uint8_t file_struct_ver;	//881	1	File Structure Version	int8	The directory records and path table version (always 0x01).
	uint8_t unused1;			//882	1	Unused	-	Always 0x00.
	uint8_t undef[512];			//883	512	Application Used	-	Contents not defined by ISO 9660.
	uint8_t reserved[653];		//1395	653	Reserved	-	Reserved by ISO.
};

struct __attribute__((packed)) iso9660_path_table_entry
{
	uint8_t len;			//0	1	Length of Directory Identifier
	uint8_t ext_attr_len;	//1	1	Extended Attribute Record Length
	uint32_t extent;		//2	4	Location of Extent (LBA). This is in a different format depending on whether this is the L-Table or M-Table (see explanation above).
	uint16_t parent;		//6	2	Directory number of parent directory (an index in to the path table). This is the field that limits the table to 65536 records.
	char name[0];			//8	(variable)	Directory Identifier (name) in d-characters.
};

struct iso9660_dir_entry_cache
{
	iso9660_path_table_entry* pte;
	std::vector<std::shared_ptr<iso9660_dir_entry_cache>> nodes;
};

struct iso9660;

struct iso9660_io_handle : public io_handle
{
	iso9660_io_handle(std::shared_ptr<disk_block_io> device, uint32_t start, uint32_t size);
	
	ssize_t read(void* buf, size_t len) override;
	ssize_t write(void*, size_t) override;
	size_t seek(size_t pos) override;
	inline size_t size() override { return fsize; }

	~iso9660_io_handle() override { con << "File was closed\n"; }

private:
	std::shared_ptr<disk_block_io> device;

	uint32_t start;
	uint32_t pos;
	uint32_t fsize;
};

struct iso9660
{
	iso9660(std::shared_ptr<disk_block_io> device);

	std::shared_ptr<io_handle> open(const char* name);

	static bool probe(std::shared_ptr<disk_block_io> device);

private:
	std::shared_ptr<iso9660_dir_entry> find_de_path(iso9660_dir_entry* de, const char* name);
	std::shared_ptr<iso9660_dir_entry> find_de(iso9660_dir_entry* de, const char* name);
	bool test_name(iso9660_dir_entry* de, int idx, const char* name, size_t name_len);
	static std::shared_ptr<iso9660_vol_desc> read_vold_desk(std::shared_ptr<disk_block_io> device, int n);
	inline std::shared_ptr<iso9660_vol_desc> read_vold_desk(int n) { return read_vold_desk(device, n); };

	std::shared_ptr<disk_block_io> device;
	std::shared_ptr<iso9660_vol_desc_primary> prim;
};

#endif /* ISO9660_H */
