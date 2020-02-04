
/*
 * varlena.c
 *   Type handler for the variable length data types.
 *
 * Copyright (c) 2008-2015 eSilo, LLC. All rights reserved.
 * This is free software; see the source for copying conditions.  There is
 * NO warranty; not even for MERCHANTABILITY or  FITNESS FOR A  PARTICULAR
 * PURPOSE.
 */

#include "libpqtypes-int.h"

/* put a type in its string representation, text format */
int
pqt_put_str(PGtypeArgs *args)
{
	args->format = TEXTFMT;
	args->put.out = va_arg(args->ap, char *);
	return args->put.out ? (int)strlen(args->put.out)+1 : 0;
}

/* varchar, bpchar and name use the text handlers */
int
pqt_put_text(PGtypeArgs *args)
{
	args->put.out = va_arg(args->ap, PGtext);
	return args->put.out ? (int)strlen(args->put.out) : 0;
}

/* varchar, bpchar and name use the text handlers */
int
pqt_get_text(PGtypeArgs *args)
{
	DECLVALUE(args);
	PGtext *textp = va_arg(args->ap, PGtext *);
	CHKGETVALS(args, textp);
	*textp = value;
	return 0;
}

int
pqt_put_bytea(PGtypeArgs *args)
{
	PGbytea *bytea = va_arg(args->ap, PGbytea *);
	PUTNULLCHK(args, bytea);
	args->put.out = bytea->data;
	return bytea->len;
}

int
pqt_get_bytea(PGtypeArgs *args)
{
	DECLVALUE(args);
	DECLLENGTH(args);
	PGbytea *bytea = va_arg(args->ap, PGbytea *);

	CHKGETVALS(args, bytea);

	if (args->format == TEXTFMT)
	{
		size_t len = 0;

		value = (char *) PQunescapeBytea((const unsigned char *) value, &len);
		if (!value)
			RERR_STR2INT(args);

		bytea->data = (char *) PQresultAlloc(args->get.result, len);
		if (!bytea->data)
		{
			PQfreemem(value);
			RERR_MEM(args);
		}

		bytea->len = (int) len;
		memcpy(bytea->data, value, len);
		PQfreemem(value);
		return 0;
  }

	/* binary format */
  bytea->len = valuel;
  bytea->data = value;
	return 0;
}

/* Bit string types */

int
pqt_put_varbit(PGtypeArgs *args)
{
	int len_bytes;
	int total_bytes;
	char *out;
	PGvarbit *varbit = va_arg(args->ap, PGvarbit *);
	PUTNULLCHK(args, varbit);
	len_bytes = varbit->len_bytes;
	total_bytes = len_bytes + (int)sizeof(int32_t);
	if (args->put.expandBuffer(args, total_bytes) == -1)
		RERR_MEM(args);
	out = args->put.out;
	pqt_buf_putint4(out, varbit->len_bits);
	memcpy(out + sizeof(int32_t), varbit->data, (size_t)len_bytes);
	return total_bytes;
}

int
pqt_get_varbit(PGtypeArgs *args)
{
	DECLVALUE(args);
	DECLLENGTH(args);
	PGvarbit *varbit = va_arg(args->ap, PGvarbit *);
	CHKGETVALS(args, varbit);
	if (args->format == TEXTFMT)
	{
		unsigned char *cur_byte;
		char *cur_char;
		unsigned char mask_byte;
		uint32_t len_bits = (uint32_t)strlen(value);
		int len_bytes = (len_bits >> 3) + !!(len_bits & 07);
		char *data = (char *)PQresultAlloc(args->get.result, (size_t)len_bytes);
		if (!data)
			RERR_MEM(args);
		cur_byte = (unsigned char *)data;
		*cur_byte = 0;
		mask_byte = 0x80;
		for (cur_char = value; *cur_char; ++cur_char)
		{
			if (*cur_char == '1')
				*cur_byte |= mask_byte;
			else if (*cur_char != '0')
			{
				PQfreemem(data);
				RERR(args, "String to bit string conversion failed");
			}
			mask_byte >>= 1;
			if (mask_byte == 0)
			{
				++cur_byte;
				*cur_byte = 0;
				mask_byte = 0x80;
			}
		}
		varbit->len_bytes = len_bytes;
		varbit->len_bits = (int32_t)len_bits;
		varbit->data = data;
		return 0;
	}
	/* binary format */
	varbit->len_bytes = valuel - (int)sizeof(int32_t);
	varbit->len_bits = pqt_buf_getint4(value);
	varbit->data = value + sizeof(int32_t);
	return 0;
}

int
pqt_put_bit(PGtypeArgs *args)
{
	return pqt_put_varbit(args);
}

int
pqt_get_bit(PGtypeArgs *args)
{
	return pqt_get_varbit(args);
}
