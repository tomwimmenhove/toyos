#include "iso9660.h"
#include "debug.h"
#include "malloc.h"

iso9660_io_handle::iso9660_io_handle(std::shared_ptr<disk_block_io> device, uint32_t start, uint32_t size)
	: device(device), start(start), pos(0), size(size)
{ }

size_t iso9660_io_handle::read(void* buf, size_t len)
{
	if (pos + len > size)
		len = size - pos;

	device->read(buf, start + pos, len);
	pos += len;

	return len;
}

size_t iso9660_io_handle::write(void*, size_t)
{
	return -1;
}

bool iso9660_io_handle::close()
{
	return true;
}

/* ------------------------------------------------------------------------- */

iso9660::iso9660(std::shared_ptr<disk_block_io> device)
	: device(device)
{
	iso9660_dir_entry_cache root;

	std::vector<iso9660_path_table_entry*> pte_all;

	for (int i = 0; ; i++)
	{
		std::shared_ptr<iso9660_vol_desc> vol_desk = read_vold_desk(i);

		if (vol_desk->type == 0x01) /* Primary volume descriptor */
		{
			prim = std::make_shared<iso9660_vol_desc_primary>();
			/* Copy */
			*prim = *reinterpret_cast<iso9660_vol_desc_primary*>(vol_desk.get());
		}
		else if (vol_desk->type == 255)
			break;
	}

	if (!prim)
		panic("ISO9660: No primary volume descriptor found");
}

std::shared_ptr<io_handle> iso9660::open(const char* name)
{
	auto dp = find_de_path(&prim->root_dir, name);
	if (!dp)
		return nullptr;

	if (dp->file_flags & 0x02)
		return nullptr;

	/* XXX: GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36566 */
	return std::make_shared<iso9660_io_handle>(device, dp->extent_le * prim->blk_size_le, (uint32_t) dp->data_len_le);
}

bool iso9660::probe(std::shared_ptr<disk_block_io> device)
{
	auto vol_desk = read_vold_desk(device, 0);

	return vol_desk->version == 0x01 &&
		vol_desk->ident[0] == 'C' &&
		vol_desk->ident[1] == 'D' &&
		vol_desk->ident[2] == '0' &&
		vol_desk->ident[3] == '0' &&
		vol_desk->ident[4] == '1';
}

std::shared_ptr<iso9660_dir_entry> iso9660::find_de_path(iso9660_dir_entry* de, const char* name)
{
	std::unique_ptr<char, decltype(&mallocator::free)> sdup(strdup(name), &mallocator::free);
	std::shared_ptr<iso9660_dir_entry> dp;

	char* s = sdup.get();
	const char* se = s;

	while (*s)
	{
		if (*s == '/')
		{
			*s = 0;

			/* Find the current path entry within the current directory entry */
			dp = find_de(de, se);
			if (!dp)
				return nullptr;

			de = dp.get();

			/* New path entry starts at the next character */
			se = s + 1;
		}

		s++;
	}

	return find_de(de, se);
}

std::shared_ptr<iso9660_dir_entry> iso9660::find_de(iso9660_dir_entry* de, const char* name)
{
	auto extent_dat = std::make_unique<uint8_t[]>(de->data_len_le);
	device->read((void*) extent_dat.get(), de->extent_le * prim->blk_size_le, de->data_len_le);

	auto name_len = strlen(name);

	int idx = 0;
	for (uint32_t i = 0; i < de->data_len_le;)
	{
		iso9660_dir_entry* child = (iso9660_dir_entry*) (((uintptr_t) extent_dat.get()) + i);

		if (!child->len)
			break;

		if (test_name(child, idx, name, name_len))
		{
			auto r = std::make_shared<iso9660_dir_entry>();
			*r = *child;

			return r;
		}

		i += child->len;
		idx++;
	}

	return nullptr;
}

bool iso9660::test_name(iso9660_dir_entry* de, int idx, const char* name, size_t name_len)
{
	if (idx == 0 && strncmp(name, ".", name_len) == 0)
		return true;

	if (idx == 1 && strncmp(name, "..", name_len) == 0)
		return true;

	return name_len == de->file_id_len && memcmp(name, de->file_id, de->file_id_len) == 0;
}

std::shared_ptr<iso9660_vol_desc> iso9660::read_vold_desk(std::shared_ptr<disk_block_io> device, int n)
{
	auto vol_desk = std::make_shared<iso9660_vol_desc>();
	device->read((void*) vol_desk.get(), (0x10 + n) * 2048, sizeof(iso9660_vol_desc));

	return vol_desk;
}

