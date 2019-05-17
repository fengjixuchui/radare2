/* radare - LGPL - Copyright 2009-2019 - pancake, h4ng3r */

#include <r_types.h>
#include <r_util.h>
#include "dex.h"

char* r_bin_dex_get_version(RBinDexObj *bin) {
	if (bin) {
		ut8* version = calloc (1, 8);
		r_buf_read_at (bin->b, 4, version, 3);
		return (char *)version;
	}
	return NULL;
}

#define FAIL(x) { eprintf(x"\n"); goto fail; }
RBinDexObj *r_bin_dex_new_buf(RBuffer *buf) {
	if (!buf) {
		return NULL;
	}
	RBinDexObj *bin = R_NEW0 (RBinDexObj);
	int i;
	struct dex_header_t *dexhdr;
	if (!bin) {
		goto fail;
	}
	bin->size = r_buf_size (buf);
	bin->b = r_buf_ref (buf);
	/* header */
	if (bin->size < sizeof (struct dex_header_t)) {
		goto fail;
	}
	dexhdr = &bin->header;

	if (bin->size < 112) {
		goto fail;
	}

	r_buf_seek (bin->b, 0, R_BUF_SET);
	r_buf_read (bin->b, (ut8 *)&dexhdr->magic, 8);
	dexhdr->checksum = r_buf_read_le32 (bin->b);
	r_buf_read (bin->b, (ut8 *)&dexhdr->signature, 20);
	dexhdr->size = r_buf_read_le32 (bin->b);
	dexhdr->header_size = r_buf_read_le32 (bin->b);
	dexhdr->endian = r_buf_read_le32 (bin->b);
	// TODO: this offsets and size will be used for checking,
	// so they should be checked. Check overlap, < 0, > bin.size
	dexhdr->linksection_size = r_buf_read_le32 (bin->b);
	dexhdr->linksection_offset = r_buf_read_le32 (bin->b);
	dexhdr->map_offset = r_buf_read_le32 (bin->b);
	dexhdr->strings_size = r_buf_read_le32 (bin->b);
	dexhdr->strings_offset = r_buf_read_le32 (bin->b);
	dexhdr->types_size = r_buf_read_le32 (bin->b);
	dexhdr->types_offset = r_buf_read_le32 (bin->b);
	dexhdr->prototypes_size = r_buf_read_le32 (bin->b);
	dexhdr->prototypes_offset = r_buf_read_le32 (bin->b);
	dexhdr->fields_size = r_buf_read_le32 (bin->b);
	dexhdr->fields_offset = r_buf_read_le32 (bin->b);
	dexhdr->method_size = r_buf_read_le32 (bin->b);
	dexhdr->method_offset = r_buf_read_le32 (bin->b);
	dexhdr->class_size = r_buf_read_le32 (bin->b);
	dexhdr->class_offset = r_buf_read_le32 (bin->b);
	dexhdr->data_size = r_buf_read_le32 (bin->b);
	dexhdr->data_offset = r_buf_read_le32 (bin->b);

	/* strings */
	#define STRINGS_SIZE ((dexhdr->strings_size + 1) * sizeof (ut32))
	bin->strings = (ut32 *) calloc (dexhdr->strings_size + 1, sizeof (ut32));
	if (!bin->strings) {
		goto fail;
	}
	if (dexhdr->strings_size > bin->size) {
		free (bin->strings);
		goto fail;
	}
	for (i = 0; i < dexhdr->strings_size; i++) {
		ut64 offset = dexhdr->strings_offset + i * sizeof (ut32);
		if (offset + 4 > bin->size) {
			free (bin->strings);
			goto fail;
		}
		bin->strings[i] = r_buf_read_le32_at (bin->b, offset);
	}
	/* classes */
	// TODO: not sure about if that is needed
	int classes_size = dexhdr->class_size * DEX_CLASS_SIZE;
	if (dexhdr->class_offset + classes_size >= bin->size) {
		classes_size = bin->size - dexhdr->class_offset;
	}
	if (classes_size < 0) {
		classes_size = 0;
	}

	dexhdr->class_size = classes_size / DEX_CLASS_SIZE;
	bin->classes = (struct dex_class_t *) calloc (dexhdr->class_size, sizeof (struct dex_class_t));
	for (i = 0; i < dexhdr->class_size; i++) {
		ut64 offset = dexhdr->class_offset + i * DEX_CLASS_SIZE;
		if (offset + 32 > bin->size) {
			free (bin->strings);
			free (bin->classes);
			goto fail;
		}
		r_buf_seek (bin->b, offset, R_BUF_SET);
		bin->classes[i].class_id = r_buf_read_le32 (bin->b);
		bin->classes[i].access_flags = r_buf_read_le32 (bin->b);
		bin->classes[i].super_class = r_buf_read_le32 (bin->b);
		bin->classes[i].interfaces_offset = r_buf_read_le32 (bin->b);
		bin->classes[i].source_file = r_buf_read_le32 (bin->b);
		bin->classes[i].anotations_offset = r_buf_read_le32 (bin->b);
		bin->classes[i].class_data_offset = r_buf_read_le32 (bin->b);
		bin->classes[i].static_values_offset = r_buf_read_le32 (bin->b);
	}

	/* methods */
	int methods_size = dexhdr->method_size * sizeof (struct dex_method_t);
	if (dexhdr->method_offset + methods_size >= bin->size) {
		methods_size = bin->size - dexhdr->method_offset;
	}
	if (methods_size < 0) {
		methods_size = 0;
	}
	dexhdr->method_size = methods_size / sizeof (struct dex_method_t);
	bin->methods = (struct dex_method_t *) calloc (methods_size + 1, 1);
	for (i = 0; i < dexhdr->method_size; i++) {
		ut64 offset = dexhdr->method_offset + i * sizeof (struct dex_method_t);
		if (offset + 8 > bin->size) {
			free (bin->strings);
			free (bin->classes);
			free (bin->methods);
			goto fail;
		}
		r_buf_seek (bin->b, offset, R_BUF_SET);
		bin->methods[i].class_id = r_buf_read_le16 (bin->b);
		bin->methods[i].proto_id = r_buf_read_le16 (bin->b);
		bin->methods[i].name_id = r_buf_read_le32 (bin->b);
	}

	/* types */
	int types_size = dexhdr->types_size * sizeof (struct dex_type_t);
	if (dexhdr->types_offset + types_size >= bin->size) {
		types_size = bin->size - dexhdr->types_offset;
	}
	if (types_size < 0) {
		types_size = 0;
	}
	dexhdr->types_size = types_size / sizeof (struct dex_type_t);
	bin->types = (struct dex_type_t *) calloc (types_size + 1, 1);
	for (i = 0; i < dexhdr->types_size; i++) {
		ut64 offset = dexhdr->types_offset + i * sizeof (struct dex_type_t);
		if (offset + 4 > bin->size) {
			free (bin->strings);
			free (bin->classes);
			free (bin->methods);
			free (bin->types);
			goto fail;
		}
		bin->types[i].descriptor_id = r_buf_read_le32_at (bin->b, offset);
	}

	/* fields */
	int fields_size = dexhdr->fields_size * sizeof (struct dex_field_t);
	if (dexhdr->fields_offset + fields_size >= bin->size) {
		fields_size = bin->size - dexhdr->fields_offset;
	}
	if (fields_size < 0) {
		fields_size = 0;
	}
	dexhdr->fields_size = fields_size / sizeof (struct dex_field_t);
	bin->fields = (struct dex_field_t *) calloc (fields_size + 1, 1);
	for (i = 0; i < dexhdr->fields_size; i++) {
		ut64 offset = dexhdr->fields_offset + i * sizeof (struct dex_field_t);
		if (offset + 8 > bin->size) {
			free (bin->strings);
			free (bin->classes);
			free (bin->methods);
			free (bin->types);
			free (bin->fields);
			goto fail;
		}
		r_buf_seek (bin->b, offset, R_BUF_SET);
		bin->fields[i].class_id = r_buf_read_le16 (bin->b);
		bin->fields[i].type_id = r_buf_read_le16 (bin->b);
		bin->fields[i].name_id = r_buf_read_le32 (bin->b);
	}

	/* proto */
	int protos_size = dexhdr->prototypes_size * sizeof (struct dex_proto_t);
	if (dexhdr->prototypes_offset + protos_size >= bin->size) {
		protos_size = bin->size - dexhdr->prototypes_offset;
	}
	if (protos_size < 1) {
		dexhdr->prototypes_size = 0;
		return bin;
	}
	dexhdr->prototypes_size = protos_size / sizeof (struct dex_proto_t);
	bin->protos = (struct dex_proto_t *) calloc (protos_size, 1);
	for (i = 0; i < dexhdr->prototypes_size; i++) {
		ut64 offset = dexhdr->prototypes_offset + i * sizeof (struct dex_proto_t);
		if (offset + 12 > bin->size) {
			free (bin->strings);
			free (bin->classes);
			free (bin->methods);
			free (bin->types);
			free (bin->fields);
			free (bin->protos);
			goto fail;
		}
		r_buf_seek (bin->b, offset, R_BUF_SET);
		bin->protos[i].shorty_id = r_buf_read_le32 (bin->b);
		bin->protos[i].return_type_id = r_buf_read_le32 (bin->b);
		bin->protos[i].parameters_off = r_buf_read_le32 (bin->b);
	}

	return bin;

fail:
	if (bin) {
		r_buf_free (bin->b);
		free (bin);
	}
	return NULL;
}

