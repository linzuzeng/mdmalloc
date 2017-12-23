/************************************************************************
Copyright (c) 2002, Jean-Sebastien Roy (js@jeannot.org)
Zuzeng Lin, KTH, Nov 2017
*************************************************************************/
#include "stdafx.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#include "mdmalloc.h"

void *mdmalloc(size_t el_size, size_t n, size_t *d)
{
	size_t i, j, k, ptr_space, arr_space, align_space,
		ptr_count, arr_count, count, step, next;
	void *zone, **ptr;

	if (el_size == 0)
	{
		PERROR( "mdmalloc error : el_size == 0\n");
		return NULL;
	}

	/* Scalar case */
	if (n == 0) return calloc(el_size,1);

	/* Checks & Sizing*/
	for (arr_count = 1, ptr_count = 0, i = 0; i<n; i++)
	{
		if (d[i] == 0)
		{
			PERROR("mdmalloc error : dimension %lld == 0\n", i);
			return NULL;
		}
		ptr_count += arr_count;
		arr_count *= d[i];
	}

	/* Vector case */
	if (n == 1) return calloc(el_size*d[0],1);

	/* Multidimensionnal case */
	ptr_count--;

	/* Space & alignement */
	ptr_space = sizeof(void *)*ptr_count;
	arr_space = el_size*arr_count;
	if (ptr_space % el_size)
		align_space = el_size - (ptr_space % el_size);
	else
		align_space = 0;

	/* allocation */
	zone = NULL;
	
	while (zone == NULL) {
		zone = calloc(ptr_space + align_space + arr_space,1);
		//	fprintf(stderr, "malloc %d\n", ptr_space + align_space + arr_space);
		
		if (zone == NULL)
			PERROR("malloc failed\n");		
	}
	 /* ptr initialiation */
	ptr = (void **)zone;

	/* ptrs to ptrs */
	for (k = 0, count = 1, i = 0; i<(n - 2); i++)
	{
		count *= d[i];
		step = d[i + 1];
		next = k + count;
		for (j = 0; j<count; j++, k++)
			ptr[k] = &ptr[next + j*step];
	}

	/* dim n-1, ptrs to data */
	count *= d[n - 2];
	step = arr_space / count;
	for (j = 0; j<count; j++, k++)
		ptr[k] = (char *)zone + ptr_space + align_space + j*step;

	return zone;
}

void *mdfirst(void *array, size_t  n)
{
	size_t dim;

	for (dim = 1; dim < n; dim++)
		array = *((void **)array);

	return array;
}
size_t mdflatsize(size_t el_size, size_t  n, size_t *d) {
	size_t size = el_size;
	size_t dim;

	if (el_size == 0)
	{
		PERROR("mdflatsize error : el_size == 0\n")
	}

	for (dim = 0; dim < n; dim++)
	{
		if (d[dim] == 0)
		{
			PERROR("mdflatsize error : dimension %lld == 0\n",dim)
		}
		size *= d[dim];
	}
	return size;
}
void mdflatcopy(void *dest, void *src, size_t el_size, size_t n, size_t *d)
{
	memcpy(mdfirst(dest, n), mdfirst(src, n), mdflatsize(el_size, n, d));
}
void mdfscanf(FILE *src, char* Format_, void *dest, size_t el_size, size_t n, size_t *d) {
	void* ptr = mdfirst(dest, n);
	for (size_t i = 0; i < mdflatsize(el_size, n, d); i += el_size) {
		fscanf(src, Format_, ((void*)((size_t)ptr + i)));
	}
}

void mdfprintf(FILE *fp, char* Format_, void *dest, size_t el_size, size_t n, size_t *d) {
	size_t *marker_period;
	marker_period = (size_t*)malloc(sizeof(*marker_period)*n);
	if (marker_period == NULL) return; // malloc failed 
	for (size_t i = 0; i < n; i++)
		marker_period[i] = d[i];
	
	for (size_t i = n - 1; i > 0; i--) {
		marker_period[i-1] = marker_period[i-1] * marker_period[i];
	}
	void* ptr = mdfirst(dest, n);
	for (size_t i = 0; i < mdflatsize(el_size, n, d); i += el_size) {
		fprintf(fp, Format_, *((size_t*)((size_t)ptr + i)));
		for (size_t j = 0; j < n; j++)
			if ((i / el_size+1)%marker_period[j] == 0)
				fprintf(fp, "\n");
	}
}

void mddump(FILE *fp, char* Format_, void *dest, size_t el_size, size_t n, size_t *d) {
	fprintf(fp, "TENSOR %s%lld %lld ",Format_, el_size, n);
	for (int i=0;i<n;i++)
		fprintf(fp, "%lld ", d[i]);
	fprintf(fp, "\n");
	mdfprintf(fp, Format_, dest, el_size, n, d);
}


