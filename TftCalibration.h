#pragma once

typedef struct Point
{
	int32_t x;
	int32_t y;
} tsPoint_t;

//Matrix
typedef struct
{
	int32_t An,
		Bn,
		Cn,
		Dn,
		En,
		Fn,
		Divider;
} tsMatrix_t;
