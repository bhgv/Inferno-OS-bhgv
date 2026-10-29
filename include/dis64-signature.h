/* Accepted versioned scalar callable signatures; calls/ref graphs remain separate. */
#ifndef DIS64_SIGNATURE_H
#define DIS64_SIGNATURE_H // Exact scalar callable-signature codec guard.

#include "dis64-scalar.h"

typedef struct Dis64Signature Dis64Signature;
struct Dis64Signature {
	const unsigned char *data; // Complete immutable version/result/count/kinds bytes.
	unsigned int bytes; // Includes BE u32 version1; exact12+4*parameters.
	unsigned int result; // Void or a supported scalar kind.
	unsigned int parameters; // Parameter count, all scalar and non-void.
};


/* Validate version and each declared scalar kind before publishing a signature. */
static int
dis64signaturecheck(const unsigned char *data, unsigned int bytes,
	unsigned int limit, Dis64Signature *result)
{
	Dis64Signature signature;
	unsigned int i;

	if(data == 0 || result == 0 || bytes < 12 || (unsigned int)-1 != 0xffffffffU ||
	   dis64u32(data) != 1)
		return 0;
	signature.result = dis64u32(data+4);
	signature.parameters = dis64u32(data+8);
	if((signature.result != Dis64Void && dis64scalarwidth(signature.result) == 0) ||
	   signature.parameters > limit || signature.parameters > (bytes-12)/4 ||
	   signature.parameters*4 != bytes-12)
		return 0;
	for(i=0; i<signature.parameters; i++)
		if(dis64scalarwidth(dis64u32(data+12+i*4)) == 0)
			return 0;
	signature.data = data;
	signature.bytes = bytes;
	*result = signature;
	return 1;
}


/* Compare validated canonical signatures exactly, without lossy interface hashes. */
static int
dis64signatureequal(const Dis64Signature *left, const Dis64Signature *right, int *equal)
{
	unsigned int i;

	if(left == 0 || right == 0 || equal == 0 || left->data == 0 || right->data == 0)
		return 0;
	if(left->bytes != right->bytes){
		*equal = 0;
		return 1;
	}
	for(i=0; i<left->bytes; i++)
		if(left->data[i] != right->data[i]){
			*equal = 0;
			return 1;
		}
	*equal = 1;
	return 1;
}


/* Emit the full versioned record only after complete kind/capacity preflight. */
static int
dis64signaturewrite(unsigned char *output, unsigned int capacity, unsigned int result,
	const unsigned int *parameters, unsigned int count, unsigned int *written)
{
	unsigned int i;

	if(output == 0 || written == 0 || capacity < 12 || (unsigned int)-1 != 0xffffffffU ||
	   (result != Dis64Void && dis64scalarwidth(result) == 0) ||
	   (count != 0 && parameters == 0) || count > (capacity-12)/4)
		return 0;
	for(i=0; i<count; i++)
		if(dis64scalarwidth(parameters[i]) == 0)
			return 0;
	dis64put32(output, 1);
	dis64put32(output+4, result);
	dis64put32(output+8, count);
	for(i=0; i<count; i++)
		dis64put32(output+12+i*4, parameters[i]);
	*written = 12+count*4;
	return 1;
}
#endif