// Move to r_util ??
int dex_read_uleb128(const ut8 *ptr, int size) {
	ut8 len = dex_uleb128_len (ptr, size);
	if (len > size) {
		return 0;
	}
	const ut8 *in = ptr + len - 1;
	ut32 result = 0;
	ut8 shift = 0;
	ut8 byte;

	while(shift < 29 && len > 0) {
		byte = *(in--);
		result |= (byte & 0x7f << shift);
		if (byte > 0x7f) {
			break;
		}
		shift += 7;
		len--;
	}
	return result;
}

#define LEB_MAX_SIZE 6
int dex_uleb128_len(const ut8 *ptr, int size) {
	int i = 1, result = *(ptr++);
	while (result > 0x7f && i <= LEB_MAX_SIZE && i < size) {
		result = *(ptr++);
		i++;
	}
	return i;
}

#define SIG_EXTEND(X,Y) X = ((X) << (Y)) >> Y
int dex_read_sleb128(const char *ptr, int size) {
	int cur, result;
	ut8 len = dex_uleb128_len ((const ut8*)ptr, size);
	if (len > size) {
		return 0;
	}
	ptr += len - 1;
	result = *(ptr--);

	if (result <= 0x7f) {
		SIG_EXTEND (result, 25);
	} else {
		cur = *(ptr--);
		result = (result & 0x7f) | ((cur & 0x7f) << 7);
		if (cur <= 0x7f) {
			SIG_EXTEND (result, 18);
		} else {
			cur = *(ptr--);
			result |= (cur & 0x7f) << 14;
			if (cur <= 0x7f) {
				SIG_EXTEND (result, 11);
			} else {
				cur = *(ptr--);
				result |= (cur & 0x7f) << 21;
				if (cur <= 0x7f) {
					SIG_EXTEND (result, 4);
				} else {
					cur = *(ptr--);
					result |= cur << 28;
				}
			}
		}
	}
	return result;
}