void* mdload(FILE *fp, size_t *el_size, size_t *n, size_t (*(*d)),bool loadcontent) {
	if (fp == NULL) {
		PERROR("mdload error: file pointer is null\n");
		return NULL;
	}
	if (el_size == NULL) {
		PERROR("mdload error: el_size is null\n");
		return NULL;
	}
	if (n == NULL) {
		PERROR("mdload error: n is null\n");
		return NULL;
	}
	if (d == NULL) {
		PERROR("mdload error: d is null\n");
		return NULL;
	}
	char buf[10];

	fscanf(fp, "%s%s%lld%lld", buf, buf, el_size, n);
	*d = (size_t*)malloc(sizeof(size_t*)*(*n));
	
	if (*d == NULL) {
		PERROR("mdload info : d==NULL")
		return NULL;
	}

	// read matrix dimensions
	for (size_t i = 0; i < *n; i++) {
		size_t temp;
		fscanf(fp, "%lld",&temp);
		(*d)[i] = temp;
	}
	
	size_t * d2 = (size_t*)malloc(sizeof(size_t)*(*n));
	memcpy(d2, *d, sizeof(size_t)*(*n));

	void* dest = mdmalloc(*el_size, *n, d2);
	if (loadcontent)
		mdfscanf(fp, buf, dest, *el_size, *n, d2);
	return dest;
}

///////////////VMD WRAPPERS/////////////////////////
#define __VMD_WRAP__ size_t i, *d;				\
va_list ap;										\
if (el_size == 0)								\
{												\
	PERROR("vmdcopy error : el_size == 0");	\
}												\
if (n > 0) {									\
	d = (size_t*)malloc(sizeof(*d)*n);			\
	if (d == NULL)                              \
		PERROR("vmdcopy error : d== NULL");\
	va_start(ap, n);							\
	for (i = 0; i<n; i++)						\
		d[i] = va_arg(ap, int);	/*fuck*/		\
	va_end(ap);									\
}												\
else d = NULL;


void vmddump(FILE *fp, char* Format_, void *dest, size_t el_size, size_t n, ...) {
	__VMD_WRAP__
	mddump(fp, Format_, dest, el_size, n, d);
	if (n > 0) free(d);
}
void vmdcopy(void *dest, void *src, size_t el_size, size_t n, ...)
{
	__VMD_WRAP__
	mdflatcopy(dest, src, el_size, n, d);
	if (n > 0) free(d);
}
void vmdfscanf(FILE *fp, char* Format_, void *dest, size_t el_size, size_t n, ...) {
	__VMD_WRAP__
	mdfscanf(fp, Format_, dest, el_size, n, d);
	if (n > 0) free(d);
}
void vmdfprintf(FILE *fp, char* Format_, void *dest, size_t el_size, size_t n, ...) {
	__VMD_WRAP__
	mdfprintf(fp, Format_,dest, el_size, n, d);
	if (n > 0) free(d);
}
void vmdfread(FILE *fp, void *dest, size_t el_size, size_t n, ...) {
	__VMD_WRAP__
	void* ptr = mdfirst(dest, n);
	fread(ptr, mdflatsize(1, n, d),1 ,fp);
	if (n > 0) free(d);
}
void vmdfwrite(FILE *fp, void *dest, size_t el_size, size_t n, ...) {
	__VMD_WRAP__
	void* ptr = mdfirst(dest, n);
	fwrite(ptr, mdflatsize(1, n, d), 1, fp);
	if (n > 0) free(d);
}
void *vmdmalloc(size_t el_size, size_t n, ...){
	__VMD_WRAP__
	void *zone = mdmalloc(el_size, n, d);
	if (n > 0) free(d);
	return zone;
}
size_t vmdsize(size_t el_size, size_t n, ...){
	__VMD_WRAP__
	size_t ret = mdflatsize(el_size, n, d);
	if (n > 0) free(d);
	return ret;
}

/////////////// END OF VMD WRAPPERS /////////////////


////////////////// TENSOR WRAPPERS //////////////////

void tensor_copy(Tensor *dest, Tensor *src)
{
	
#ifdef _DEBUG
	PINFO("TENSOR COPY WILL NOT FREE DEST");
#endif 
	(*dest).el_size = (*src).el_size;
	(*dest).n = (*src).n;
	//create dest.d
	size_t * d2 = (size_t*)malloc(sizeof(size_t)*((*src).n));
	memcpy(d2, (*src).d, sizeof(size_t)*((*src).n));
	(*dest).d = d2;
	//create dest.ptr
	(*dest).ptr=mdmalloc((*src).el_size, (*src).n, (*src).d);

	mdflatcopy((*dest).ptr, (*src).ptr, (*src).el_size, (*src).n, (*src).d);
}

/////////////// END OF TENSOR WRAPPERS///////////////